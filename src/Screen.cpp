/*
   SPDX-FileCopyrightText: 2007-2008 Robert Knight <robert.knight@gmail.com>
   SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

   SPDX-License-Identifier: GPL-2.0-or-later
   */

// Own
#include "Screen.h"

#include "config-konsole.h"

// Qt
#include <QTextStream>

// Konsole decoders
#include <HTMLDecoder.h>
#include <PlainTextDecoder.h>

#include "session/Session.h"
#include "session/SessionController.h"
#include "terminalDisplay/TerminalDisplay.h"
#include "terminalDisplay/TerminalFonts.h"

#include "EscapeSequenceUrlExtractor.h"
#include "characters/ExtendedCharTable.h"
#include "history/HistoryScrollNone.h"
#include "history/HistoryType.h"
#include "profile/Profile.h"

#ifdef HAVE_MALLOC_H
    // For malloc_trim, which is a GNU extension
    #ifdef __GNUC__
    extern "C" {
            #include <malloc.h>
    }
    #endif
#endif

using namespace Konsole;

// Macro to convert x,y position on screen to position within an image.
//
// Originally the image was stored as one large contiguous block of
// memory, so a position within the image could be represented as an
// offset from the beginning of the block.  For efficiency reasons this
// is no longer the case.
// Many internal parts of this class still use this representation for parameters and so on,
// notably moveImage() and clearImage().
// This macro converts from an X,Y position into an image offset.
#ifndef loc
#define loc(X, Y) ((Y)*_columns + (X))
#endif

const Character Screen::DefaultChar =
    Character(' ', CharacterColor(COLOR_SPACE_DEFAULT, DEFAULT_FORE_COLOR), CharacterColor(COLOR_SPACE_DEFAULT, DEFAULT_BACK_COLOR), DEFAULT_RENDITION, false);

Screen::Screen(int lines, int columns)
    : _currentTerminalDisplay(nullptr)
    , _lines(lines)
    , _columns(columns)
    , _screenLines(_lines + 1)
    , _screenLinesSize(_lines)
    , _scrolledLines(0)
    , _lastScrolledRegion(QRect())
    , _droppedLines(0)
    , _oldTotalLines(0)
    , _isResize(false)
    , _enableReflowLines(false)
    , _lineProperties(_lines + 1)
    , _history(std::make_unique<HistoryScrollNone>())
    , _cuX(0)
    , _cuY(0)
    , _currentForeground(CharacterColor())
    , _currentBackground(CharacterColor())
    , _currentRendition(DEFAULT_RENDITION)
    , _topMargin(0)
    , _bottomMargin(0)
    , _tabStops(QBitArray())
    , _selBegin(0)
    , _selTopLeft(0)
    , _selBottomRight(0)
    , _blockSelectionMode(false)
    , _effectiveForeground(CharacterColor())
    , _effectiveBackground(CharacterColor())
    , _effectiveRendition(DEFAULT_RENDITION)
    , _lastPos(-1)
    , _lastDrawnChar(0)
    , _escapeSequenceUrlExtractor(std::make_unique<EscapeSequenceUrlExtractor>())
{
    _escapeSequenceUrlExtractor->setScreen(this);
    std::fill(_lineProperties.begin(), _lineProperties.end(), LINE_DEFAULT);

    _graphicsPlacements = std::vector<std::unique_ptr<TerminalGraphicsPlacement_t>>();
    _hasGraphics = false;

    initTabStops();
    clearSelection();
    reset();
}

Screen::~Screen() = default;

void Screen::cursorUp(int n)
//=CUU
{
    if (n < 1) {
        n = 1; // Default
    }
    const int stop = _cuY < _topMargin ? 0 : _topMargin;
    _cuX = qMin(getScreenLineColumns(_cuY) - 1, _cuX); // nowrap!
    _cuY = qMax(stop, _cuY - n);
}

void Screen::cursorDown(int n)
//=CUD
{
    if (n < 1) {
        n = 1; // Default
    }
    if (n > MAX_SCREEN_ARGUMENT) {
        n = MAX_SCREEN_ARGUMENT;
    }
    const int stop = _cuY > _bottomMargin ? _lines - 1 : _bottomMargin;
    _cuX = qMin(getScreenLineColumns(_cuY) - 1, _cuX); // nowrap!
    _cuY = qMin(stop, _cuY + n);
}

void Screen::cursorLeft(int n)
//=CUB
{
    if (n < 1) {
        n = 1; // Default
    }
    _cuX = qMin(getScreenLineColumns(_cuY) - 1, _cuX); // nowrap!
    _cuX = qMax(0, _cuX - n);
}

void Screen::cursorNextLine(int n)
//=CNL
{
    if (n < 1) {
        n = 1; // Default
    }
    if (n > MAX_SCREEN_ARGUMENT) {
        n = MAX_SCREEN_ARGUMENT;
    }
    _cuX = 0;
    const int stop = _cuY > _bottomMargin ? _lines - 1 : _bottomMargin;
    _cuY = qMin(stop, _cuY + n);
}

void Screen::cursorPreviousLine(int n)
//=CPL
{
    if (n < 1) {
        n = 1; // Default
    }
    _cuX = 0;
    const int stop = _cuY < _topMargin ? 0 : _topMargin;
    _cuY = qMax(stop, _cuY - n);
}

void Screen::cursorRight(int n)
//=CUF
{
    if (n < 1) {
        n = 1; // Default
    }
    if (n > MAX_SCREEN_ARGUMENT) {
        n = MAX_SCREEN_ARGUMENT;
    }
    _cuX = qMin(getScreenLineColumns(_cuY) - 1, _cuX + n);
}

void Screen::setMargins(int top, int bot)
//=STBM
{
    if (top < 1) {
        top = 1; // Default
    }
    if (bot < 1) {
        bot = _lines; // Default
    }
    top = top - 1; // Adjust to internal lineno
    bot = bot - 1; // Adjust to internal lineno
    if (!(0 <= top && top < bot && bot < _lines)) {
        // Debug()<<" setRegion("<<top<<","<<bot<<") : bad range.";
        return; // Default error action: ignore
    }
    _topMargin = top;
    _bottomMargin = bot;
    _cuX = 0;
    _cuY = getMode(MODE_Origin) ? top : 0;
}

int Screen::topMargin() const
{
    return _topMargin;
}
int Screen::bottomMargin() const
{
    return _bottomMargin;
}

void Screen::index()
//=IND
{
    if (_cuY == _bottomMargin) {
        scrollUp(1);
    } else if (_cuY < _lines - 1) {
        _cuY += 1;
    }
}

void Screen::reverseIndex()
//=RI
{
    if (_cuY == _topMargin) {
        scrollDown(_topMargin, 1);
    } else if (_cuY > 0) {
        _cuY -= 1;
    }
}

void Screen::nextLine()
//=NEL
{
    toStartOfLine();
    index();
}

void Screen::eraseChars(int n)
{
    if (n < 1) {
        n = 1; // Default
    }
    if (n > MAX_SCREEN_ARGUMENT) {
        n = MAX_SCREEN_ARGUMENT;
    }
    const int p = qBound(0, _cuX + n - 1, _columns - 1);
    clearImage(loc(_cuX, _cuY), loc(p, _cuY), ' ', false);
}

void Screen::eraseBlock(int y, int x, int height, int width)
{
    width = qBound(0, width, _columns - x - 1);
    height = qBound(0, height, _lines - y - 1);
    for (int row = y; row < y + height; row++) {
        clearImage(loc(x, row), loc(x + width - 1, row), ' ', false);
    }
}

void Screen::deleteChars(int n)
{
    Q_ASSERT(n >= 0);

    // always delete at least one char
    if (n < 1) {
        n = 1;
    }

    // if cursor is beyond the end of the line there is nothing to do
    if (_cuX >= _screenLines.at(_cuY).count()) {
        return;
    }

    if (_cuX + n > _screenLines.at(_cuY).count()) {
        n = _screenLines.at(_cuY).count() - _cuX;
    }

    Q_ASSERT(n >= 0);
    Q_ASSERT(_cuX + n <= _screenLines.at(_cuY).count());

    _screenLines[_cuY].remove(_cuX, n);

    // Append space(s) with current attributes
    Character spaceWithCurrentAttrs(' ', _effectiveForeground, _effectiveBackground, _effectiveRendition, false);

    for (int i = 0; i < n; ++i) {
        _screenLines[_cuY].append(spaceWithCurrentAttrs);
    }
}

void Screen::insertChars(int n)
{
    if (n < 1) {
        n = 1; // Default
    }

    if (_screenLines.at(_cuY).size() < _cuX) {
        _screenLines[_cuY].resize(_cuX);
    }

    _screenLines[_cuY].insert(_cuX, n, Character(' '));

    if (_screenLines.at(_cuY).count() > getScreenLineColumns(_cuY)) {
        _screenLines[_cuY].resize(getScreenLineColumns(_cuY));
    }
}

void Screen::repeatChars(int n)
{
    if (n < 1) {
        n = 1; // Default
    }

    // From ECMA-48 version 5, section 8.3.103:
    // "If the character preceding REP is a control function or part of a
    // control function, the effect of REP is not defined by this Standard."
    //
    // So, a "normal" program should always use REP immediately after a visible
    // character (those other than escape sequences). So, _lastDrawnChar can be
    // safely used.
    while (n > 0) {
        displayCharacter(_lastDrawnChar);
        --n;
    }
}

void Screen::deleteLines(int n)
{
    if (_cuY < _topMargin) {
        return;
    }
    if (n < 1) {
        n = 1; // Default
    }
    scrollUp(_cuY, n);
}

void Screen::insertLines(int n)
{
    if (_cuY < _topMargin) {
        return;
    }
    if (n < 1) {
        n = 1; // Default
    }
    scrollDown(_cuY, n);
}

void Screen::setMode(int m)
{
    _currentModes[m] = 1;
    switch (m) {
    case MODE_Origin:
        _cuX = 0;
        _cuY = _topMargin;
        break; // FIXME: home
    }
}

void Screen::resetMode(int m)
{
    _currentModes[m] = 0;
    switch (m) {
    case MODE_Origin:
        _cuX = 0;
        _cuY = 0;
        break; // FIXME: home
    }
}

void Screen::saveMode(int m)
{
    _savedModes[m] = _currentModes[m];
}

void Screen::restoreMode(int m)
{
    _currentModes[m] = _savedModes[m];
}

bool Screen::getMode(int m) const
{
    return _currentModes[m] != 0;
}

void Screen::saveCursor()
{
    _savedState.cursorColumn = _cuX;
    _savedState.cursorLine = _cuY;
    _savedState.rendition = _currentRendition;
    _savedState.foreground = _currentForeground;
    _savedState.background = _currentBackground;
    _savedState.originMode = _currentModes[MODE_Origin];
}

void Screen::restoreCursor()
{
    _cuY = qMin(_savedState.cursorLine, _lines - 1);
    _cuX = qMin(_savedState.cursorColumn, getScreenLineColumns(_cuY) - 1);
    _currentRendition = _savedState.rendition;
    _currentForeground = _savedState.foreground;
    _currentBackground = _savedState.background;
    updateEffectiveRendition();
    _currentModes[MODE_Origin] = _savedState.originMode;
    /* XXX: DEC STD-070 states that DECRC should make sure the cursor lies
     * inside the scrolling region, but that behavior doesn't seem to be
     * widespread (neither VT1xx, VT240, mlterm, vte do it, and xterm
     * only limits the bottom margin).
    if (getMode(MODE_Origin)) {
        _cuY = qBound(_topMargin, _cuY, _bottomMargin);
    }
    */
}

int Screen::getOldTotalLines()
{
    return _oldTotalLines;
}

bool Screen::isResize()
{
    if (_isResize) {
        _isResize = false;
        return true;
    }
    return _isResize;
}

void Screen::setReflowLines(bool enable)
{
    _enableReflowLines = enable;
}

/* Note that if you use these debugging functions, it will
   fail to compile on gcc 8.3.1 as of Feb 2021 due to for_each_n().
   See BKO: 432639

// Debugging auxiliary functions to show what is written in screen or history
void toDebug(const Character *s, int count, bool wrapped = false)
{
    QString out;
    std::for_each_n(s, count, [&out](const Character &i) { out += i.character; });
    if (wrapped) {
        qDebug() << out << "*wrapped*";
    } else {
        qDebug() << out;
    }
}
void toDebug(const QVector<Character> &s, bool wrapped = false)
{
    toDebug(s.data(), s.size(), wrapped);
}
*/

int Screen::getCursorLine()
{
    if (isAppMode()) {
        return _savedState.cursorLine;
    }
    return _cuY;
}

void Screen::setCursorLine(int newLine)
{
    if (isAppMode()) {
        _savedState.cursorLine = newLine;
        _cuY = qBound(0, _cuY, _lines - 1);
    } else {
        _cuY = newLine;
    }
}

void Screen::resizeImage(int new_lines, int new_columns)
{
    if ((new_lines == _lines) && (new_columns == _columns)) {
        return;
    }

    // Adjust scroll position, and fix glitches
    _oldTotalLines = getLines() + getHistLines();
    _isResize = true;

    int cursorLine = getCursorLine();
    const int oldCursorLine = (cursorLine == _lines - 1 || cursorLine > new_lines - 1) ? new_lines - 1 : cursorLine;

    // Check if _history need to change
    if (_enableReflowLines && new_columns != _columns && _history->getLines() && _history->getMaxLines()) {
        // Join next line from _screenLine to _history
        while (!_screenLines.empty() && _history->isWrappedLine(_history->getLines() - 1)) {
            fastAddHistLine();
            --cursorLine;
        }
        auto removedLines = _history->reflowLines(new_columns);

        // If _history size > max history size it will drop a line from _history.
        // We need to verify if we need to remove a URL.
        if (removedLines) {
            _escapeSequenceUrlExtractor->historyLinesRemoved(removedLines);
        }
    }

    if (_enableReflowLines && new_columns != _columns) {
        int cursorLineCorrection = 0;
        if (_currentTerminalDisplay) {
            // The 'zsh' works different from other shells when writing the command line.
            // It needs to identify the 'zsh' and calculate the new command line.
            auto sessionController = _currentTerminalDisplay->sessionController();
            auto terminal = sessionController->session()->foregroundProcessName();
            if (terminal == QLatin1String("zsh")) {
                while (cursorLine + cursorLineCorrection > 0 && (_lineProperties.at(cursorLine + cursorLineCorrection) & LINE_PROMPT_START) == 0) {
                    --cursorLineCorrection;
                }
                if (cursorLine + cursorLineCorrection > 0 && (_lineProperties.at(cursorLine + cursorLineCorrection) & LINE_PROMPT_START) != 0) {
                    _lineProperties[cursorLine + cursorLineCorrection - 1] &= ~LINE_WRAPPED;
                } else {
                    cursorLineCorrection = 0;
                    while (cursorLine + cursorLineCorrection > 0 && (_lineProperties.at(cursorLine + cursorLineCorrection - 1) & LINE_WRAPPED) != 0) {
                        --cursorLineCorrection;
                    }
                }
            }
        }

        // Analyze the lines and move the data to lines below.
        int currentPos = 0;
        while (currentPos < (cursorLine + cursorLineCorrection) && currentPos < (int)_screenLines.size() - 1) {
            // Join wrapped line in current position
            if ((_lineProperties.at(currentPos) & LINE_WRAPPED) != 0) {
                _screenLines[currentPos].append(_screenLines.at(currentPos + 1));
                _screenLines.erase(_screenLines.begin() + currentPos + 1);
                _lineProperties.erase(_lineProperties.begin() + currentPos);
                --cursorLine;
                continue;
            }

            // Ignore whitespaces at the end of the line
            int lineSize = _screenLines.at(currentPos).size();
            while (lineSize > 0 && QChar(_screenLines.at(currentPos).at(lineSize - 1).character).isSpace()) {
                --lineSize;
            }

            // If need to move to line below, copy from the current line, to the next one.
            if (lineSize > new_columns && !(_lineProperties.at(currentPos) & (LINE_DOUBLEHEIGHT_BOTTOM | LINE_DOUBLEHEIGHT_TOP))) {
                auto values = _screenLines.at(currentPos).mid(new_columns);
                _screenLines[currentPos].resize(new_columns);
                _lineProperties.insert(_lineProperties.begin() + currentPos + 1, _lineProperties.at(currentPos));
                _screenLines.insert(_screenLines.begin() + currentPos + 1, std::move(values));
                _lineProperties[currentPos] |= LINE_WRAPPED;
                ++cursorLine;
            }
            currentPos += 1;
        }
    }

    // Check if it need to move from _screenLine to _history
    while (cursorLine > new_lines - 1) {
        fastAddHistLine();
        --cursorLine;
    }

    if (_enableReflowLines) {
        // Check cursor position and send from _history to _screenLines
        ImageLine histLine;
        while (cursorLine < oldCursorLine && _history->getLines()) {
            int histPos = _history->getLines() - 1;
            int histLineLen = _history->getLineLen(histPos);
            LineProperty lineProperty = _history->getLineProperty(histPos);
            histLine.resize(histLineLen);
            _history->getCells(histPos, 0, histLineLen, histLine.data());
            _screenLines.insert(_screenLines.begin(), std::move(histLine));
            _lineProperties.insert(_lineProperties.begin(), lineProperty);
            _history->removeCells();
            ++cursorLine;
        }
    }

    _lineProperties.resize(new_lines + 1);
    if (_lineProperties.size() > _screenLines.size()) {
        std::fill(_lineProperties.begin() + _screenLines.size(), _lineProperties.end(), LINE_DEFAULT);
    }
    _screenLines.resize(new_lines + 1);

    _screenLinesSize = new_lines;
    _lines = new_lines;
    _columns = new_columns;
    _cuX = qMin(_cuX, _columns - 1);
    cursorLine = qBound(0, cursorLine, _lines - 1);
    setCursorLine(cursorLine);

    // FIXME: try to keep values, evtl.
    setDefaultMargins();
    initTabStops();
    clearSelection();
}

void Screen::setDefaultMargins()
{
    _topMargin = 0;
    _bottomMargin = _lines - 1;
}

/*
   Clarifying rendition here and in the display.

   The rendition attributes are

   attribute
   --------------
   RE_UNDERLINE
   RE_BLINK
   RE_BOLD
   RE_REVERSE
   RE_TRANSPARENT
   RE_FAINT
   RE_STRIKEOUT
   RE_CONCEAL
   RE_OVERLINE

   Depending on settings, bold may be rendered as a heavier font
   in addition to a different color.
   */

void Screen::reverseRendition(Character &p) const
{
    CharacterColor f = p.foregroundColor;
    CharacterColor b = p.backgroundColor;

    p.foregroundColor = b;
    p.backgroundColor = f; // p->r &= ~RE_TRANSPARENT;
}

void Screen::updateEffectiveRendition()
{
    _effectiveRendition = _currentRendition;
    if ((_currentRendition & RE_REVERSE) != 0) {
        _effectiveForeground = _currentBackground;
        _effectiveBackground = _currentForeground;
    } else {
        _effectiveForeground = _currentForeground;
        _effectiveBackground = _currentBackground;
    }

    if ((_currentRendition & RE_BOLD) != 0) {
        if ((_currentRendition & RE_FAINT) == 0) {
            _effectiveForeground.setIntensive();
        }
    } else {
        if ((_currentRendition & RE_FAINT) != 0) {
            _effectiveForeground.setFaint();
        }
    }
}

void Screen::copyFromHistory(Character *dest, int startLine, int count) const
{
    const int columns = _columns;

    Q_ASSERT(startLine >= 0 && count > 0 && startLine + count <= _history->getLines());

    for (int line = startLine; line < startLine + count; ++line) {
        const int length = qMin(columns, _history->getLineLen(line));
        const int destLineOffset = (line - startLine) * columns;
        const int lastColumn = (_history->getLineProperty(line) & LINE_DOUBLEWIDTH) ? columns / 2 : columns;

        _history->getCells(line, 0, length, dest + destLineOffset);

        if (length < columns) {
            const int begin = destLineOffset + length;
            const int end = destLineOffset + columns;
            std::fill(dest + begin, dest + end, Screen::DefaultChar);
        }

        // invert selected text
        if (_selBegin != -1) {
            for (int column = 0; column < lastColumn; ++column) {
                if (isSelected(column, line)) {
                    dest[destLineOffset + column].rendition |= RE_SELECTED;
                }
            }
        }
    }
}

void Screen::copyFromScreen(Character *dest, int startLine, int count) const
{
    const int endLine = startLine + count;
    const int columns = _columns;
    const int historyLines = _history->getLines();

    Q_ASSERT(startLine >= 0 && count > 0 && endLine <= _lines);

    for (int line = startLine; line < endLine; ++line) {
        const int destLineOffset = (line - startLine) * columns;
        const int lastColumn = (line < (int)_lineProperties.size() && _lineProperties[line] & LINE_DOUBLEWIDTH) ? columns / 2 : columns;
        const ImageLine srcLine = _screenLines.at(line);
        const int length = qMin(columns, srcLine.size());

        std::copy(srcLine.cbegin(), srcLine.cbegin() + length, dest + destLineOffset);

        if (length < columns) {
            const int begin = destLineOffset + length;
            const int end = destLineOffset + columns;
            std::fill(dest + begin, dest + end, Screen::DefaultChar);
        }

        if (_selBegin != -1) {
            for (int column = 0; column < lastColumn; ++column) {
                if (isSelected(column, line + historyLines)) {
                    dest[destLineOffset + column].rendition |= RE_SELECTED;
                }
            }
        }
    }
}

void Screen::getImage(Character *dest, int size, int startLine, int endLine) const
{
    Q_ASSERT(startLine >= 0);
    Q_ASSERT(endLine >= startLine && endLine < _history->getLines() + _lines);

    const int mergedLines = endLine - startLine + 1;

    Q_ASSERT(size >= mergedLines * _columns);
    Q_UNUSED(size)

    const int linesInHistoryBuffer = qBound(0, _history->getLines() - startLine, mergedLines);
    const int linesInScreenBuffer = mergedLines - linesInHistoryBuffer;

    // copy _lines from history buffer
    if (linesInHistoryBuffer > 0) {
        copyFromHistory(dest, startLine, linesInHistoryBuffer);
    }

    // copy _lines from screen buffer
    if (linesInScreenBuffer > 0) {
        copyFromScreen(dest + linesInHistoryBuffer * _columns, startLine + linesInHistoryBuffer - _history->getLines(), linesInScreenBuffer);
    }

    // invert display when in screen mode
    if (getMode(MODE_Screen)) {
        for (int i = 0; i < mergedLines * _columns; ++i) {
            reverseRendition(dest[i]); // for reverse display
        }
    }

    int visX = qMin(_cuX, getScreenLineColumns(_cuY) - 1);
    // mark the character at the current cursor position
    int cursorIndex = loc(visX, _cuY + linesInHistoryBuffer);
    if (getMode(MODE_Cursor) && cursorIndex < _columns * mergedLines) {
        dest[cursorIndex].rendition |= RE_CURSOR;
    }
}

QVector<LineProperty> Screen::getLineProperties(int startLine, int endLine) const
{
    Q_ASSERT(startLine >= 0);
    Q_ASSERT(endLine >= startLine && endLine < _history->getLines() + _lines);

    const int mergedLines = endLine - startLine + 1;
    const int linesInHistory = qBound(0, _history->getLines() - startLine, mergedLines);
    const int linesInScreen = mergedLines - linesInHistory;

    QVector<LineProperty> result(mergedLines);
    int index = 0;

    // copy properties for _lines in history
    for (int line = startLine; line < startLine + linesInHistory; ++line) {
        result[index] = _history->getLineProperty(line);
        ++index;
    }

    // copy properties for _lines in screen buffer
    const int firstScreenLine = startLine + linesInHistory - _history->getLines();
    for (int line = firstScreenLine; line < firstScreenLine + linesInScreen; ++line) {
        result[index] = _lineProperties.at(line);
        ++index;
    }

    return result;
}

int Screen::getScreenLineColumns(const int line) const
{
    if (line < (int)_lineProperties.size() && _lineProperties.at(line) & LINE_DOUBLEWIDTH) {
        return _columns / 2;
    }

    return _columns;
}

void Screen::reset(bool softReset)
{
    // Clear screen, but preserve the current line
    if (!softReset) {
        scrollUp(0, _cuY);
        _cuY = 0;
        _cuX = 0;

        resetMode(MODE_Screen); // screen not inverse
        resetMode(MODE_NewLine);

        initTabStops();

        if (_hasGraphics) {
            delPlacements();
            _currentTerminalDisplay->update();
        }
    }

    _currentModes[MODE_Origin] = 0;
    _savedModes[MODE_Origin] = 0;

    setMode(MODE_Wrap);
    saveMode(MODE_Wrap); // wrap at end of margin

    resetMode(MODE_Insert);
    saveMode(MODE_Insert); // overstroke

    setMode(MODE_Cursor); // cursor visible

    _topMargin = 0;
    _bottomMargin = _lines - 1;

    // Other terminal emulators reset the entire scroll history during a reset
    //    setScroll(getScroll(), false);

    setDefaultRendition();
    saveCursor();

    // DECSTR homes the saved cursor even though it doesn't home the current cursor
    _savedState.cursorColumn = 0;
    _savedState.cursorLine = 0;
}

void Screen::backspace()
{
    _cuX = qMin(getScreenLineColumns(_cuY) - 1, _cuX); // nowrap!
    _cuX = qMax(0, _cuX - 1);

    if (_screenLines.at(_cuY).size() < _cuX + 1) {
        _screenLines[_cuY].resize(_cuX + 1);
    }
}

void Screen::tab(int n)
{
    // note that TAB is a format effector (does not write ' ');
    if (n < 1) {
        n = 1;
    }
    while ((n > 0) && (_cuX < getScreenLineColumns(_cuY) - 1)) {
        cursorRight(1);
        while ((_cuX < getScreenLineColumns(_cuY) - 1) && !_tabStops.at(_cuX)) {
            cursorRight(1);
        }
        --n;
    }
}

void Screen::backtab(int n)
{
    // note that TAB is a format effector (does not write ' ');
    if (n < 1) {
        n = 1;
    }
    while ((n > 0) && (_cuX > 0)) {
        cursorLeft(1);
        while ((_cuX > 0) && !_tabStops.at(_cuX)) {
            cursorLeft(1);
        }
        --n;
    }
}

void Screen::clearTabStops()
{
    _tabStops.fill(false);
}

void Screen::changeTabStop(bool set)
{
    if (_cuX >= _columns) {
        return;
    }

    _tabStops[_cuX] = set;
}

void Screen::initTabStops()
{
    _tabStops.resize(_columns);

    // The 1st tabstop has to be one longer than the other.
    // i.e. the kids start counting from 0 instead of 1.
    // Other programs might behave correctly. Be aware.
    for (int i = 0; i < _columns; ++i) {
        _tabStops[i] = (i % 8 == 0 && i != 0);
    }
}

void Screen::newLine()
{
    if (getMode(MODE_NewLine)) {
        toStartOfLine();
    }

    index();
}

void Screen::checkSelection(int from, int to)
{
    if (_selBegin == -1) {
        return;
    }
    const int scr_TL = loc(0, _history->getLines());
    // Clear entire selection if it overlaps region [from, to]
    if ((_selBottomRight >= (from + scr_TL)) && (_selTopLeft <= (to + scr_TL))) {
        clearSelection();
    }
}

void Screen::displayCharacter(uint c)
{
    // Note that VT100 does wrapping BEFORE putting the character.
    // This has impact on the assumption of valid cursor positions.
    // We indicate the fact that a newline has to be triggered by
    // putting the cursor one right to the last column of the screen.

    int w = Character::width(c);

    if (w < 0) {
        // Non-printable character
        return;
    } else if (w == 0) {
        const QChar::Category category = QChar::category(c);
        if (category != QChar::Mark_NonSpacing && category != QChar::Letter_Other && category != QChar::Other_Format) {
            return;
        }
        // Find previous "real character" to try to combine with
        int charToCombineWithX = qMin(_cuX, _screenLines.at(_cuY).length());
        int charToCombineWithY = _cuY;
        bool previousChar = true;
        do {
            if (charToCombineWithX > 0) {
                --charToCombineWithX;
            } else if (charToCombineWithY > 0 && _lineProperties.at(charToCombineWithY - 1) & LINE_WRAPPED) { // Try previous line
                --charToCombineWithY;
                charToCombineWithX = _screenLines.at(charToCombineWithY).length() - 1;
            } else {
                // Give up
                previousChar = false;
                break;
            }

            // Failsafe
            if (charToCombineWithX < 0) {
                previousChar = false;
                break;
            }
        } while (_screenLines.at(charToCombineWithY).at(charToCombineWithX).isRightHalfOfDoubleWide());

        if (!previousChar) {
            if (!Hangul::isHangul(c)) {
                return;
            } else {
                w = 2;
                goto notcombine;
            }
        }

        Character &currentChar = _screenLines[charToCombineWithY][charToCombineWithX];

        if (Hangul::isHangul(c) && !Hangul::combinesWith(currentChar, c)) {
            w = 2;
            goto notcombine;
        }

        if ((currentChar.rendition & RE_EXTENDED_CHAR) == 0) {
            const uint chars[2] = {currentChar.character, c};
            currentChar.rendition |= RE_EXTENDED_CHAR;
            auto extChars = [this]() {
                return usedExtendedChars();
            };
            currentChar.character = ExtendedCharTable::instance.createExtendedChar(chars, 2, extChars);
        } else {
            ushort extendedCharLength;
            const uint *oldChars = ExtendedCharTable::instance.lookupExtendedChar(currentChar.character, extendedCharLength);
            Q_ASSERT(oldChars);
            if (((oldChars) != nullptr) && extendedCharLength < 8) {
                Q_ASSERT(extendedCharLength > 1);
                Q_ASSERT(extendedCharLength < 65535); // redundant due to above check
                auto chars = std::make_unique<uint[]>(extendedCharLength + 1);
                std::copy_n(oldChars, extendedCharLength, chars.get());
                chars[extendedCharLength] = c;
                auto extChars = [this]() {
                    return usedExtendedChars();
                };
                currentChar.character = ExtendedCharTable::instance.createExtendedChar(chars.get(), extendedCharLength + 1, extChars);
            }
        }
        return;
    }

notcombine:
    if (_cuX + w > getScreenLineColumns(_cuY)) {
        if (getMode(MODE_Wrap)) {
            _lineProperties[_cuY] = static_cast<LineProperty>(_lineProperties.at(_cuY) | LINE_WRAPPED);
            nextLine();
        } else {
            _cuX = qMax(getScreenLineColumns(_cuY) - w, 0);
        }
    }

    // ensure current line vector has enough elements
    if (_screenLines[_cuY].size() < _cuX + w) {
        _screenLines[_cuY].resize(_cuX + w);
    }

    if (getMode(MODE_Insert)) {
        insertChars(w);
    }

    _lastPos = loc(_cuX, _cuY);

    // check if selection is still valid.
    checkSelection(_lastPos, _lastPos);

    Character &currentChar = _screenLines[_cuY][_cuX];

    currentChar.character = c;
    currentChar.foregroundColor = _effectiveForeground;
    currentChar.backgroundColor = _effectiveBackground;
    currentChar.rendition = _effectiveRendition;
    currentChar.isRealCharacter = true;

    _lastDrawnChar = c;

    int i = 0;
    const int newCursorX = _cuX + w--;
    while (w != 0) {
        ++i;

        if (_screenLines.at(_cuY).size() < _cuX + i + 1) {
            _screenLines[_cuY].resize(_cuX + i + 1);
        }

        Character &ch = _screenLines[_cuY][_cuX + i];
        ch.setRightHalfOfDoubleWide();
        ch.foregroundColor = _effectiveForeground;
        ch.backgroundColor = _effectiveBackground;
        ch.rendition = _effectiveRendition;
        ch.isRealCharacter = false;

        --w;
    }
    _cuX = newCursorX;
    _escapeSequenceUrlExtractor->appendUrlText(QChar(c));
}

int Screen::scrolledLines() const
{
    return _scrolledLines;
}
int Screen::droppedLines() const
{
    return _droppedLines;
}
void Screen::resetDroppedLines()
{
    _droppedLines = 0;
}
void Screen::resetScrolledLines()
{
    _scrolledLines = 0;
}

void Screen::scrollUp(int n)
{
    if (n < 1) {
        n = 1; // Default
    }
    for (int i = 0; i < n; i++) {
        if (_topMargin == 0) {
            addHistLine(); // history.history
        }
        scrollUp(_topMargin, 1);
    }
}

QRect Screen::lastScrolledRegion() const
{
    return _lastScrolledRegion;
}

void Screen::scrollUp(int from, int n)
{
    if (n <= 0) {
        return;
    }
    if (from > _bottomMargin) {
        return;
    }
    if (from + n > _bottomMargin) {
        n = _bottomMargin + 1 - from;
    }

    _scrolledLines -= n;
    _lastScrolledRegion = QRect(0, _topMargin, _columns - 1, (_bottomMargin - _topMargin));

    // FIXME: make sure `topMargin', `bottomMargin', `from', `n' is in bounds.
    moveImage(loc(0, from), loc(0, from + n), loc(_columns, _bottomMargin));
    clearImage(loc(0, _bottomMargin - n + 1), loc(_columns - 1, _bottomMargin), ' ');
    if (_hasGraphics) {
        scrollUpVisiblePlacements(n);
    }
}

void Screen::scrollDown(int n)
{
    if (n < 1) {
        n = 1; // Default
    }
    scrollDown(_topMargin, n);
}

void Screen::scrollDown(int from, int n)
{
    _scrolledLines += n;

    // FIXME: make sure `topMargin', `bottomMargin', `from', `n' is in bounds.
    if (n <= 0) {
        return;
    }
    if (from > _bottomMargin) {
        return;
    }
    if (n >= _bottomMargin + 1 - from) {
        n = _bottomMargin + 1 - from;
    } else {
        moveImage(loc(0, from + n), loc(0, from), loc(_columns - 1, _bottomMargin - n));
    }
    clearImage(loc(0, from), loc(_columns - 1, from + n - 1), ' ');
}

void Screen::setCursorYX(int y, int x)
{
    setCursorY(y);
    setCursorX(x);
}

void Screen::setCursorX(int x)
{
    if (x < 1) {
        x = 1; // Default
    }
    _cuX = qBound(0, x - 1, _columns - 1);
}

void Screen::setCursorY(int y)
{
    if (y < 1) {
        y = 1; // Default
    }
    if (y > MAX_SCREEN_ARGUMENT) {
        y = MAX_SCREEN_ARGUMENT;
    }
    y += (getMode(MODE_Origin) ? _topMargin : 0);
    _cuY = qBound(0, y - 1, _lines - 1);
}

void Screen::toStartOfLine()
{
    _cuX = 0;
}

int Screen::getCursorX() const
{
    return qMin(_cuX, _columns - 1);
}

int Screen::getCursorY() const
{
    return _cuY;
}

void Screen::clearImage(int loca, int loce, char c, bool resetLineRendition)
{
    const int scr_TL = loc(0, _history->getLines());
    // FIXME: check positions

    // Clear entire selection if it overlaps region to be moved...
    if ((_selBottomRight > (loca + scr_TL)) && (_selTopLeft < (loce + scr_TL))) {
        clearSelection();
    }

    const int topLine = loca / _columns;
    const int bottomLine = loce / _columns;

    Character clearCh(uint(c), _currentForeground, _currentBackground, DEFAULT_RENDITION, false);

    // if the character being used to clear the area is the same as the
    // default character, the affected _lines can simply be shrunk.
    const bool isDefaultCh = (clearCh == Screen::DefaultChar);

    for (int y = topLine; y <= bottomLine; ++y) {
        _lineProperties[y] &= ~LINE_WRAPPED;

        const int endCol = (y == bottomLine) ? loce % _columns : _columns - 1;
        const int startCol = (y == topLine) ? loca % _columns : 0;

        QVector<Character> &line = _screenLines[y];

        if (isDefaultCh && endCol == _columns - 1) {
            line.resize(startCol);
        } else {
            if (line.size() < endCol + 1) {
                line.resize(endCol + 1);
            }

            if (endCol == _columns - 1) {
                line.resize(endCol + 1);
            }

            if (startCol <= endCol) {
                std::fill(line.begin() + startCol, line.begin() + (endCol + 1), clearCh);
            }
        }

        if (resetLineRendition && startCol == 0 && endCol == _columns - 1) {
            _lineProperties[y] &= ~(LINE_DOUBLEWIDTH | LINE_DOUBLEHEIGHT_TOP | LINE_DOUBLEHEIGHT_BOTTOM);
        }
    }
}

void Screen::moveImage(int dest, int sourceBegin, int sourceEnd)
{
    Q_ASSERT(sourceBegin <= sourceEnd);

    const int lines = (sourceEnd - sourceBegin) / _columns;

    // move screen image and line properties:
    // the source and destination areas of the image may overlap,
    // so it matters that we do the copy in the right order -
    // forwards if dest < sourceBegin or backwards otherwise.
    //(search the web for 'memmove implementation' for details)
    const int destY = dest / _columns;
    const int srcY = sourceBegin / _columns;
    if (dest < sourceBegin) {
        /**
         * This is basically a left rotate.
         *
         * - "dest -> src" is the range of lines we want to move to the end
         * - "lines" is the range of lines that will be rotated
         *
         * we take lines [destY, srcY] and move them to the end of 'lines'.
         * Then we remove the now moved-from lines [destY, srcY].
         *
         * std::rotate can be used here but it is slower than this approach.
         */
        auto from = std::make_move_iterator(_screenLines.begin() + destY);
        auto to = std::make_move_iterator(_screenLines.begin() + srcY);

        _screenLines.insert(_screenLines.begin() + lines + srcY, from, to);
        _screenLines.erase(_screenLines.begin() + destY, _screenLines.begin() + srcY);

        std::rotate(_lineProperties.begin() + destY, _lineProperties.begin() + srcY, _lineProperties.begin() + srcY + lines);
    } else {
        for (int i = lines; i >= 0; --i) {
            _screenLines[destY + i] = std::move(_screenLines[srcY + i]);
            _lineProperties[destY + i] = _lineProperties.at(srcY + i);
        }
    }

    if (_lastPos != -1) {
        const int diff = dest - sourceBegin; // Scroll by this amount
        _lastPos += diff;
        if ((_lastPos < 0) || (_lastPos >= (lines * _columns))) {
            _lastPos = -1;
        }
    }

    // Adjust selection to follow scroll.
    if (_selBegin != -1) {
        const bool beginIsTL = (_selBegin == _selTopLeft);
        const int diff = dest - sourceBegin; // Scroll by this amount
        const int scr_TL = loc(0, _history->getLines());
        const int srca = sourceBegin + scr_TL; // Translate index from screen to global
        const int srce = sourceEnd + scr_TL; // Translate index from screen to global
        const int desta = srca + diff;
        const int deste = srce + diff;

        if ((_selTopLeft >= srca) && (_selTopLeft <= srce)) {
            _selTopLeft += diff;
        } else if ((_selTopLeft >= desta) && (_selTopLeft <= deste)) {
            _selBottomRight = -1; // Clear selection (see below)
        }

        if ((_selBottomRight >= srca) && (_selBottomRight <= srce)) {
            _selBottomRight += diff;
        } else if ((_selBottomRight >= desta) && (_selBottomRight <= deste)) {
            _selBottomRight = -1; // Clear selection (see below)
        }

        if (_selBottomRight < 0) {
            clearSelection();
        } else {
            if (_selTopLeft < 0) {
                _selTopLeft = 0;
            }
        }

        if (beginIsTL) {
            _selBegin = _selTopLeft;
        } else {
            _selBegin = _selBottomRight;
        }
    }
}

void Screen::clearToEndOfScreen()
{
    clearImage(loc(_cuX, _cuY), loc(_columns - 1, _lines - 1), ' ');
}

void Screen::clearToBeginOfScreen()
{
    clearImage(loc(0, 0), loc(_cuX, _cuY), ' ');
}

void Screen::clearEntireScreen()
{
    clearImage(loc(0, 0), loc(_columns - 1, _lines - 1), ' ');
    if (_hasGraphics) {
        delPlacements();
        _currentTerminalDisplay->update();
    }
}

/*! fill screen with 'E'
  This is to aid screen alignment
  */

void Screen::helpAlign()
{
    clearImage(loc(0, 0), loc(_columns - 1, _lines - 1), 'E');
    _cuY = 0;
    _cuX = 0;
}

void Screen::clearToEndOfLine()
{
    clearImage(loc(_cuX, _cuY), loc(_columns - 1, _cuY), ' ', false);
}

void Screen::clearToBeginOfLine()
{
    clearImage(loc(0, _cuY), loc(_cuX, _cuY), ' ', false);
}

void Screen::clearEntireLine()
{
    clearImage(loc(0, _cuY), loc(_columns - 1, _cuY), ' ', false);
}

void Screen::setRendition(RenditionFlags rendition)
{
    _currentRendition |= rendition;
    updateEffectiveRendition();
}

void Screen::resetRendition(RenditionFlags rendition)
{
    _currentRendition &= ~rendition;
    updateEffectiveRendition();
}

void Screen::setDefaultRendition()
{
    setForeColor(COLOR_SPACE_DEFAULT, DEFAULT_FORE_COLOR);
    setBackColor(COLOR_SPACE_DEFAULT, DEFAULT_BACK_COLOR);
    _currentRendition = DEFAULT_RENDITION;
    updateEffectiveRendition();
}

void Screen::setForeColor(int space, int color)
{
    _currentForeground = CharacterColor(quint8(space), color);

    if (_currentForeground.isValid()) {
        updateEffectiveRendition();
    } else {
        setForeColor(COLOR_SPACE_DEFAULT, DEFAULT_FORE_COLOR);
    }
}

void Screen::setBackColor(int space, int color)
{
    _currentBackground = CharacterColor(quint8(space), color);

    if (_currentBackground.isValid()) {
        updateEffectiveRendition();
    } else {
        setBackColor(COLOR_SPACE_DEFAULT, DEFAULT_BACK_COLOR);
    }
}

void Screen::clearSelection()
{
    _selBottomRight = -1;
    _selTopLeft = -1;
    _selBegin = -1;
}

bool Screen::hasSelection() const
{
    return _selBegin != -1;
}

void Screen::getSelectionStart(int &column, int &line) const
{
    if (_selTopLeft != -1) {
        column = _selTopLeft % _columns;
        line = _selTopLeft / _columns;
    } else {
        column = _cuX + getHistLines();
        line = _cuY + getHistLines();
    }
}
void Screen::getSelectionEnd(int &column, int &line) const
{
    if (_selBottomRight != -1) {
        column = _selBottomRight % _columns;
        line = _selBottomRight / _columns;
    } else {
        column = _cuX + getHistLines();
        line = _cuY + getHistLines();
    }
}
void Screen::setSelectionStart(const int x, const int y, const bool blockSelectionMode)
{
    _selBegin = loc(x, y);
    /* FIXME, HACK to correct for x too far to the right... */
    if (x == _columns) {
        --_selBegin;
    }

    _selBottomRight = _selBegin;
    _selTopLeft = _selBegin;
    _blockSelectionMode = blockSelectionMode;
}

void Screen::setSelectionEnd(const int x, const int y)
{
    if (_selBegin == -1) {
        return;
    }

    int endPos = loc(x, y);

    if (endPos < _selBegin) {
        _selTopLeft = endPos;
        _selBottomRight = _selBegin;
    } else {
        /* FIXME, HACK to correct for x too far to the right... */
        if (x == _columns) {
            --endPos;
        }

        _selTopLeft = _selBegin;
        _selBottomRight = endPos;
    }

    // Normalize the selection in column mode
    if (_blockSelectionMode) {
        const int topRow = _selTopLeft / _columns;
        const int topColumn = _selTopLeft % _columns;
        const int bottomRow = _selBottomRight / _columns;
        const int bottomColumn = _selBottomRight % _columns;

        _selTopLeft = loc(qMin(topColumn, bottomColumn), topRow);
        _selBottomRight = loc(qMax(topColumn, bottomColumn), bottomRow);
    }
}

bool Screen::isSelected(const int x, const int y) const
{
    bool columnInSelection = true;
    if (_blockSelectionMode) {
        columnInSelection = x >= (_selTopLeft % _columns) && x <= (_selBottomRight % _columns);
    }

    const int pos = loc(x, y);
    return pos >= _selTopLeft && pos <= _selBottomRight && columnInSelection;
}

QString Screen::selectedText(const DecodingOptions options) const
{
    if (!isSelectionValid()) {
        return QString();
    }

    return text(_selTopLeft, _selBottomRight, options);
}

QString Screen::text(int startIndex, int endIndex, const DecodingOptions options) const
{
    QString result;
    QTextStream stream(&result, QIODevice::ReadWrite);

    HTMLDecoder htmlDecoder;
    PlainTextDecoder plainTextDecoder;

    TerminalCharacterDecoder *decoder;
    if ((options & ConvertToHtml) != 0U) {
        decoder = &htmlDecoder;
    } else {
        decoder = &plainTextDecoder;
    }

    decoder->begin(&stream);
    writeToStream(decoder, startIndex, endIndex, options);
    decoder->end();

    return result;
}

bool Screen::isSelectionValid() const
{
    return _selTopLeft >= 0 && _selBottomRight >= 0;
}

void Screen::writeToStream(TerminalCharacterDecoder *decoder, int startIndex, int endIndex, const DecodingOptions options) const
{
    const int top = startIndex / _columns;
    const int left = startIndex % _columns;

    const int bottom = endIndex / _columns;
    const int right = endIndex % _columns;

    Q_ASSERT(top >= 0 && left >= 0 && bottom >= 0 && right >= 0);

    for (int y = top; y <= bottom; ++y) {
        int start = 0;
        if (y == top || _blockSelectionMode) {
            start = left;
        }

        int count = -1;
        if (y == bottom || _blockSelectionMode) {
            count = right - start + 1;
        }

        const bool appendNewLine = (y != bottom);
        int copied = copyLineToStream(y, start, count, decoder, appendNewLine, _blockSelectionMode, options);

        // if the selection goes beyond the end of the last line then
        // append a new line character.
        //
        // this makes it possible to 'select' a trailing new line character after
        // the text on a line.
        if (y == bottom && copied < count && !options.testFlag(TrimTrailingWhitespace)) {
            Character newLineChar('\n');
            decoder->decodeLine(&newLineChar, 1, 0);
        }
    }
}

int Screen::getLineLength(const int line) const
{
    // determine if the line is in the history buffer or the screen image
    const bool isInHistoryBuffer = line < _history->getLines();

    if (isInHistoryBuffer) {
        return _history->getLineLen(line);
    }

    return _columns;
}

Character *Screen::getCharacterBuffer(const int size)
{
    // buffer to hold characters for decoding
    // the buffer is static to avoid initializing every
    // element on each call to copyLineToStream
    // (which is unnecessary since all elements will be overwritten anyway)
    static const int MAX_CHARS = 1024;
    static QVector<Character> characterBuffer(MAX_CHARS);

    if (characterBuffer.count() < size) {
        characterBuffer.resize(size);
    }

    return characterBuffer.data();
}

int Screen::copyLineToStream(int line,
                             int start,
                             int count,
                             TerminalCharacterDecoder *decoder,
                             bool appendNewLine,
                             bool isBlockSelectionMode,
                             const DecodingOptions options) const
{
    const int lineLength = getLineLength(line);
    // ensure that this method, can append space or 'eol' character to
    // the selection
    Character *characterBuffer = getCharacterBuffer((count > -1 ? count : lineLength - start) + 1);
    LineProperty currentLineProperties = 0;

    // determine if the line is in the history buffer or the screen image
    if (line < _history->getLines()) {
        // ensure that start position is before end of line
        start = qBound(0, start, lineLength - 1);

        // retrieve line from history buffer.  It is assumed
        // that the history buffer does not store trailing white space
        // at the end of the line, so it does not need to be trimmed here
        if (count == -1) {
            count = lineLength - start;
        } else {
            count = qMin(start + count, lineLength) - start;
        }

        // safety checks
        Q_ASSERT(start >= 0);
        Q_ASSERT(count >= 0);
        Q_ASSERT((start + count) <= _history->getLineLen(line));

        _history->getCells(line, start, count, characterBuffer);

        if (_history->isWrappedLine(line)) {
            currentLineProperties |= LINE_WRAPPED;
        }
    } else {
        if (count == -1) {
            count = lineLength - start;
        }

        Q_ASSERT(count >= 0);

        int screenLine = line - _history->getLines();

        Q_ASSERT(screenLine <= _screenLinesSize);

        screenLine = qMin(screenLine, _screenLinesSize);

        auto *data = _screenLines[screenLine].data();
        int length = _screenLines.at(screenLine).count();

        // Don't remove end spaces in lines that wrap
        if (options.testFlag(TrimTrailingWhitespace) && ((_lineProperties.at(screenLine) & LINE_WRAPPED) == 0)) {
            // ignore trailing white space at the end of the line
            while (length > 0 && QChar(data[length - 1].character).isSpace()) {
                length--;
            }
        }

        // retrieve line from screen image
        auto end = qMin(start + count, length);
        if (start < end) {
            std::copy(data + start, data + end, characterBuffer);
        }

        // count cannot be any greater than length
        count = qBound(0, count, length - start);

        Q_ASSERT((size_t)screenLine < _lineProperties.size());
        currentLineProperties |= _lineProperties[screenLine];
    }

    if (appendNewLine) {
        // When users ask not to preserve the linebreaks, they usually mean:
        // `treat LINEBREAK as SPACE, thus joining multiple _lines into
        // single line in the same way as 'J' does in VIM.`
        const bool isLineWrapped = (currentLineProperties & LINE_WRAPPED) != 0;
        if (isBlockSelectionMode || !isLineWrapped) {
            characterBuffer[count] = options.testFlag(PreserveLineBreaks) ? Character('\n') : Character(' ');
            ++count;
        }
    }

    int spacesCount = 0;
    if ((options & TrimLeadingWhitespace) != 0U) {
        for (spacesCount = 0; spacesCount < count; ++spacesCount) {
            if (QChar::category(characterBuffer[spacesCount].character) != QChar::Category::Separator_Space) {
                break;
            }
        }

        if (spacesCount >= count) {
            return 0;
        }

        count -= spacesCount;
    }

    // decode line and write to text stream
    decoder->decodeLine(characterBuffer + spacesCount, count, currentLineProperties);

    return count;
}

void Screen::writeLinesToStream(TerminalCharacterDecoder *decoder, int fromLine, int toLine) const
{
    writeToStream(decoder, loc(0, fromLine), loc(_columns - 1, toLine), PreserveLineBreaks);
}

void Screen::fastAddHistLine()
{
    const bool removeLine = _history->getLines() == _history->getMaxLines();
    _history->addCellsVector(_screenLines.at(0));
    _history->addLine(_lineProperties.at(0));

    // If _history size > max history size it will drop a line from _history.
    // We need to verify if we need to remove a URL.
    if (removeLine) {
        _escapeSequenceUrlExtractor->historyLinesRemoved(1);
    }

    // Rotate left + clear the last line
    std::rotate(_screenLines.begin(), _screenLines.begin() + 1, _screenLines.end());
    auto last = _screenLines.back();
    Character clearCh(uint(' '), _currentForeground, _currentBackground, DEFAULT_RENDITION, false);
    std::fill(last.begin(), last.end(), clearCh);

    _lineProperties.erase(_lineProperties.begin());
}

void Screen::addHistLine()
{
    // add line to history buffer
    // we have to take care about scrolling, too...
    const int oldHistLines = _history->getLines();
    int newHistLines = _history->getLines();

    if (hasScroll()) {
        _history->addCellsVector(_screenLines.at(0));
        _history->addLine(_lineProperties.at(0));

        newHistLines = _history->getLines();

        // If the history is full, increment the count
        // of dropped _lines
        if (newHistLines == oldHistLines) {
            ++_droppedLines;

            // We removed a line, we need to verify if we need to remove a URL.
            _escapeSequenceUrlExtractor->historyLinesRemoved(1);
        }
    }

    bool beginIsTL = (_selBegin == _selTopLeft);

    // Adjust selection for the new point of reference
    if (newHistLines != oldHistLines) {
        if (_selBegin != -1) {
            _selTopLeft += _columns * (newHistLines - oldHistLines);
            _selBottomRight += _columns * (newHistLines - oldHistLines);
        }
    }

    if (_selBegin != -1) {
        // Scroll selection in history up
        const int top_BR = loc(0, 1 + newHistLines);

        if (_selTopLeft < top_BR) {
            _selTopLeft -= _columns;
        }

        if (_selBottomRight < top_BR) {
            _selBottomRight -= _columns;
        }

        if (_selBottomRight < 0) {
            clearSelection();
        } else {
            if (_selTopLeft < 0) {
                _selTopLeft = 0;
            }
        }

        if (beginIsTL) {
            _selBegin = _selTopLeft;
        } else {
            _selBegin = _selBottomRight;
        }
    }
}

int Screen::getHistLines() const
{
    return _history->getLines();
}

void Screen::setScroll(const HistoryType &t, bool copyPreviousScroll)
{
    clearSelection();

    if (copyPreviousScroll) {
        t.scroll(_history);
    } else {
        // As 't' can be '_history' pointer, move it to a temporary smart pointer
        // making _history = nullptr
        auto oldHistory = std::move(_history);
        t.scroll(_history);
    }

#ifdef HAVE_MALLOC_H
#ifdef Q_OS_LINUX
#ifdef __GNUC__
    // We might have been using gigabytes of memory, so make sure it is actually released
    malloc_trim(0);
#endif
#endif
#endif
}

bool Screen::hasScroll() const
{
    return _history->hasScroll();
}

const HistoryType &Screen::getScroll() const
{
    return _history->getType();
}

void Screen::setLineProperty(LineProperty property, bool enable)
{
    if (enable) {
        _lineProperties[_cuY] = static_cast<LineProperty>(_lineProperties.at(_cuY) | property);
    } else {
        _lineProperties[_cuY] = static_cast<LineProperty>(_lineProperties.at(_cuY) & ~property);
    }
}
void Screen::fillWithDefaultChar(Character *dest, int count)
{
    std::fill_n(dest, count, Screen::DefaultChar);
}

Konsole::EscapeSequenceUrlExtractor *Konsole::Screen::urlExtractor() const
{
    return _escapeSequenceUrlExtractor.get();
}

void Screen::addPlacement(QPixmap pixmap,
                          int &rows,
                          int &cols,
                          int row,
                          int col,
                          bool scrolling,
                          int moveCursor,
                          bool leaveText,
                          int z,
                          int id,
                          int pid,
                          qreal opacity,
                          int X,
                          int Y)
{
    if (pixmap.isNull()) {
        return;
    }

    std::unique_ptr<TerminalGraphicsPlacement_t> p(new TerminalGraphicsPlacement_t);

    if (row == -1) {
        row = _cuY;
    }
    if (col == -1) {
        col = _cuX;
    }
    if (rows == -1) {
        rows = (pixmap.height() - 1) / _currentTerminalDisplay->terminalFont()->fontHeight() + 1;
    }
    if (cols == -1) {
        cols = (pixmap.width() - 1) / _currentTerminalDisplay->terminalFont()->fontWidth() + 1;
    }

    p->pixmap = pixmap;
    p->z = z;
    p->row = row;
    p->col = col;
    p->rows = rows;
    p->cols = cols;
    p->id = id;
    p->pid = pid;
    p->opacity = opacity;
    p->scrolling = scrolling;
    p->X = X;
    p->Y = Y;

    if (!leaveText) {
        eraseBlock(row, col, rows, cols);
    }
    addPlacement(p);
    int needScroll = qBound(0, row + rows - _lines, rows);
    if (moveCursor && scrolling && needScroll > 0) {
        scrollUp(needScroll);
    }
    if (moveCursor) {
        if (rows - needScroll - 1 > 0) {
            cursorDown(rows - needScroll - 1);
        }
        if (moveCursor == 2 || _cuX + cols >= _columns) {
            toStartOfLine();
            newLine();
        } else {
            cursorRight(cols);
        }
    }
}

void Screen::addPlacement(std::unique_ptr<TerminalGraphicsPlacement_t> &placement)
{
    std::vector<std::unique_ptr<TerminalGraphicsPlacement_t>>::iterator i;
    // remove placement with the same id and pid, if pid is non zero
    if (placement->pid >= 0 && placement->id >= 0) {
        i = _graphicsPlacements.begin();
        while (i != _graphicsPlacements.end()) {
            TerminalGraphicsPlacement_t *p = i->get();
            if (p->id == placement->id && p->pid == placement->pid) {
                _graphicsPlacements.erase(i);
                break;
            }
            i++;
        }
    }

    for (i = _graphicsPlacements.begin(); i != _graphicsPlacements.end() && placement->z >= i->get()->z; i++)
        ;
    _graphicsPlacements.insert(i, std::move(placement));
    _hasGraphics = true;
    // Placements with pid<0 cannot be deleted by the application, so remove those fully covered
    // by others.
    QRegion covered = QRegion();
    std::vector<std::unique_ptr<TerminalGraphicsPlacement_t>>::reverse_iterator rit;
    rit = _graphicsPlacements.rbegin();
    while (rit != _graphicsPlacements.rend()) {
        TerminalGraphicsPlacement_t *p = rit->get();
        if (p->pid < 0) {
            QRect rect(p->col, p->row, p->cols, p->rows);
            if (covered.intersected(rect) == QRegion(rect)) {
                std::advance(rit, 1);
                _graphicsPlacements.erase(rit.base());
            } else {
                covered += rect;
                std::advance(rit, 1);
            }
        } else {
            std::advance(rit, 1);
        }
    }
}

TerminalGraphicsPlacement_t *Screen::getGraphicsPlacement(unsigned int i)
{
    if (i >= _graphicsPlacements.size()) {
        return nullptr;
    }
    return _graphicsPlacements[i].get();
}

void Screen::scrollUpVisiblePlacements(int n)
{
    std::vector<std::unique_ptr<TerminalGraphicsPlacement_t>>::iterator i;
    int histMaxLines = _history->getMaxLines();
    i = _graphicsPlacements.begin();
    while (i != _graphicsPlacements.end()) {
        TerminalGraphicsPlacement_t *placement = i->get();
        if (placement->scrolling) {
            placement->row -= n;
            if (placement->row + placement->rows < -histMaxLines) {
                i = _graphicsPlacements.erase(i);
            } else {
                i++;
            }
        } else {
            i++;
        }
    }
}

void Screen::delPlacements(int del, qint64 id, qint64 pid, int x, int y, int z)
{
    auto i = _graphicsPlacements.begin();
    while (i != _graphicsPlacements.end()) {
        TerminalGraphicsPlacement_t *placement = i->get();
        bool remove = false;
        switch (del) {
        case 1:
            remove = true;
            break;
        case 'z':
            if (placement->z == z) {
                remove = true;
            }
            break;
        case 'x':
            if (placement->col <= x && x < placement->col + placement->cols) {
                remove = true;
            }
            break;
        case 'y':
            if (placement->row <= y && y < placement->row + placement->rows) {
                remove = true;
            }
            break;
        case 'p':
            if (placement->col <= x && x < placement->col + placement->cols && placement->row <= y && y < placement->row + placement->rows) {
                remove = true;
            }
            break;
        case 'q':
            if (placement->col <= x && x < placement->col + placement->cols && placement->row <= y && y < placement->row + placement->rows
                && placement->z == z) {
                remove = true;
            }
            break;
        case 'a':
            if (placement->row + placement->rows > 0) {
                remove = true;
            }
            break;
        case 'i':
            if ((id < 0 || placement->id == id) && (pid < 0 || placement->pid == pid)) {
                remove = true;
            }
            break;
        }
        if (remove) {
            i = _graphicsPlacements.erase(i);
        } else {
            i++;
        }
    }
}

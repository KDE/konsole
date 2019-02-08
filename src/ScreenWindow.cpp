/*
    Copyright (C) 2007 by Robert Knight <robertknight@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own
#include "ScreenWindow.h"

// Konsole
#include "Screen.h"
#include "ExtendedCharTable.h"

#include <memory>

using namespace Konsole;

ScreenWindow::ScreenWindow(Screen *screen, QObject *parent) :
    QObject(parent),
    _screen(nullptr),
    _windowBuffer(nullptr),
    _windowBufferSize(0),
    _bufferNeedsUpdate(true),
    _windowLines(1),
    _currentLine(0),
    _currentResultLine(-1),
    _trackOutput(true),
    _scrollCount(0)
{
    setScreen(screen);
}

ScreenWindow::~ScreenWindow()
{
    delete[] _windowBuffer;
}

void ScreenWindow::setScreen(Screen *screen)
{
    Q_ASSERT(screen);

    _screen = screen;
}

Screen *ScreenWindow::screen() const
{
    return _screen;
}

Character *ScreenWindow::getImage()
{
    // reallocate internal buffer if the window size has changed
    int size = windowLines() * windowColumns();
    if (_windowBuffer == nullptr || _windowBufferSize != size) {
        delete[] _windowBuffer;
        _windowBufferSize = size;
        _windowBuffer = new Character[size];
        _bufferNeedsUpdate = true;
    }

    if (!_bufferNeedsUpdate) {
        return _windowBuffer;
    }

    _screen->getImage(_windowBuffer, size,
                      currentLine(), endWindowLine());

    // this window may look beyond the end of the screen, in which
    // case there will be an unused area which needs to be filled
    // with blank characters
    fillUnusedArea();

    _bufferNeedsUpdate = false;
    return _windowBuffer;
}

void ScreenWindow::fillUnusedArea()
{
    int screenEndLine = _screen->getHistLines() + _screen->getLines() - 1;
    int windowEndLine = currentLine() + windowLines() - 1;

    int unusedLines = windowEndLine - screenEndLine;

    // stop when unusedLines is negative; there is an issue w/ charsToFill
    //  being greater than an int can hold
    if (unusedLines <= 0) {
        return;
    }

    int charsToFill = unusedLines * windowColumns();

    Screen::fillWithDefaultChar(_windowBuffer + _windowBufferSize - charsToFill, charsToFill);
}

int ScreenWindow::loc(int x, int y) const
{
    Q_ASSERT(y >= 0 && y < lineCount());
    Q_ASSERT(x >= 0 && x < columnCount());
    x = qBound(0, x, columnCount() - 1);
    y = qBound(0, y, lineCount() - 1);

    return y * columnCount() + x;
}

// return the index of the line at the end of this window, or if this window
// goes beyond the end of the screen, the index of the line at the end
// of the screen.
//
// when passing a line number to a Screen method, the line number should
// never be more than endWindowLine()
//
int ScreenWindow::endWindowLine() const
{
    return qMin(currentLine() + windowLines() - 1,
                lineCount() - 1);
}

QVector<LineProperty> ScreenWindow::getLineProperties()
{
    QVector<LineProperty> result = _screen->getLineProperties(currentLine(), endWindowLine());

    if (result.count() != windowLines()) {
        result.resize(windowLines());
    }

    return result;
}

QString ScreenWindow::selectedText(const Screen::DecodingOptions options) const
{
    return _screen->selectedText(options);
}

QPoint ScreenWindow::findLineStart(const QPoint &pnt)
{
    QVector<LineProperty> lineProperties = getLineProperties();

    const int visibleScreenLines = lineProperties.size();
    const int topVisibleLine = currentLine();
    int line = pnt.y();
    int lineInHistory= line + topVisibleLine;


    while (lineInHistory > 0) {
        for (; line > 0; line--, lineInHistory--) {
            // Does previous line wrap around?
            if ((lineProperties[line - 1] & LINE_WRAPPED) == 0) {
                return {0, lineInHistory - topVisibleLine};
            }
        }

        if (lineInHistory < 1) {
            break;
        }

        // _lineProperties is only for the visible screen, so grab new data
        int newRegionStart = qMax(0, lineInHistory - visibleScreenLines);
        lineProperties = _screen->getLineProperties(newRegionStart, lineInHistory - 1);
        line = lineInHistory - newRegionStart;
    }

    return {0, lineInHistory - topVisibleLine};
}

QPoint ScreenWindow::findLineEnd(const QPoint &pnt)
{
    QVector<LineProperty> lineProperties = getLineProperties();

    const int visibleScreenLines = lineProperties.size();
    const int topVisibleLine = currentLine();
    const int maxY = lineCount() - 1;
    int line = pnt.y();
    int lineInHistory= line + topVisibleLine;

    while (lineInHistory < maxY) {
        for (; line < lineProperties.count() && lineInHistory < maxY; line++, lineInHistory++) {
            // Does current line wrap around?
            if ((lineProperties[line] & LINE_WRAPPED) == 0) {
                return {columnCount() - 1, lineInHistory - topVisibleLine};
            }
        }

        line = 0;
        lineProperties = _screen->getLineProperties(lineInHistory, qMin(lineInHistory + visibleScreenLines, maxY));
    }
    return {columnCount() - 1, lineInHistory - topVisibleLine};
}

QPoint ScreenWindow::findWordStart(const QPoint &pnt)
{
    const int regSize = qMax(windowLines(), 10);
    const int firstVisibleLine = currentLine();

    Character *image = getImage();
    std::unique_ptr<Character[]> tempImage;

    int imgLine = pnt.y();

    if (imgLine < 0 || imgLine >= lineCount()) {
        return pnt;
    }

    int x = pnt.x();
    int y = imgLine + firstVisibleLine;
    int imgLoc = loc(x, imgLine);
    QVector<LineProperty> lineProperties = getLineProperties();
    const QChar selClass = charClass(image[imgLoc]);
    const int imageSize = regSize * columnCount();

    while (imgLoc >= 0) {
        for (; imgLoc > 0 && imgLine >= 0; imgLoc--, x--) {
            const QChar &curClass = charClass(image[imgLoc - 1]);
            if (curClass != selClass) {
                return {x, y - firstVisibleLine};
            }

            // has previous char on this line
            if (x > 0) {
                continue;
            }

            // not the first line in the session
            if ((lineProperties[imgLine - 1] & LINE_WRAPPED) == 0) {
                return {x, y - firstVisibleLine};
            }

            // have continuation on prev line
            x = columnCount();
            imgLine--;
            y--;
        }

        if (y <= 0) {
            return {x, y - firstVisibleLine};
        }

        // Fetch new region
        const int newRegStart = qMax(y - regSize + 1, 0);
        lineProperties = _screen->getLineProperties(newRegStart, y - 1);
        imgLine = y - newRegStart;

        if (!tempImage) {
            tempImage.reset(new Character[imageSize]);
            image = tempImage.get();
        }

        _screen->getImage(image, imageSize, newRegStart, y - 1);
        imgLoc = loc(x, imgLine);
    }

    return {x, y - firstVisibleLine};

}

QPoint ScreenWindow::findWordEnd(const QPoint &pnt)
{
    const int regSize = qMax(windowLines(), 10);
    const int curLine = currentLine();
    int line = pnt.y();

    // The selection is already scrolled out of view, so assume it is already at a boundary
    if (line < 0 || line >= lineCount()) {
        return pnt;
    }

    int x = pnt.x();
    int y = line + curLine;
    QVector<LineProperty> lineProperties = getLineProperties();
    Character *image = getImage();
    std::unique_ptr<Character[]> tempImage;

    int imgPos = loc(x, line);
    const QChar selClass = charClass(image[imgPos]);
    QChar curClass;
    QChar nextClass;

    const int imageSize = regSize * columnCount();
    const int maxY = lineCount();
    const int maxX = columnCount() - 1;

    while (x >= 0 && line >= 0) {
        imgPos = loc(x, line);

        const int visibleLinesCount = lineProperties.count();
        bool changedClass = false;

        for (;y < maxY && line < visibleLinesCount; imgPos++, x++) {
            curClass = charClass(image[imgPos + 1]);
            nextClass = charClass(image[imgPos + 2]);

            changedClass = curClass != selClass &&
                // A colon right before whitespace is never part of a word
                !(image[imgPos + 1].character == ':' && nextClass == QLatin1Char(' '));

            if (changedClass) {
                break;
            }

            if (x >= maxX) {
                if ((lineProperties[line] & LINE_WRAPPED) == 0) {
                    break;
                }

                line++;
                y++;
                x = -1;
            }
        }

        if (changedClass) {
            break;
        }

        if (line < visibleLinesCount && ((lineProperties[line] & LINE_WRAPPED) == 0)) {
            break;
        }

        const int newRegEnd = qMin(y + regSize - 1, maxY - 1);
        lineProperties = _screen->getLineProperties(y, newRegEnd);
        if (!tempImage) {
            tempImage.reset(new Character[imageSize]);
            image = tempImage.get();
        }
        _screen->getImage(tempImage.get(), imageSize, y, newRegEnd);

        line = 0;
    }

    y -= curLine;
    // In word selection mode don't select @ (64) if at end of word.
    if (((image[imgPos].rendition & RE_EXTENDED_CHAR) == 0) &&
        (QChar(image[imgPos].character) == QLatin1Char('@')) &&
        (y > pnt.y() || x > pnt.x())) {
        if (x > 0) {
            x--;
        } else {
            y--;
        }
    }

    return {x, y};

}

QChar ScreenWindow::charClass(const Character &ch) const
{
    if ((ch.rendition & RE_EXTENDED_CHAR) != 0) {
        ushort extendedCharLength = 0;
        const uint* chars = ExtendedCharTable::instance.lookupExtendedChar(ch.character, extendedCharLength);
        if ((chars != nullptr) && extendedCharLength > 0) {
            const QString s = QString::fromUcs4(chars, extendedCharLength);
            if (_wordCharacters.contains(s, Qt::CaseInsensitive)) {
                return QLatin1Char('a');
            }
            bool letterOrNumber = false;
            for (int i = 0; !letterOrNumber && i < s.size(); ++i) {
                letterOrNumber = s.at(i).isLetterOrNumber();
            }
            return letterOrNumber ? QLatin1Char('a') : s.at(0);
        }
        return 0;
    } else {
        const QChar qch(ch.character);
        if (qch.isSpace()) {
            return QLatin1Char(' ');
        }

        if (qch.isLetterOrNumber() || _wordCharacters.contains(qch, Qt::CaseInsensitive)) {
            return QLatin1Char('a');
        }

        return qch;
    }
}

void ScreenWindow::setWordCharacters(const QString &wc)
{
    _wordCharacters = wc;
}

void ScreenWindow::getSelectionStart(int &column, int &line)
{
    _screen->getSelectionStart(column, line);
    line -= currentLine();
}

void ScreenWindow::getSelectionEnd(int &column, int &line)
{
    _screen->getSelectionEnd(column, line);
    line -= currentLine();
}

void ScreenWindow::setSelectionStart(int column, int line, bool columnMode)
{
    _screen->setSelectionStart(column, line + currentLine(), columnMode);

    _bufferNeedsUpdate = true;
    emit selectionChanged();
}

void ScreenWindow::setSelectionEnd(int column, int line)
{
    _screen->setSelectionEnd(column, line + currentLine());

    _bufferNeedsUpdate = true;
    emit selectionChanged();
}

void ScreenWindow::setSelectionByLineRange(int start, int end)
{
    clearSelection();

    _screen->setSelectionStart(0, start, false);
    _screen->setSelectionEnd(windowColumns(), end);

    _bufferNeedsUpdate = true;
    emit selectionChanged();
}

bool ScreenWindow::isSelected(int column, int line)
{
    return _screen->isSelected(column, qMin(line + currentLine(), endWindowLine()));
}

void ScreenWindow::clearSelection()
{
    _screen->clearSelection();

    emit selectionChanged();
}

void ScreenWindow::setWindowLines(int lines)
{
    Q_ASSERT(lines > 0);
    _windowLines = lines;
}

int ScreenWindow::windowLines() const
{
    return _windowLines;
}

int ScreenWindow::windowColumns() const
{
    return _screen->getColumns();
}

int ScreenWindow::lineCount() const
{
    return _screen->getHistLines() + _screen->getLines();
}

int ScreenWindow::columnCount() const
{
    return _screen->getColumns();
}

QPoint ScreenWindow::cursorPosition() const
{
    QPoint position;

    position.setX(_screen->getCursorX());
    position.setY(_screen->getCursorY());

    return position;
}

int ScreenWindow::currentLine() const
{
    return qBound(0, _currentLine, lineCount() - windowLines());
}

int ScreenWindow::currentResultLine() const
{
    return _currentResultLine;
}

void ScreenWindow::setCurrentResultLine(int line)
{
    if (_currentResultLine == line) {
        return;
    }

    _currentResultLine = line;
    emit currentResultLineChanged();
}

void ScreenWindow::scrollBy(RelativeScrollMode mode, int amount, bool fullPage)
{
    if (mode == ScrollLines) {
        scrollTo(currentLine() + amount);
    } else if (mode == ScrollPages) {
        if (fullPage) {
            scrollTo(currentLine() + amount * (windowLines()));
        } else {
            scrollTo(currentLine() + amount * (windowLines() / 2));
        }
    }
}

bool ScreenWindow::atEndOfOutput() const
{
    return currentLine() == (lineCount() - windowLines());
}

void ScreenWindow::scrollTo(int line)
{
    int maxCurrentLineNumber = lineCount() - windowLines();
    line = qBound(0, line, maxCurrentLineNumber);

    const int delta = line - _currentLine;
    _currentLine = line;

    // keep track of number of lines scrolled by,
    // this can be reset by calling resetScrollCount()
    _scrollCount += delta;

    _bufferNeedsUpdate = true;

    emit scrolled(_currentLine);
}

void ScreenWindow::setTrackOutput(bool trackOutput)
{
    _trackOutput = trackOutput;
}

bool ScreenWindow::trackOutput() const
{
    return _trackOutput;
}

int ScreenWindow::scrollCount() const
{
    return _scrollCount;
}

void ScreenWindow::resetScrollCount()
{
    _scrollCount = 0;
}

QRect ScreenWindow::scrollRegion() const
{
    bool equalToScreenSize = windowLines() == _screen->getLines();

    if (atEndOfOutput() && equalToScreenSize) {
        return _screen->lastScrolledRegion();
    } else {
        return {0, 0, windowColumns(), windowLines()};
    }
}

void ScreenWindow::notifyOutputChanged()
{
    // move window to the bottom of the screen and update scroll count
    // if this window is currently tracking the bottom of the screen
    if (_trackOutput) {
        _scrollCount -= _screen->scrolledLines();
        _currentLine = qMax(0, _screen->getHistLines() - (windowLines() - _screen->getLines()));
    } else {
        // if the history is not unlimited then it may
        // have run out of space and dropped the oldest
        // lines of output - in this case the screen
        // window's current line number will need to
        // be adjusted - otherwise the output will scroll
        _currentLine = qMax(0, _currentLine
                            -_screen->droppedLines());

        // ensure that the screen window's current position does
        // not go beyond the bottom of the screen
        _currentLine = qMin(_currentLine, _screen->getHistLines());
    }

    _bufferNeedsUpdate = true;

    emit outputChanged();
}

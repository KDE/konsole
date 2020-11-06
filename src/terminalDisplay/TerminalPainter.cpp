/*
    Copyright 2020-2020 by Gustavo Carneiro <gcarneiroa@hotmail.com>
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

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
#include "TerminalPainter.hpp"

// Konsole
#include "TerminalDisplay.h"
#include "TerminalScrollBar.hpp"
#include "ExtendedCharTable.h"
#include "../characters/LineBlockCharacters.h"

// Qt
#include <QRect>
#include <QColor>
#include <QRegion>
#include <QPainter>
#include <QString>
#include <QVector>
#include <QChar>
#include <QMatrix>
#include <QTransform>
#include <QTimer>
#include <QPen>
#include <QDebug>

// we use this to force QPainter to display text in LTR mode
// more information can be found in: https://unicode.org/reports/tr9/
const QChar LTR_OVERRIDE_CHAR(0x202D);

namespace Konsole
{

    TerminalPainter::TerminalPainter(TerminalDisplay *display)
        : _display(display)
    {}

    static inline bool isLineCharString(const QString &string)
    {
        if (string.length() == 0) {
            return false;
        }
        return LineBlockCharacters::canDraw(string.at(0).unicode());
    }

    static int baseCodePoint(const Character &ch)
    {
        if (ch.rendition & RE_EXTENDED_CHAR) {
            ushort extendedCharLength = 0;
            const uint *chars = ExtendedCharTable::instance.lookupExtendedChar(ch.character, extendedCharLength);
            return chars[0];
        } else {
            return ch.character;
        }
    }

    void TerminalPainter::drawContents(QPainter &paint, const QRect &rect, bool printerFriendly)
    {
        const int numberOfColumns = _display->_usedColumns;
        QVector<uint> univec;
        univec.reserve(numberOfColumns);
        for (int y = rect.y(); y <= rect.bottom(); y++) {
            int x = rect.x();
            if ((_display->_image[_display->loc(rect.x(), y)].character == 0u) && (x != 0)) {
                x--; // Search for start of multi-column character
            }
            for (; x <= rect.right(); x++) {
                int len = 1;
                int p = 0;

                // reset our buffer to the number of columns
                int bufferSize = numberOfColumns;
                univec.resize(bufferSize);
                uint *disstrU = univec.data();

                // is this a single character or a sequence of characters ?
                if ((_display->_image[_display->loc(x, y)].rendition & RE_EXTENDED_CHAR) != 0) {
                    // sequence of characters
                    ushort extendedCharLength = 0;
                    const uint *chars = ExtendedCharTable::instance.lookupExtendedChar(_display->_image[_display->loc(x, y)].character, extendedCharLength);
                    if (chars != nullptr) {
                        Q_ASSERT(extendedCharLength > 1);
                        bufferSize += extendedCharLength - 1;
                        univec.resize(bufferSize);
                        disstrU = univec.data();
                        for (int index = 0; index < extendedCharLength; index++) {
                            Q_ASSERT(p < bufferSize);
                            disstrU[p++] = chars[index];
                        }
                    }
                } else {
                    const uint c = _display->_image[_display->loc(x, y)].character;
                    if (c != 0u) {
                        Q_ASSERT(p < bufferSize);
                        disstrU[p++] = c;
                    }
                }

                const bool lineDraw = LineBlockCharacters::canDraw(_display->_image[_display->loc(x, y)].character);
                const bool doubleWidth = (_display->_image[qMin(_display->loc(x, y) + 1, _display->_imageSize - 1)].character == 0);
                const CharacterColor currentForeground = _display->_image[_display->loc(x, y)].foregroundColor;
                const CharacterColor currentBackground = _display->_image[_display->loc(x, y)].backgroundColor;
                const RenditionFlags currentRendition = _display->_image[_display->loc(x, y)].rendition;
                const QChar::Script currentScript = QChar::script(baseCodePoint(_display->_image[_display->loc(x, y)]));

                const auto isInsideDrawArea = [&](int column) { return column <= rect.right(); };
                const auto hasSameColors = [&](int column) {
                    return _display->_image[_display->loc(column, y)].foregroundColor == currentForeground
                        && _display->_image[_display->loc(column, y)].backgroundColor == currentBackground;
                };
                const auto hasSameRendition = [&](int column) {
                    return (_display->_image[_display->loc(column, y)].rendition & ~RE_EXTENDED_CHAR)
                        == (currentRendition & ~RE_EXTENDED_CHAR);
                };
                const auto hasSameWidth = [&](int column) {
                    const int characterLoc = qMin(_display->loc(column, y) + 1, _display->_imageSize - 1);
                    return (_display->_image[characterLoc].character == 0) == doubleWidth;
                };
                const auto hasSameLineDrawStatus = [&](int column) {
                    return LineBlockCharacters::canDraw(_display->_image[_display->loc(column, y)].character)
                        == lineDraw;
                };
                const auto isSameScript = [&](int column) {
                    const QChar::Script script = QChar::script(baseCodePoint(_display->_image[_display->loc(column, y)]));
                    if (currentScript == QChar::Script_Common || script == QChar::Script_Common
                        || currentScript == QChar::Script_Inherited || script == QChar::Script_Inherited) {
                        return true;
                    }
                    return currentScript == script;
                };
                const auto canBeGrouped = [&](int column) {
                    return _display->_image[_display->loc(column, y)].character <= 0x7e
                            || (_display->_image[_display->loc(column, y)].rendition & RE_EXTENDED_CHAR)
                            || (_display->_bidiEnabled && !doubleWidth);
                };

                if (canBeGrouped(x)) {
                    while (isInsideDrawArea(x + len) && hasSameColors(x + len)
                            && hasSameRendition(x + len) && hasSameWidth(x + len)
                            && hasSameLineDrawStatus(x + len) && isSameScript(x + len)
                            && canBeGrouped(x + len)) {
                        const uint c = _display->_image[_display->loc(x + len, y)].character;
                        if ((_display->_image[_display->loc(x + len, y)].rendition & RE_EXTENDED_CHAR) != 0) {
                            // sequence of characters
                            ushort extendedCharLength = 0;
                            const uint *chars = ExtendedCharTable::instance.lookupExtendedChar(c, extendedCharLength);
                            if (chars != nullptr) {
                                Q_ASSERT(extendedCharLength > 1);
                                bufferSize += extendedCharLength - 1;
                                univec.resize(bufferSize);
                                disstrU = univec.data();
                                for (int index = 0; index < extendedCharLength; index++) {
                                    Q_ASSERT(p < bufferSize);
                                    disstrU[p++] = chars[index];
                                }
                            }
                        } else {
                            // single character
                            if (c != 0u) {
                                Q_ASSERT(p < bufferSize);
                                disstrU[p++] = c;
                            }
                        }

                        if (doubleWidth) {
                            len++;
                        }
                        len++;
                    }
                } else {
                    // Group spaces following any non-wide character with the character. This allows for
                    // rendering ambiguous characters with wide glyphs without clipping them.
                    while (!doubleWidth && isInsideDrawArea(x + len)
                            && _display->_image[_display->loc(x + len, y)].character == ' ' && hasSameColors(x + len)
                            && hasSameRendition(x + len)) {
                        // disstrU intentionally not modified - trailing spaces are meaningless
                        len++;
                    }
                }
                if ((x + len < _display->_usedColumns) && (_display->_image[_display->loc(x + len, y)].character == 0u)) {
                    len++; // Adjust for trailing part of multi-column character
                }

                const bool save__fixedFont = _display->_fixedFont;
                if (lineDraw) {
                    _display->_fixedFont = false;
                }
                if (doubleWidth) {
                    _display->_fixedFont = false;
                }
                univec.resize(p);

                QMatrix textScale;

                if (y < _display->_lineProperties.size()) {
                    if ((_display->_lineProperties[y] & LINE_DOUBLEWIDTH) != 0) {
                        textScale.scale(2, 1);
                    }

                    if ((_display->_lineProperties[y] & LINE_DOUBLEHEIGHT) != 0) {
                        textScale.scale(1, 2);
                    }
                }

                // Apply text scaling matrix
                paint.setWorldTransform(QTransform(textScale), true);

                // Calculate the area in which the text will be drawn
                QRect textArea = QRect(_display->contentRect().left() + _display->contentsRect().left() + _display->fontWidth() * x,
                                        _display->contentRect().top() + _display->contentsRect().top() + _display->fontHeight() * y,
                                        _display->fontWidth() * len,
                                        _display->fontHeight());

                //move the calculated area to take account of scaling applied to the painter.
                //the position of the area from the origin (0,0) is scaled
                //by the opposite of whatever
                //transformation has been applied to the painter.  this ensures that
                //painting does actually start from textArea.topLeft()
                //(instead of textArea.topLeft() * painter-scale)
                textArea.moveTopLeft(textScale.inverted().map(textArea.topLeft()));

                QString unistr = QString::fromUcs4(univec.data(), univec.length());

                // paint text fragment
                if (printerFriendly) {
                    drawPrinterFriendlyTextFragment(paint,
                                                    textArea,
                                                    unistr,
                                                    &_display->_image[_display->loc(x, y)]);    
                } else {
                    drawTextFragment(paint,
                                    textArea,
                                    unistr,
                                    &_display->_image[_display->loc(x, y)]);
                }

                _display->_fixedFont = save__fixedFont;

                paint.setWorldTransform(QTransform(textScale.inverted()), true);

                if (y < _display->_lineProperties.size() - 1) {

                    if ((_display->_lineProperties[y] & LINE_DOUBLEHEIGHT) != 0) {
                        y++;
                    }
                }

                x += len - 1;
            }
        }
    }
    
    void TerminalPainter::drawCurrentResultRect(QPainter &painter)
    {
        if (_display->_screenWindow->currentResultLine() == -1) {
            return;
        }
        
        _display->_searchResultRect.setRect(0, _display->contentRect().top() + (_display->_screenWindow->currentResultLine() - _display->_screenWindow->currentLine()) * _display->fontHeight(),
            _display->columns() * _display->fontWidth(), _display->fontHeight());
        painter.fillRect(_display->_searchResultRect, QColor(0, 0, 255, 80));
    }
    
    void TerminalPainter::highlightScrolledLines(QPainter& painter) 
    {
        if (!_display->_highlightScrolledLinesControl.enabled) {
            return;
        }

        QColor color = QColor(_display->_colorTable[Color4Index]);
        color.setAlpha(_display->_highlightScrolledLinesControl.timer->isActive() ? 255 : 150);
        painter.fillRect(_display->_highlightScrolledLinesControl.rect, color);
    }
    
    QRegion TerminalPainter::highlightScrolledLinesRegion(bool nothingChanged) 
    {
        QRegion dirtyRegion;
        const int highlightLeftPosition = _display->scrollBar()->scrollBarPosition() == Enum::ScrollBarLeft ? _display->scrollBar()->width() : 0;

        int start = 0;
        int nb_lines = abs(_display->_screenWindow->scrollCount());
        if (nb_lines > 0 && _display->scrollBar()->maximum() > 0) {
            QRect new_highlight;
            bool addToCurrentHighlight = _display->_highlightScrolledLinesControl.timer->isActive() &&
                                     (_display->_screenWindow->scrollCount() * _display->_highlightScrolledLinesControl.previousScrollCount > 0);
            if (addToCurrentHighlight) {
                if (_display->_screenWindow->scrollCount() > 0) {
                    start = -1 * (_display->_highlightScrolledLinesControl.previousScrollCount + _display->screenWindow()->scrollCount()) + _display->screenWindow()->windowLines();
                } else {
                    start = -1 * _display->_highlightScrolledLinesControl.previousScrollCount;
                }
                _display->_highlightScrolledLinesControl.previousScrollCount += _display->_screenWindow->scrollCount();
            } else {
                start = _display->_screenWindow->scrollCount() > 0 ? _display->_screenWindow->windowLines() - nb_lines : 0;
                _display->_highlightScrolledLinesControl.previousScrollCount = _display->_screenWindow->scrollCount();
            }

            new_highlight.setRect(highlightLeftPosition, _display->contentRect().top() + start * _display->fontHeight(), _display->HIGHLIGHT_SCROLLED_LINES_WIDTH, nb_lines * _display->fontHeight());
            new_highlight.setTop(std::max(new_highlight.top(), _display->contentRect().top()));
            new_highlight.setBottom(std::min(new_highlight.bottom(), _display->contentRect().bottom()));
            if (!new_highlight.isValid()) {
                new_highlight = QRect(0, 0, 0, 0);
            }

            if (addToCurrentHighlight) {
                _display->_highlightScrolledLinesControl.rect |= new_highlight;
            } else {
                dirtyRegion |= _display->_highlightScrolledLinesControl.rect;
                _display->_highlightScrolledLinesControl.rect = new_highlight;
            }

            _display->_highlightScrolledLinesControl.timer->start();
        } else if (!nothingChanged || _display->_highlightScrolledLinesControl.needToClear) {
            dirtyRegion = _display->_highlightScrolledLinesControl.rect;
            _display->_highlightScrolledLinesControl.rect.setRect(0, 0, 0, 0);
            _display->_highlightScrolledLinesControl.needToClear = false;
        }

        return dirtyRegion;
    }
    
    void TerminalPainter::drawTextFragment(QPainter &painter, const QRect &rect, const QString &text,
                                const Character *style) 
    {
        // setup painter
        const QColor foregroundColor = style->foregroundColor.color(_display->_colorTable);
        const QColor backgroundColor = style->backgroundColor.color(_display->_colorTable);

        if (backgroundColor != _display->_colorTable[DEFAULT_BACK_COLOR]) {
            drawBackground(painter, rect, backgroundColor, false);
        }

        QColor characterColor;
        if ((style->rendition & RE_CURSOR) != 0) {
            drawCursor(painter, rect, foregroundColor, backgroundColor, characterColor);
        }

        // draw text
        drawCharacters(painter, rect, text, style, characterColor);
    }
    
    void TerminalPainter::drawPrinterFriendlyTextFragment(QPainter &painter, const QRect &rect, const QString &text,
                                                const Character *style) 
    {
        Character print_style = *style;
        print_style.foregroundColor = CharacterColor(COLOR_SPACE_RGB, 0x00000000);
        print_style.backgroundColor = CharacterColor(COLOR_SPACE_RGB, 0xFFFFFFFF);

        drawCharacters(painter, rect, text, &print_style, QColor());
    }
    
    void TerminalPainter::drawBackground(QPainter &painter, const QRect &rect, const QColor &backgroundColor,
                                bool useOpacitySetting) 
    {
        if (useOpacitySetting && !_display->_wallpaper->isNull() &&
                _display->_wallpaper->draw(painter, rect, _display->_opacity)) {
        } else if (qAlpha(_display->_blendColor) < 0xff && useOpacitySetting) {
#if defined(Q_OS_MACOS)
            // TODO: On MacOS, using CompositionMode doesn't work. Altering the
            //       transparency in the color scheme alters the brightness.
            painter.fillRect(rect, backgroundColor);
#else
            QColor color(backgroundColor);
            color.setAlpha(qAlpha(_display->_blendColor));

            const QPainter::CompositionMode originalMode = painter.compositionMode();
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.fillRect(rect, color);
            painter.setCompositionMode(originalMode);
#endif
        } else {
            painter.fillRect(rect, backgroundColor);
        }
    }
    
    void TerminalPainter::drawCursor(QPainter &painter, const QRect &rect, const QColor &foregroundColor,
                            const QColor &backgroundColor, QColor &characterColor) 
    {
        if (_display->_cursorBlinking) {
            return;
        }

        QRectF cursorRect = rect.adjusted(0, 1, 0, 0);

        QColor cursorColor = _display->_cursorColor.isValid() ? _display->_cursorColor : foregroundColor;
        QPen pen(cursorColor);
        // TODO: the relative pen width to draw the cursor is a bit hacky
        // and set to 1/12 of the font width. Visually it seems to work at
        // all scales but there must be better ways to do it
        const qreal width = qMax(_display->fontWidth() / 12.0, 1.0);
        const qreal halfWidth = width / 2.0;
        pen.setWidthF(width);
        painter.setPen(pen);

        if (_display->_cursorShape == Enum::BlockCursor) {
            painter.drawRect(cursorRect.adjusted(halfWidth, halfWidth, -halfWidth, -halfWidth));

            if (_display->hasFocus()) {
                painter.fillRect(cursorRect, cursorColor);

                characterColor = _display->_cursorTextColor.isValid() ? _display->_cursorTextColor : backgroundColor;
            }
        } else if (_display->_cursorShape == Enum::UnderlineCursor) {
            QLineF line(cursorRect.left() + halfWidth,
                        cursorRect.bottom() - halfWidth,
                        cursorRect.right() - halfWidth,
                        cursorRect.bottom() - halfWidth);
            painter.drawLine(line);

        } else if (_display->_cursorShape == Enum::IBeamCursor) {
            QLineF line(cursorRect.left() + halfWidth,
                        cursorRect.top() + halfWidth,
                        cursorRect.left() + halfWidth,
                        cursorRect.bottom() - halfWidth);
            painter.drawLine(line);
        }
    }
    
    void TerminalPainter::drawCharacters(QPainter &painter, const QRect &rect, const QString &text,
                                const Character *style, const QColor &characterColor) 
    {
        if (_display->_textBlinking && ((style->rendition & RE_BLINK) != 0)) {
            return;
        }

        if ((style->rendition & RE_CONCEAL) != 0) {
            return;
        }

        static constexpr int MaxFontWeight = 99;
        
        const int normalWeight = _display->font().weight();

        const int boldWeight = qMin(normalWeight + 26, MaxFontWeight);

        const auto isBold = [boldWeight](const QFont &font) { return font.weight() >= boldWeight; };

        const bool useBold = (((style->rendition & RE_BOLD) != 0) && _display->_boldIntense);
        const bool useUnderline = ((style->rendition & RE_UNDERLINE) != 0) || _display->font().underline();
        const bool useItalic = ((style->rendition & RE_ITALIC) != 0) || _display->font().italic();
        const bool useStrikeOut = ((style->rendition & RE_STRIKEOUT) != 0) || _display->font().strikeOut();
        const bool useOverline = ((style->rendition & RE_OVERLINE) != 0) || _display->font().overline();

        QFont currentFont = painter.font();

        if (isBold(currentFont) != useBold
                || currentFont.underline() != useUnderline
                || currentFont.italic() != useItalic
                || currentFont.strikeOut() != useStrikeOut
                || currentFont.overline() != useOverline) {

            currentFont.setWeight(useBold ? boldWeight : normalWeight);
            currentFont.setUnderline(useUnderline);
            currentFont.setItalic(useItalic);
            currentFont.setStrikeOut(useStrikeOut);
            currentFont.setOverline(useOverline);
            painter.setFont(currentFont);
        }

        // setup pen
        const QColor foregroundColor = style->foregroundColor.color(_display->_colorTable);
        const QColor color = characterColor.isValid() ? characterColor : foregroundColor;
        QPen pen = painter.pen();
        if (pen.color() != color) {
            pen.setColor(color);
            painter.setPen(color);
        }

        const bool origClipping = painter.hasClipping();
        const auto origClipRegion = painter.clipRegion();
        painter.setClipRect(rect);
        // draw text
        if (isLineCharString(text) && !_display->_useFontLineCharacters) {
            drawLineCharString(painter, rect.x(), rect.y(), text, style);
        } else {
            painter.setLayoutDirection(Qt::LeftToRight);

            if (_display->_bidiEnabled) {
                painter.drawText(rect.x(), rect.y() + _display->_fontAscent + _display->lineSpacing(), text);
            } else {
                painter.drawText(rect.x(), rect.y() + _display->_fontAscent + _display->lineSpacing(), LTR_OVERRIDE_CHAR + text);
            }
        }
        painter.setClipRegion(origClipRegion);
        painter.setClipping(origClipping);
    }

    void TerminalPainter::drawLineCharString(QPainter &painter, int x, int y, const QString &str,
                                    const Character *attributes)
    {
        painter.setRenderHint(QPainter::Antialiasing, _display->_antialiasText);

        const bool useBoldPen = (attributes->rendition & RE_BOLD) != 0 && _display->_boldIntense;

        QRect cellRect = {x, y, _display->fontWidth(), _display->fontHeight()};
        for (int i = 0; i < str.length(); i++) {
            LineBlockCharacters::draw(painter, cellRect.translated(i * _display->fontWidth(), 0), str[i], useBoldPen);
        }
        painter.setRenderHint(QPainter::Antialiasing, false);
    }

    void TerminalPainter::drawInputMethodPreeditString(QPainter &painter, const QRect &rect)
    {
        if (_display->_inputMethodData.preeditString.isEmpty() || !_display->isCursorOnDisplay()) {
            return;
        }

        const QPoint cursorPos = _display->cursorPosition();

        QColor characterColor;
        const QColor background = _display->_colorTable[DEFAULT_BACK_COLOR];
        const QColor foreground = _display->_colorTable[DEFAULT_FORE_COLOR];
        const Character *style = &_display->_image[_display->loc(cursorPos.x(), cursorPos.y())];

        drawBackground(painter, rect, background, true);
        drawCursor(painter, rect, foreground, background, characterColor);
        drawCharacters(painter, rect, _display->_inputMethodData.preeditString, style, characterColor);

        _display->_inputMethodData.previousPreeditRect = rect;
    }

}

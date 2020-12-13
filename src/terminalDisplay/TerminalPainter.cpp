/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "TerminalPainter.h"

// Konsole
#include "TerminalScrollBar.h"
#include "../characters/ExtendedCharTable.h"
#include "../characters/LineBlockCharacters.h"
#include "TerminalColor.h"

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

    TerminalPainter::TerminalPainter(QObject *parent)
        : QObject(parent)
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
            // FIXME: Coverity-Dereferencing chars, which is known to be nullptr
            return chars[0];
        } else {
            return ch.character;
        }
    }

    void TerminalPainter::drawContents(Character *image, QPainter &paint, const QRect &rect, bool printerFriendly, int imageSize, bool bidiEnabled, bool &fixedFont,
            QVector<LineProperty> lineProperties)
    {
        const auto display = qobject_cast<TerminalDisplay*>(sender());

        const int numberOfColumns = display->usedColumns();
        QVector<uint> univec;
        univec.reserve(numberOfColumns);
        for (int y = rect.y(); y <= rect.bottom(); y++) {
            int x = rect.x();
            if ((image[display->loc(rect.x(), y)].character == 0u) && (x != 0)) {
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
                if ((image[display->loc(x, y)].rendition & RE_EXTENDED_CHAR) != 0) {
                    // sequence of characters
                    ushort extendedCharLength = 0;
                    const uint *chars = ExtendedCharTable::instance.lookupExtendedChar(image[display->loc(x, y)].character, extendedCharLength);
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
                    const uint c = image[display->loc(x, y)].character;
                    if (c != 0u) {
                        Q_ASSERT(p < bufferSize);
                        disstrU[p++] = c;
                    }
                }

                const bool lineDraw = LineBlockCharacters::canDraw(image[display->loc(x, y)].character);
                const bool doubleWidth = (image[qMin(display->loc(x, y) + 1, imageSize - 1)].character == 0);
                const CharacterColor currentForeground = image[display->loc(x, y)].foregroundColor;
                const CharacterColor currentBackground = image[display->loc(x, y)].backgroundColor;
                const RenditionFlags currentRendition = image[display->loc(x, y)].rendition;
                const QChar::Script currentScript = QChar::script(baseCodePoint(image[display->loc(x, y)]));

                const auto isInsideDrawArea = [&](int column) { return column <= rect.right(); };
                const auto hasSameColors = [&](int column) {
                    return image[display->loc(column, y)].foregroundColor == currentForeground
                        && image[display->loc(column, y)].backgroundColor == currentBackground;
                };
                const auto hasSameRendition = [&](int column) {
                    return (image[display->loc(column, y)].rendition & ~RE_EXTENDED_CHAR)
                        == (currentRendition & ~RE_EXTENDED_CHAR);
                };
                const auto hasSameWidth = [&](int column) {
                    const int characterLoc = qMin(display->loc(column, y) + 1, imageSize - 1);
                    return (image[characterLoc].character == 0) == doubleWidth;
                };
                const auto hasSameLineDrawStatus = [&](int column) {
                    return LineBlockCharacters::canDraw(image[display->loc(column, y)].character)
                        == lineDraw;
                };
                const auto isSameScript = [&](int column) {
                    const QChar::Script script = QChar::script(baseCodePoint(image[display->loc(column, y)]));
                    if (currentScript == QChar::Script_Common || script == QChar::Script_Common
                        || currentScript == QChar::Script_Inherited || script == QChar::Script_Inherited) {
                        return true;
                    }
                    return currentScript == script;
                };
                const auto canBeGrouped = [&](int column) {
                    return image[display->loc(column, y)].character <= 0x7e
                            || (image[display->loc(column, y)].rendition & RE_EXTENDED_CHAR)
                            || (bidiEnabled && !doubleWidth);
                };

                if (canBeGrouped(x)) {
                    while (isInsideDrawArea(x + len) && hasSameColors(x + len)
                            && hasSameRendition(x + len) && hasSameWidth(x + len)
                            && hasSameLineDrawStatus(x + len) && isSameScript(x + len)
                            && canBeGrouped(x + len)) {
                        const uint c = image[display->loc(x + len, y)].character;
                        if ((image[display->loc(x + len, y)].rendition & RE_EXTENDED_CHAR) != 0) {
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
                            && image[display->loc(x + len, y)].character == ' ' && hasSameColors(x + len)
                            && hasSameRendition(x + len)) {
                        // disstrU intentionally not modified - trailing spaces are meaningless
                        len++;
                    }
                }
                if ((x + len < display->usedColumns()) && (image[display->loc(x + len, y)].character == 0u)) {
                    len++; // Adjust for trailing part of multi-column character
                }

                const bool save__fixedFont = fixedFont;
                if (lineDraw) {
                    fixedFont = false;
                }
                if (doubleWidth) {
                    fixedFont = false;
                }
                univec.resize(p);

                QMatrix textScale;

                if (y < lineProperties.size()) {
                    if ((lineProperties[y] & LINE_DOUBLEWIDTH) != 0) {
                        textScale.scale(2, 1);
                    }

                    if ((lineProperties[y] & LINE_DOUBLEHEIGHT) != 0) {
                        textScale.scale(1, 2);
                    }
                }

                // Apply text scaling matrix
                paint.setWorldTransform(QTransform(textScale), true);

                // Calculate the area in which the text will be drawn
                QRect textArea = QRect(display->contentRect().left() + display->contentsRect().left() + display->fontWidth() * x,
                                        display->contentRect().top() + display->contentsRect().top() + display->fontHeight() * y,
                                        display->fontWidth() * len,
                                        display->fontHeight());

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
                                                    &image[display->loc(x, y)]);
                } else {
                    drawTextFragment(paint,
                                    textArea,
                                    unistr,
                                    &image[display->loc(x, y)],
                                    display->terminalColor()->colorTable());
                }

                fixedFont = save__fixedFont;

                paint.setWorldTransform(QTransform(textScale.inverted()), true);

                if (y < lineProperties.size() - 1) {

                    if ((lineProperties[y] & LINE_DOUBLEHEIGHT) != 0) {
                        y++;
                    }
                }

                x += len - 1;
            }
        }
    }
    
    void TerminalPainter::drawCurrentResultRect(QPainter &painter, QRect searchResultRect)
    {
        const auto display = qobject_cast<TerminalDisplay*>(sender());

        if (display->screenWindow()->currentResultLine() == -1) {
            return;
        }
        
        searchResultRect.setRect(0, display->contentRect().top() + (display->screenWindow()->currentResultLine() - display->screenWindow()->currentLine()) * display->fontHeight(),
            display->columns() * display->fontWidth(), display->fontHeight());
        painter.fillRect(searchResultRect, QColor(0, 0, 255, 80));
    }
    
    void TerminalPainter::highlightScrolledLines(QPainter& painter, QTimer *timer, QRect rect) 
    {
        const auto display = qobject_cast<TerminalDisplay*>(sender());

        QColor color = QColor(display->terminalColor()->colorTable()[Color4Index]);
        color.setAlpha(timer->isActive() ? 255 : 150);
        painter.fillRect(rect, color);
    }
    
    QRegion TerminalPainter::highlightScrolledLinesRegion(bool nothingChanged, QTimer *timer, int &previousScrollCount, QRect &rect, bool &needToClear, int HighlightScrolledLinesWidth)
    {
        const auto display = qobject_cast<TerminalDisplay*>(sender());

        QRegion dirtyRegion;
        const int highlightLeftPosition = display->scrollBar()->scrollBarPosition() == Enum::ScrollBarLeft ? display->scrollBar()->width() : 0;

        int start = 0;
        int nb_lines = abs(display->screenWindow()->scrollCount());
        if (nb_lines > 0 && display->scrollBar()->maximum() > 0) {
            QRect new_highlight;
            bool addToCurrentHighlight = timer->isActive() &&
                                     (display->screenWindow()->scrollCount() * previousScrollCount > 0);
            if (addToCurrentHighlight) {
                if (display->screenWindow()->scrollCount() > 0) {
                    start = -1 * (previousScrollCount + display->screenWindow()->scrollCount()) + display->screenWindow()->windowLines();
                } else {
                    start = -1 * previousScrollCount;
                }
                previousScrollCount += display->screenWindow()->scrollCount();
            } else {
                start = display->screenWindow()->scrollCount() > 0 ? display->screenWindow()->windowLines() - nb_lines : 0;
                previousScrollCount = display->screenWindow()->scrollCount();
            }

            new_highlight.setRect(highlightLeftPosition, display->contentRect().top() + start * display->fontHeight(), HighlightScrolledLinesWidth, nb_lines * display->fontHeight());
            new_highlight.setTop(std::max(new_highlight.top(), display->contentRect().top()));
            new_highlight.setBottom(std::min(new_highlight.bottom(), display->contentRect().bottom()));
            if (!new_highlight.isValid()) {
                new_highlight = QRect(0, 0, 0, 0);
            }

            if (addToCurrentHighlight) {
                rect |= new_highlight;
            } else {
                dirtyRegion |= rect;
                rect = new_highlight;
            }

            timer->start();
        } else if (!nothingChanged || needToClear) {
            dirtyRegion = rect;
            rect.setRect(0, 0, 0, 0);
            needToClear = false;
        }

        return dirtyRegion;
    }
    
    void TerminalPainter::drawTextFragment(QPainter &painter, const QRect &rect, const QString &text,
                                const Character *style, const ColorEntry *colorTable)
    {
        // setup painter
        const QColor foregroundColor = style->foregroundColor.color(colorTable);
        const QColor backgroundColor = style->backgroundColor.color(colorTable);

        if (backgroundColor != colorTable[DEFAULT_BACK_COLOR]) {
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
        const auto display = qobject_cast<TerminalDisplay*>(sender());

        if (useOpacitySetting && !display->wallpaper()->isNull() &&
               display->wallpaper()->draw(painter, rect, display->terminalColor()->opacity())) {
        } else if (qAlpha(display->terminalColor()->blendColor()) < 0xff && useOpacitySetting) {
#if defined(Q_OS_MACOS)
            // TODO: On MacOS, using CompositionMode doesn't work. Altering the
            //       transparency in the color scheme alters the brightness.
            painter.fillRect(rect, backgroundColor);
#else
            QColor color(backgroundColor);
            color.setAlpha(qAlpha(display->terminalColor()->blendColor()));

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
        const auto display = qobject_cast<TerminalDisplay*>(sender());

        if (display->cursorBlinking()) {
            return;
        }

        QRectF cursorRect = rect.adjusted(0, 1, 0, 0);

        QColor color = display->terminalColor()->cursorColor();
        QColor cursorColor = color.isValid() ? color : foregroundColor;

        QPen pen(cursorColor);
        // TODO: the relative pen width to draw the cursor is a bit hacky
        // and set to 1/12 of the font width. Visually it seems to work at
        // all scales but there must be better ways to do it
        const qreal width = qMax(display->fontWidth() / 12.0, 1.0);
        const qreal halfWidth = width / 2.0;
        pen.setWidthF(width);
        painter.setPen(pen);

        if (display->cursorShape() == Enum::BlockCursor) {
            painter.drawRect(cursorRect.adjusted(halfWidth, halfWidth, -halfWidth, -halfWidth));

            if (display->hasFocus()) {
                painter.fillRect(cursorRect, cursorColor);

                QColor color = display->terminalColor()->cursorTextColor();
                characterColor = color.isValid() ? color : backgroundColor;
            }
        } else if (display->cursorShape() == Enum::UnderlineCursor) {
            QLineF line(cursorRect.left() + halfWidth,
                        cursorRect.bottom() - halfWidth,
                        cursorRect.right() - halfWidth,
                        cursorRect.bottom() - halfWidth);
            painter.drawLine(line);

        } else if (display->cursorShape() == Enum::IBeamCursor) {
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
        const auto display = qobject_cast<TerminalDisplay*>(sender());

        if (display->textBlinking() && ((style->rendition & RE_BLINK) != 0)) {
            return;
        }

        if ((style->rendition & RE_CONCEAL) != 0) {
            return;
        }

        static constexpr int MaxFontWeight = 99;
        
        const int normalWeight = display->font().weight();

        const int boldWeight = qMin(normalWeight + 26, MaxFontWeight);

        const auto isBold = [boldWeight](const QFont &font) { return font.weight() >= boldWeight; };

        const bool useBold = (((style->rendition & RE_BOLD) != 0) && display->boldIntense());
        const bool useUnderline = ((style->rendition & RE_UNDERLINE) != 0) || display->font().underline();
        const bool useItalic = ((style->rendition & RE_ITALIC) != 0) || display->font().italic();
        const bool useStrikeOut = ((style->rendition & RE_STRIKEOUT) != 0) || display->font().strikeOut();
        const bool useOverline = ((style->rendition & RE_OVERLINE) != 0) || display->font().overline();

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
        const QColor foregroundColor = style->foregroundColor.color(display->terminalColor()->colorTable());
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
        if (isLineCharString(text) && !display->useFontLineCharacters()) {
            drawLineCharString(display, painter, rect.x(), rect.y(), text, style);
        } else {
            painter.setLayoutDirection(Qt::LeftToRight);

            if (display->bidiEnabled()) {
                painter.drawText(rect.x(), rect.y() + display->fontAscent() + display->lineSpacing(), text);
            } else {
                painter.drawText(rect.x(), rect.y() + display->fontAscent() + display->lineSpacing(), LTR_OVERRIDE_CHAR + text);
            }
        }
        painter.setClipRegion(origClipRegion);
        painter.setClipping(origClipping);
    }

    void TerminalPainter::drawLineCharString(TerminalDisplay *display, QPainter &painter, int x, int y, const QString &str, const Character *attributes)
    {
        painter.setRenderHint(QPainter::Antialiasing, display->antialiasText());

        const bool useBoldPen = (attributes->rendition & RE_BOLD) != 0 && display->boldIntense();

        QRect cellRect = {x, y, display->fontWidth(), display->fontHeight()};
        for (int i = 0; i < str.length(); i++) {
            LineBlockCharacters::draw(painter, cellRect.translated(i * display->fontWidth(), 0), str[i], useBoldPen);
        }
        painter.setRenderHint(QPainter::Antialiasing, false);
    }

    void TerminalPainter::drawInputMethodPreeditString(QPainter &painter, const QRect &rect, TerminalDisplay::InputMethodData &inputMethodData, Character *image)
    {
        const auto display = qobject_cast<TerminalDisplay*>(sender());

        if (inputMethodData.preeditString.isEmpty() || !display->isCursorOnDisplay()) {
            return;
        }

        const QPoint cursorPos = display->cursorPosition();

        QColor characterColor;
        const QColor background = display->terminalColor()->colorTable()[DEFAULT_BACK_COLOR];
        const QColor foreground = display->terminalColor()->colorTable()[DEFAULT_FORE_COLOR];
        const Character *style = &image[display->loc(cursorPos.x(), cursorPos.y())];

        drawBackground(painter, rect, background, true);
        drawCursor(painter, rect, foreground, background, characterColor);
        drawCharacters(painter, rect, inputMethodData.preeditString, style, characterColor);

        inputMethodData.previousPreeditRect = rect;
    }

}

/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "TerminalPainter.h"

// Konsole
#include "../Screen.h"
#include "../characters/ExtendedCharTable.h"
#include "../characters/LineBlockCharacters.h"
#include "../session/SessionManager.h"
#include "TerminalColor.h"
#include "TerminalFonts.h"
#include "TerminalScrollBar.h"

// Qt
#include <QChar>
#include <QColor>
#include <QDebug>
#include <QMatrix>
#include <QPainter>
#include <QPen>
#include <QRect>
#include <QRegion>
#include <QString>
#include <QTransform>
#include <QtMath>

#include <optional>

// we use this to force QPainter to display text in LTR mode
// more information can be found in: https://unicode.org/reports/tr9/
const QChar LTR_OVERRIDE_CHAR(0x202D);

namespace Konsole
{
TerminalPainter::TerminalPainter(QObject *parent)
    : QObject(parent)
{
}

static inline bool isLineCharString(const QString &string)
{
    if (string.length() == 0) {
        return false;
    }
    if (LineBlockCharacters::canDraw(string.at(0).unicode())) {
        return true;
    }
    if (string.length() <= 1 || !string[0].isSurrogate()) {
        return false;
    }
    uint ucs4;
    if (string[0].isHighSurrogate()) {
        ucs4 = QChar::surrogateToUcs4(string[0], string[1]);
    } else {
        ucs4 = QChar::surrogateToUcs4(string[1], string[0]);
    }
    return LineBlockCharacters::isLegacyComputingSymbol(ucs4);
}

bool isInvertedRendition(const TerminalDisplay *display)
{
    auto currentProfile = SessionManager::instance()->sessionProfile(display->session());
    const bool isInvert = currentProfile ? currentProfile->property<bool>(Profile::InvertSelectionColors) : false;

    return isInvert;
}

void TerminalPainter::drawContents(Character *image,
                                   QPainter &paint,
                                   const QRect &rect,
                                   bool printerFriendly,
                                   int imageSize,
                                   bool bidiEnabled,
                                   QVector<LineProperty> lineProperties)
{
    const auto display = qobject_cast<TerminalDisplay *>(sender());
    const bool invertedRendition = isInvertedRendition(display);

    QVector<uint> univec;
    univec.reserve(display->usedColumns());

    for (int y = rect.y(); y <= rect.bottom(); y++) {
        int x = rect.x();
        bool doubleHeightLinePair = false;

        // Search for start of multi-column character
        if (image[display->loc(rect.x(), y)].isRightHalfOfDoubleWide() && (x != 0)) {
            x--;
        }

        for (; x <= rect.right(); x++) {
            int len = 1;
            const int pos = display->loc(x, y);
            const Character char_value = image[pos];

            // is this a single character or a sequence of characters ?
            if ((char_value.rendition & RE_EXTENDED_CHAR) != 0) {
                // sequence of characters
                ushort extendedCharLength = 0;
                const uint *chars = ExtendedCharTable::instance.lookupExtendedChar(char_value.character, extendedCharLength);
                if (chars != nullptr) {
                    Q_ASSERT(extendedCharLength > 1);
                    for (int index = 0; index < extendedCharLength; index++) {
                        univec << chars[index];
                    }
                }
            } else {
                if (!char_value.isRightHalfOfDoubleWide()) {
                    univec << char_value.character;
                }
            }

            // TODO: Move all those lambdas to Character, so it's easy to test.
            const bool doubleWidth = image[qMin(pos + 1, imageSize - 1)].isRightHalfOfDoubleWide();

            const auto isInsideDrawArea = [rectRight = rect.right()](int column) {
                return column <= rectRight;
            };

            const auto hasSameWidth = [imageSize, image, doubleWidth](int nextPos) {
                const int characterLoc = qMin(nextPos + 1, imageSize - 1);
                return image[characterLoc].isRightHalfOfDoubleWide() == doubleWidth;
            };

            if (char_value.canBeGrouped(bidiEnabled, doubleWidth)) {
                while (isInsideDrawArea(x + len)) {
                    const int nextPos = display->loc(x + len, y);
                    const Character next_char = image[nextPos];

                    if (!hasSameWidth(nextPos) || !next_char.canBeGrouped(bidiEnabled, doubleWidth) || !char_value.hasSameAttributes(next_char)) {
                        break;
                    }

                    const uint c = next_char.character;
                    if ((next_char.rendition & RE_EXTENDED_CHAR) != 0) {
                        // sequence of characters
                        ushort extendedCharLength = 0;
                        const uint *chars = ExtendedCharTable::instance.lookupExtendedChar(c, extendedCharLength);
                        if (chars != nullptr) {
                            Q_ASSERT(extendedCharLength > 1);
                            for (int index = 0; index < extendedCharLength; index++) {
                                univec << chars[index];
                            }
                        }
                    } else {
                        // single character
                        if (c != 0u) {
                            univec << c;
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
                while (!doubleWidth && isInsideDrawArea(x + len)) {
                    const Character next_char = image[display->loc(x + len, y)];
                    if (next_char.character == ' ' && char_value.hasSameColors(next_char) && char_value.hasSameRendition(next_char)) {
                        // univec intentionally not modified - trailing spaces are meaningless
                        len++;
                    } else {
                        // break otherwise, we don't want to be stuck in this loop
                        break;
                    }
                }
            }

            // Adjust for trailing part of multi-column character
            if ((x + len < display->usedColumns()) && image[display->loc(x + len, y)].isRightHalfOfDoubleWide()) {
                len++;
            }

            QMatrix textScale;
            bool doubleHeight = false;
            bool doubleWidthLine = false;

            if (y < lineProperties.size()) {
                if ((lineProperties[y] & LINE_DOUBLEWIDTH) != 0) {
                    textScale.scale(2, 1);
                    doubleWidthLine = true;
                }

                doubleHeight = lineProperties[y] & (LINE_DOUBLEHEIGHT_TOP | LINE_DOUBLEHEIGHT_BOTTOM);
                if (doubleHeight) {
                    textScale.scale(1, 2);
                }
            }

            if (y < lineProperties.size() - 1) {
                if (((lineProperties[y] & LINE_DOUBLEHEIGHT_TOP) != 0) && ((lineProperties[y + 1] & LINE_DOUBLEHEIGHT_BOTTOM) != 0)) {
                    doubleHeightLinePair = true;
                }
            }

            // Apply text scaling matrix
            paint.setWorldTransform(QTransform(textScale), true);

            // Calculate the area in which the text will be drawn
            QRect textArea =
                QRect(display->contentRect().left() + display->contentsRect().left() + display->terminalFont()->fontWidth() * x * (doubleWidthLine ? 2 : 1),
                      display->contentRect().top() + display->contentsRect().top() + display->terminalFont()->fontHeight() * y,
                      display->terminalFont()->fontWidth() * len,
                      doubleHeight && !doubleHeightLinePair ? display->terminalFont()->fontHeight() / 2 : display->terminalFont()->fontHeight());

            // move the calculated area to take account of scaling applied to the painter.
            // the position of the area from the origin (0,0) is scaled
            // by the opposite of whatever
            // transformation has been applied to the painter.  this ensures that
            // painting does actually start from textArea.topLeft()
            //(instead of textArea.topLeft() * painter-scale)
            textArea.moveTopLeft(textScale.inverted().map(textArea.topLeft()));

            QString unistr = QString::fromUcs4(univec.data(), univec.length());

            univec.clear();

            // paint text fragment
            if (printerFriendly) {
                drawPrinterFriendlyTextFragment(paint, textArea, unistr, image[pos], y < lineProperties.size() ? lineProperties[y] : 0);
            } else {
                drawTextFragment(paint,
                                 textArea,
                                 unistr,
                                 image[pos],
                                 display->terminalColor()->colorTable(),
                                 invertedRendition,
                                 y < lineProperties.size() ? lineProperties[y] : 0);
            }

            paint.setWorldTransform(QTransform(textScale.inverted()), true);

            x += len - 1;
        }

        if (doubleHeightLinePair) {
            y++;
        }
    }
}

void TerminalPainter::drawCurrentResultRect(QPainter &painter, const QRect &searchResultRect)
{
    painter.fillRect(searchResultRect, QColor(0, 0, 255, 80));
}

void TerminalPainter::highlightScrolledLines(QPainter &painter, bool isTimerActive, QRect rect)
{
    const auto display = qobject_cast<TerminalDisplay *>(sender());

    QColor color = QColor(display->terminalColor()->colorTable()[Color4Index]);
    color.setAlpha(isTimerActive ? 255 : 150);
    painter.fillRect(rect, color);
}

QRegion TerminalPainter::highlightScrolledLinesRegion(TerminalScrollBar *scrollBar)
{
    const auto display = qobject_cast<TerminalDisplay *>(sender());

    QRegion dirtyRegion;
    const int highlightLeftPosition = display->scrollBar()->scrollBarPosition() == Enum::ScrollBarLeft ? display->scrollBar()->width() : 0;

    int start = 0;
    int nb_lines = abs(display->screenWindow()->scrollCount());
    if (nb_lines > 0 && display->scrollBar()->maximum() > 0) {
        QRect new_highlight;
        bool addToCurrentHighlight = scrollBar->highlightScrolledLines().isTimerActive()
            && (display->screenWindow()->scrollCount() * scrollBar->highlightScrolledLines().getPreviousScrollCount() > 0);
        if (addToCurrentHighlight) {
            const int oldScrollCount = scrollBar->highlightScrolledLines().getPreviousScrollCount();
            if (display->screenWindow()->scrollCount() > 0) {
                start = -1 * (oldScrollCount + display->screenWindow()->scrollCount()) + display->screenWindow()->windowLines();
            } else {
                start = -1 * oldScrollCount;
            }
            scrollBar->highlightScrolledLines().setPreviousScrollCount(oldScrollCount + display->screenWindow()->scrollCount());
        } else {
            start = display->screenWindow()->scrollCount() > 0 ? display->screenWindow()->windowLines() - nb_lines : 0;
            scrollBar->highlightScrolledLines().setPreviousScrollCount(display->screenWindow()->scrollCount());
        }

        new_highlight.setRect(highlightLeftPosition,
                              display->contentRect().top() + start * display->terminalFont()->fontHeight(),
                              scrollBar->highlightScrolledLines().HIGHLIGHT_SCROLLED_LINES_WIDTH,
                              nb_lines * display->terminalFont()->fontHeight());
        new_highlight.setTop(std::max(new_highlight.top(), display->contentRect().top()));
        new_highlight.setBottom(std::min(new_highlight.bottom(), display->contentRect().bottom()));
        if (!new_highlight.isValid()) {
            new_highlight = QRect(0, 0, 0, 0);
        }

        if (addToCurrentHighlight) {
            scrollBar->highlightScrolledLines().rect() |= new_highlight;
        } else {
            dirtyRegion |= scrollBar->highlightScrolledLines().rect();
            scrollBar->highlightScrolledLines().rect() = new_highlight;
        }
        dirtyRegion |= new_highlight;

        scrollBar->highlightScrolledLines().startTimer();
    }

    return dirtyRegion;
}

QColor alphaBlend(const QColor &foreground, const QColor &background)
{
    const auto foregroundAlpha = foreground.alphaF();
    const auto inverseForegroundAlpha = 1.0 - foregroundAlpha;
    const auto backgroundAlpha = background.alphaF();

    if (foregroundAlpha == 0.0) {
        return background;
    }

    if (backgroundAlpha == 1.0) {
        return QColor::fromRgb((foregroundAlpha * foreground.red()) + (inverseForegroundAlpha * background.red()),
                               (foregroundAlpha * foreground.green()) + (inverseForegroundAlpha * background.green()),
                               (foregroundAlpha * foreground.blue()) + (inverseForegroundAlpha * background.blue()),
                               0xff);
    } else {
        const auto inverseBackgroundAlpha = (backgroundAlpha * inverseForegroundAlpha);
        const auto finalAlpha = foregroundAlpha + inverseBackgroundAlpha;
        Q_ASSERT(finalAlpha != 0.0);

        return QColor::fromRgb((foregroundAlpha * foreground.red()) + (inverseBackgroundAlpha * background.red()),
                               (foregroundAlpha * foreground.green()) + (inverseBackgroundAlpha * background.green()),
                               (foregroundAlpha * foreground.blue()) + (inverseBackgroundAlpha * background.blue()),
                               finalAlpha);
    }
}

qreal wcag20AdjustColorPart(qreal v)
{
    return v <= 0.03928 ? v / 12.92 : qPow((v + 0.055) / 1.055, 2.4);
}

qreal wcag20RelativeLuminosity(const QColor &of)
{
    auto r = of.redF(), g = of.greenF(), b = of.blueF();

    const auto a = wcag20AdjustColorPart;

    auto r2 = a(r), g2 = a(g), b2 = a(b);

    return r2 * 0.2126 + g2 * 0.7152 + b2 * 0.0722;
}

qreal wcag20Contrast(const QColor &c1, const QColor &c2)
{
    const auto l1 = wcag20RelativeLuminosity(c1) + 0.05, l2 = wcag20RelativeLuminosity(c2) + 0.05;

    return (l1 > l2) ? l1 / l2 : l2 / l1;
}

std::optional<QColor> calculateBackgroundColor(const Character style, const QColor *colorTable)
{
    auto c1 = style.backgroundColor.color(colorTable);
    const auto initialBG = c1;

    c1.setAlphaF(0.8);

    const auto blend1 = alphaBlend(c1, colorTable[DEFAULT_FORE_COLOR]), blend2 = alphaBlend(c1, colorTable[DEFAULT_BACK_COLOR]);
    const auto fg = style.foregroundColor.color(colorTable);

    const auto contrast1 = wcag20Contrast(fg, blend1), contrast2 = wcag20Contrast(fg, blend2);
    const auto contrastBG1 = wcag20Contrast(blend1, initialBG), contrastBG2 = wcag20Contrast(blend2, initialBG);

    const auto fgFactor = 5.5; // if text contrast is too low against our calculated bg, then we flip to reverse
    const auto bgFactor = 1.6; // if bg contrast is too low against default bg, then we flip to reverse

    if ((contrast1 < fgFactor && contrast2 < fgFactor) || (contrastBG1 < bgFactor && contrastBG2 < bgFactor)) {
        return {};
    }

    return (contrast1 < contrast2) ? blend1 : blend2;
}

static void reverseRendition(Character &p)
{
    CharacterColor f = p.foregroundColor;
    CharacterColor b = p.backgroundColor;

    p.foregroundColor = b;
    p.backgroundColor = f;
}

void TerminalPainter::drawTextFragment(QPainter &painter,
                                       const QRect &rect,
                                       const QString &text,
                                       Character style,
                                       const QColor *colorTable,
                                       const bool invertedRendition,
                                       const LineProperty lineProperty)
{
    // setup painter

    // Sets the text selection colors, either:
    // - using reverseRendition(), which inverts the foreground/background
    //   colors OR
    // - blending the foreground/background colors
    if (style.rendition & RE_SELECTED) {
        if (invertedRendition) {
            reverseRendition(style);
        }
    }

    QColor foregroundColor = style.foregroundColor.color(colorTable);
    QColor backgroundColor = style.backgroundColor.color(colorTable);

    if (style.rendition & RE_SELECTED) {
        if (!invertedRendition) {
            backgroundColor = calculateBackgroundColor(style, colorTable).value_or(foregroundColor);
            if (backgroundColor == foregroundColor) {
                foregroundColor = style.backgroundColor.color(colorTable);
            }
        }
    }

    if (backgroundColor != colorTable[DEFAULT_BACK_COLOR]) {
        drawBackground(painter, rect, backgroundColor, false);
    }

    const auto display = qobject_cast<TerminalDisplay *>(sender());
    Screen *screen = display->screenWindow()->screen();
    int placementIdx = 0;
    qreal opacity = painter.opacity();
    int scrollDelta = display->terminalFont()->fontHeight() * (display->screenWindow()->currentLine() - screen->getHistLines());
    const bool origClipping = painter.hasClipping();
    const auto origClipRegion = painter.clipRegion();
    if (screen->hasGraphics()) {
        painter.setClipRect(rect);
        while (1) {
            TerminalGraphicsPlacement_t *p = screen->getGraphicsPlacement(placementIdx);
            if (!p || p->z >= 0) {
                break;
            }
            int x = p->col * display->terminalFont()->fontWidth() + p->X + display->contentRect().left();
            int y = p->row * display->terminalFont()->fontHeight() + p->Y + display->contentRect().top();
            QRectF srcRect(0, 0, p->pixmap.width(), p->pixmap.height());
            QRectF dstRect(x, y - scrollDelta, p->pixmap.width(), p->pixmap.height());
            painter.setOpacity(p->opacity);
            painter.drawPixmap(dstRect, p->pixmap, srcRect);
            placementIdx++;
        }
        painter.setOpacity(opacity);
    }

    QColor characterColor = foregroundColor;
    if ((style.rendition & RE_CURSOR) != 0) {
        drawCursor(painter, rect, foregroundColor, backgroundColor, characterColor);
    }

    // draw text
    drawCharacters(painter, rect, text, style, characterColor, lineProperty);

    if (screen->hasGraphics()) {
        while (1) {
            TerminalGraphicsPlacement_t *p = screen->getGraphicsPlacement(placementIdx);
            if (!p) {
                break;
            }
            QPixmap image = p->pixmap;
            int x = p->col * display->terminalFont()->fontWidth() + p->X + display->contentRect().left();
            int y = p->row * display->terminalFont()->fontHeight() + p->Y + display->contentRect().top();
            QRectF srcRect(0, 0, image.width(), image.height());
            QRectF dstRect(x, y - scrollDelta, image.width(), image.height());
            painter.setOpacity(p->opacity);
            painter.drawPixmap(dstRect, image, srcRect);
            placementIdx++;
        }
        painter.setOpacity(opacity);
        painter.setClipRegion(origClipRegion);
        painter.setClipping(origClipping);
    }
}

void TerminalPainter::drawPrinterFriendlyTextFragment(QPainter &painter,
                                                      const QRect &rect,
                                                      const QString &text,
                                                      Character style,
                                                      const LineProperty lineProperty)
{
    style.foregroundColor = CharacterColor(COLOR_SPACE_RGB, 0x00000000);
    style.backgroundColor = CharacterColor(COLOR_SPACE_RGB, 0xFFFFFFFF);

    drawCharacters(painter, rect, text, style, QColor(), lineProperty);
}

void TerminalPainter::drawBackground(QPainter &painter, const QRect &rect, const QColor &backgroundColor, bool useOpacitySetting)
{
    const auto display = qobject_cast<TerminalDisplay *>(sender());

    if (useOpacitySetting && !display->wallpaper()->isNull()
        && display->wallpaper()->draw(painter, rect, display->terminalColor()->opacity(), backgroundColor)) {
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

void TerminalPainter::drawCursor(QPainter &painter, const QRect &rect, const QColor &foregroundColor, const QColor &backgroundColor, QColor &characterColor)
{
    const auto display = qobject_cast<TerminalDisplay *>(sender());

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
    const qreal width = qMax(display->terminalFont()->fontWidth() / 12.0, 1.0);
    const qreal halfWidth = width / 2.0;
    pen.setWidthF(width);
    painter.setPen(pen);

    if (display->cursorShape() == Enum::BlockCursor) {
        painter.drawRect(cursorRect.adjusted(halfWidth, halfWidth, -halfWidth, -halfWidth));

        if (display->hasFocus()) {
            painter.fillRect(cursorRect, cursorColor);

            QColor cursorTextColor = display->terminalColor()->cursorTextColor();
            characterColor = cursorTextColor.isValid() ? cursorTextColor : backgroundColor;
        }
    } else if (display->cursorShape() == Enum::UnderlineCursor) {
        QLineF line(cursorRect.left() + halfWidth, cursorRect.bottom() - halfWidth, cursorRect.right() - halfWidth, cursorRect.bottom() - halfWidth);
        painter.drawLine(line);

    } else if (display->cursorShape() == Enum::IBeamCursor) {
        QLineF line(cursorRect.left() + halfWidth, cursorRect.top() + halfWidth, cursorRect.left() + halfWidth, cursorRect.bottom() - halfWidth);
        painter.drawLine(line);
    }
}

void TerminalPainter::drawCharacters(QPainter &painter,
                                     const QRect &rect,
                                     const QString &text,
                                     const Character style,
                                     const QColor &characterColor,
                                     const LineProperty lineProperty)
{
    const auto display = qobject_cast<TerminalDisplay *>(sender());

    if (display->textBlinking() && ((style.rendition & RE_BLINK) != 0)) {
        return;
    }

    if ((style.rendition & RE_CONCEAL) != 0) {
        return;
    }

    static constexpr int MaxFontWeight = 99;

    const int normalWeight = display->font().weight();

    const int boldWeight = qMin(normalWeight + 26, MaxFontWeight);

    const auto isBold = [boldWeight](const QFont &font) {
        return font.weight() >= boldWeight;
    };

    const bool useBold = (((style.rendition & RE_BOLD) != 0) && display->terminalFont()->boldIntense());
    const bool useUnderline = ((style.rendition & RE_UNDERLINE) != 0) || display->font().underline();
    const bool useItalic = ((style.rendition & RE_ITALIC) != 0) || display->font().italic();
    const bool useStrikeOut = ((style.rendition & RE_STRIKEOUT) != 0) || display->font().strikeOut();
    const bool useOverline = ((style.rendition & RE_OVERLINE) != 0) || display->font().overline();

    QFont currentFont = painter.font();

    if (isBold(currentFont) != useBold || currentFont.underline() != useUnderline || currentFont.italic() != useItalic
        || currentFont.strikeOut() != useStrikeOut || currentFont.overline() != useOverline) {
        currentFont.setWeight(useBold ? boldWeight : normalWeight);
        currentFont.setUnderline(useUnderline);
        currentFont.setItalic(useItalic);
        currentFont.setStrikeOut(useStrikeOut);
        currentFont.setOverline(useOverline);
        painter.setFont(currentFont);
    }

    // setup pen
    const QColor foregroundColor = style.foregroundColor.color(display->terminalColor()->colorTable());
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
    if (isLineCharString(text) && !display->terminalFont()->useFontLineCharacters()) {
        int y = rect.y();

        if (lineProperty & LINE_DOUBLEHEIGHT_BOTTOM) {
            y -= display->terminalFont()->fontHeight() / 2;
        }

        drawLineCharString(display, painter, rect.x(), y, text, style);
    } else {
        painter.setLayoutDirection(Qt::LeftToRight);
        int y = rect.y() + display->terminalFont()->fontAscent();

        if (lineProperty & LINE_DOUBLEHEIGHT_BOTTOM) {
            y -= display->terminalFont()->fontHeight() / 2;
        } else {
            y += display->terminalFont()->lineSpacing();
        }

        if (display->bidiEnabled()) {
            painter.drawText(rect.x(), y, text);
        } else {
            painter.drawText(rect.x(), y, LTR_OVERRIDE_CHAR + text);
        }
    }
    painter.setClipRegion(origClipRegion);
    painter.setClipping(origClipping);
}

void TerminalPainter::drawLineCharString(TerminalDisplay *display, QPainter &painter, int x, int y, const QString &str, const Character attributes)
{
    painter.setRenderHint(QPainter::Antialiasing, display->terminalFont()->antialiasText());

    const bool useBoldPen = (attributes.rendition & RE_BOLD) != 0 && display->terminalFont()->boldIntense();
    QRect cellRect = {x, y, display->terminalFont()->fontWidth(), display->terminalFont()->fontHeight()};
    QVector<uint> ucs4str = str.toUcs4();
    for (int i = 0; i < ucs4str.length(); i++) {
        LineBlockCharacters::draw(painter, cellRect.translated(i * display->terminalFont()->fontWidth(), 0), ucs4str[i], useBoldPen);
    }
    painter.setRenderHint(QPainter::Antialiasing, false);
}

void TerminalPainter::drawInputMethodPreeditString(QPainter &painter, const QRect &rect, TerminalDisplay::InputMethodData &inputMethodData, Character *image)
{
    const auto display = qobject_cast<TerminalDisplay *>(sender());

    if (inputMethodData.preeditString.isEmpty() || !display->isCursorOnDisplay()) {
        return;
    }

    const QPoint cursorPos = display->cursorPosition();

    QColor characterColor;
    const QColor background = display->terminalColor()->colorTable()[DEFAULT_BACK_COLOR];
    const QColor foreground = display->terminalColor()->colorTable()[DEFAULT_FORE_COLOR];
    const Character style = image[display->loc(cursorPos.x(), cursorPos.y())];

    drawBackground(painter, rect, background, true);
    drawCursor(painter, rect, foreground, background, characterColor);
    drawCharacters(painter, rect, inputMethodData.preeditString, style, characterColor, 0);

    inputMethodData.previousPreeditRect = rect;
}

}

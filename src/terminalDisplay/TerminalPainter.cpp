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
#include "../filterHotSpots/FilterChain.h"
#include "../session/SessionManager.h"
#include "TerminalColor.h"
#include "TerminalFonts.h"
#include "TerminalScrollBar.h"

// Qt
#include <QChar>
#include <QColor>
#include <QDebug>
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
TerminalPainter::TerminalPainter(TerminalDisplay *parent)
    : QObject(parent)
    , m_parentDisplay(parent)
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
    const bool invertedRendition = isInvertedRendition(m_parentDisplay);

    QVector<uint> univec;
    univec.reserve(m_parentDisplay->usedColumns());

    const int leftPadding = m_parentDisplay->contentRect().left() + m_parentDisplay->contentsRect().left();
    const int topPadding = m_parentDisplay->contentRect().top() + m_parentDisplay->contentsRect().top();
    const int fontWidth = m_parentDisplay->terminalFont()->fontWidth();
    const int fontHeight = m_parentDisplay->terminalFont()->fontHeight();

    for (int y = rect.y(); y <= rect.bottom(); y++) {
        const int textY = topPadding + fontHeight * y;
        int x = rect.x();
        bool doubleHeightLinePair = false;
        LineProperty lineProperty = y < lineProperties.size() ? lineProperties[y] : 0;

        // Search for start of multi-column character
        if (image[m_parentDisplay->loc(rect.x(), y)].isRightHalfOfDoubleWide() && (x != 0)) {
            x--;
        }

        for (; x <= rect.right(); x++) {
            int len = 1;
            const int pos = m_parentDisplay->loc(x, y);
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
                    const int nextPos = m_parentDisplay->loc(x + len, y);
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
                    const Character next_char = image[m_parentDisplay->loc(x + len, y)];
                    if (next_char.character == ' ' && char_value.hasSameColors(next_char) && char_value.hasSameRendition(next_char)) {
                        univec << next_char.character;
                        len++;
                    } else {
                        // break otherwise, we don't want to be stuck in this loop
                        break;
                    }
                }
            }

            // Adjust for trailing part of multi-column character
            if ((x + len < m_parentDisplay->usedColumns()) && image[m_parentDisplay->loc(x + len, y)].isRightHalfOfDoubleWide()) {
                len++;
            }

            QTransform textScale;
            bool doubleHeight = false;
            bool doubleWidthLine = false;

            if ((lineProperty & LINE_DOUBLEWIDTH) != 0) {
                textScale.scale(2, 1);
                doubleWidthLine = true;
            }

            doubleHeight = lineProperty & (LINE_DOUBLEHEIGHT_TOP | LINE_DOUBLEHEIGHT_BOTTOM);
            if (doubleHeight) {
                textScale.scale(1, 2);
            }

            if (y < lineProperties.size() - 1) {
                if (((lineProperties[y] & LINE_DOUBLEHEIGHT_TOP) != 0) && ((lineProperties[y + 1] & LINE_DOUBLEHEIGHT_BOTTOM) != 0)) {
                    doubleHeightLinePair = true;
                }
            }

            // Apply text scaling matrix
            paint.setWorldTransform(textScale, true);

            // Calculate the area in which the text will be drawn
            const int textX = leftPadding + fontWidth * x * (doubleWidthLine ? 2 : 1);
            const int textWidth = fontWidth * len;
            const int textHeight = doubleHeight && !doubleHeightLinePair ? fontHeight / 2 : fontHeight;

            // move the calculated area to take account of scaling applied to the painter.
            // the position of the area from the origin (0,0) is scaled
            // by the opposite of whatever
            // transformation has been applied to the painter.  this ensures that
            // painting does actually start from textArea.topLeft()
            //(instead of textArea.topLeft() * painter-scale)
            const QRect textArea(textScale.inverted().map(QPoint(textX, textY)), QSize(textWidth, textHeight));

            const QString unistr = QString::fromUcs4(univec.data(), univec.length());

            univec.clear();

            // paint text fragment
            if (printerFriendly) {
                drawPrinterFriendlyTextFragment(paint, textArea, unistr, image[pos], lineProperty);
            } else {
                drawTextFragment(paint, textArea, unistr, image[pos], m_parentDisplay->terminalColor()->colorTable(), invertedRendition, lineProperty);
            }

            paint.setWorldTransform(textScale.inverted(), true);

            x += len - 1;
        }
        if ((lineProperty & LINE_PROMPT_START) != 0
            && ((m_parentDisplay->semanticHints() == Enum::SemanticHintsURL && m_parentDisplay->filterChain()->showUrlHint())
                || m_parentDisplay->semanticHints() == Enum::SemanticHintsAlways)) {
            QPen pen(m_parentDisplay->terminalColor()->foregroundColor());
            paint.setPen(pen);
            paint.drawLine(leftPadding, textY, m_parentDisplay->contentRect().right(), textY);
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
    QColor color = QColor(m_parentDisplay->terminalColor()->colorTable()[Color4Index]);
    color.setAlpha(isTimerActive ? 255 : 150);
    painter.fillRect(rect, color);
}

QRegion TerminalPainter::highlightScrolledLinesRegion(TerminalScrollBar *scrollBar)
{
    QRegion dirtyRegion;
    const int highlightLeftPosition = m_parentDisplay->scrollBar()->scrollBarPosition() == Enum::ScrollBarLeft ? m_parentDisplay->scrollBar()->width() : 0;

    int start = 0;
    int nb_lines = abs(m_parentDisplay->screenWindow()->scrollCount());
    if (nb_lines > 0 && m_parentDisplay->scrollBar()->maximum() > 0) {
        QRect new_highlight;
        bool addToCurrentHighlight = scrollBar->highlightScrolledLines().isTimerActive()
            && (m_parentDisplay->screenWindow()->scrollCount() * scrollBar->highlightScrolledLines().getPreviousScrollCount() > 0);
        if (addToCurrentHighlight) {
            const int oldScrollCount = scrollBar->highlightScrolledLines().getPreviousScrollCount();
            if (m_parentDisplay->screenWindow()->scrollCount() > 0) {
                start = -1 * (oldScrollCount + m_parentDisplay->screenWindow()->scrollCount()) + m_parentDisplay->screenWindow()->windowLines();
            } else {
                start = -1 * oldScrollCount;
            }
            scrollBar->highlightScrolledLines().setPreviousScrollCount(oldScrollCount + m_parentDisplay->screenWindow()->scrollCount());
        } else {
            start = m_parentDisplay->screenWindow()->scrollCount() > 0 ? m_parentDisplay->screenWindow()->windowLines() - nb_lines : 0;
            scrollBar->highlightScrolledLines().setPreviousScrollCount(m_parentDisplay->screenWindow()->scrollCount());
        }

        new_highlight.setRect(highlightLeftPosition,
                              m_parentDisplay->contentRect().top() + start * m_parentDisplay->terminalFont()->fontHeight(),
                              scrollBar->highlightScrolledLines().HIGHLIGHT_SCROLLED_LINES_WIDTH,
                              nb_lines * m_parentDisplay->terminalFont()->fontHeight());
        new_highlight.setTop(std::max(new_highlight.top(), m_parentDisplay->contentRect().top()));
        new_highlight.setBottom(std::min(new_highlight.bottom(), m_parentDisplay->contentRect().bottom()));
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

    Screen *screen = m_parentDisplay->screenWindow()->screen();

    int placementIdx = 0;
    qreal opacity = painter.opacity();
    int scrollDelta = m_parentDisplay->terminalFont()->fontHeight() * (m_parentDisplay->screenWindow()->currentLine() - screen->getHistLines());
    const bool origClipping = painter.hasClipping();
    const auto origClipRegion = painter.clipRegion();
    if (screen->hasGraphics()) {
        painter.setClipRect(rect);
        while (1) {
            TerminalGraphicsPlacement_t *p = screen->getGraphicsPlacement(placementIdx);
            if (!p || p->z >= 0) {
                break;
            }
            int x = p->col * m_parentDisplay->terminalFont()->fontWidth() + p->X + m_parentDisplay->contentRect().left();
            int y = p->row * m_parentDisplay->terminalFont()->fontHeight() + p->Y + m_parentDisplay->contentRect().top();
            QRectF srcRect(0, 0, p->pixmap.width(), p->pixmap.height());
            QRectF dstRect(x, y - scrollDelta, p->pixmap.width(), p->pixmap.height());
            painter.setOpacity(p->opacity);
            painter.drawPixmap(dstRect, p->pixmap, srcRect);
            placementIdx++;
        }
        painter.setOpacity(opacity);
    }

    bool drawBG = backgroundColor != colorTable[DEFAULT_BACK_COLOR];
    if (screen->hasGraphics() && style.rendition == RE_TRANSPARENT) {
        drawBG = false;
    }

    if (drawBG) {
        drawBackground(painter, rect, backgroundColor, false);
    }

    QColor characterColor = foregroundColor;
    if ((style.rendition & RE_CURSOR) != 0) {
        drawCursor(painter, rect, foregroundColor, backgroundColor, characterColor);
    }
    if (m_parentDisplay->filterChain()->showUrlHint()) {
        if ((style.flags & EF_REPL) == EF_REPL_PROMPT) {
            int h, s, v;
            characterColor.getHsv(&h, &s, &v);
            s = s / 2;
            v = v / 2;
            characterColor.setHsv(h, s, v);
        }
        if ((style.flags & EF_REPL) == EF_REPL_INPUT) {
            int h, s, v;
            characterColor.getHsv(&h, &s, &v);
            s = (511 + s) / 3;
            v = (511 + v) / 3;
            characterColor.setHsv(h, s, v);
        }
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
            int x = p->col * m_parentDisplay->terminalFont()->fontWidth() + p->X + m_parentDisplay->contentRect().left();
            int y = p->row * m_parentDisplay->terminalFont()->fontHeight() + p->Y + m_parentDisplay->contentRect().top();
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
    if (useOpacitySetting && !m_parentDisplay->wallpaper()->isNull()
        && m_parentDisplay->wallpaper()->draw(painter, rect, m_parentDisplay->terminalColor()->opacity(), backgroundColor)) {
    } else if (qAlpha(m_parentDisplay->terminalColor()->blendColor()) < 0xff && useOpacitySetting) {
#if defined(Q_OS_MACOS)
        // TODO: On MacOS, using CompositionMode doesn't work. Altering the
        //       transparency in the color scheme alters the brightness.
        painter.fillRect(rect, backgroundColor);
#else
        QColor color(backgroundColor);
        color.setAlpha(qAlpha(m_parentDisplay->terminalColor()->blendColor()));

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
    if (m_parentDisplay->cursorBlinking()) {
        return;
    }

    // shift rectangle top down one pixel to leave some space
    // between top and bottom
    // noticeable when linespace>1
    QRectF cursorRect = rect.adjusted(0, 1, 0, 0);

    QColor color = m_parentDisplay->terminalColor()->cursorColor();
    QColor cursorColor = color.isValid() ? color : foregroundColor;

    QPen pen(cursorColor);
    // TODO: the relative pen width to draw the cursor is a bit hacky
    // and set to 1/12 of the font width. Visually it seems to work at
    // all scales but there must be better ways to do it
    const qreal width = qMax(m_parentDisplay->terminalFont()->fontWidth() / 12.0, 1.0);
    const qreal halfWidth = width / 2.0;
    pen.setWidthF(width);
    painter.setPen(pen);

    if (m_parentDisplay->cursorShape() == Enum::BlockCursor) {
        if (m_parentDisplay->hasFocus()) {
            painter.fillRect(cursorRect, cursorColor);

            // invert the color used to draw the text to ensure that the character at
            // the cursor position is readable
            QColor cursorTextColor = m_parentDisplay->terminalColor()->cursorTextColor();

            characterColor = cursorTextColor.isValid() ? cursorTextColor : backgroundColor;
        } else {
            // draw the cursor outline, adjusting the area so that
            // it is drawn entirely inside cursorRect
            painter.drawRect(cursorRect.adjusted(halfWidth, halfWidth, -halfWidth, -halfWidth));
        }
    } else if (m_parentDisplay->cursorShape() == Enum::UnderlineCursor) {
        QLineF line(cursorRect.left() + halfWidth, cursorRect.bottom() - halfWidth, cursorRect.right() - halfWidth, cursorRect.bottom() - halfWidth);
        painter.drawLine(line);

    } else if (m_parentDisplay->cursorShape() == Enum::IBeamCursor) {
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
    if (m_parentDisplay->textBlinking() && ((style.rendition & RE_BLINK) != 0)) {
        return;
    }

    if ((style.rendition & RE_CONCEAL) != 0) {
        return;
    }

    static const QFont::Weight FontWeights[] = {
        QFont::Thin,
        QFont::Light,
        QFont::Normal,
        QFont::Bold,
        QFont::Black,
    };

    // The weight used as bold depends on selected font's weight.
    // "Regular" will use "Bold", but e.g. "Thin" will use "Light".
    // Note that QFont::weight/setWeight() returns/takes an int in Qt5,
    // and a QFont::Weight in Qt6
    const auto normalWeight = m_parentDisplay->font().weight();
    auto it = std::upper_bound(std::begin(FontWeights), std::end(FontWeights), normalWeight);
    const QFont::Weight boldWeight = it != std::end(FontWeights) ? *it : QFont::Black;

    const bool useBold = (((style.rendition & RE_BOLD) != 0) && m_parentDisplay->terminalFont()->boldIntense());
    const bool useUnderline = ((style.rendition & RE_UNDERLINE) != 0) || m_parentDisplay->font().underline();
    const bool useItalic = ((style.rendition & RE_ITALIC) != 0) || m_parentDisplay->font().italic();
    const bool useStrikeOut = ((style.rendition & RE_STRIKEOUT) != 0) || m_parentDisplay->font().strikeOut();
    const bool useOverline = ((style.rendition & RE_OVERLINE) != 0) || m_parentDisplay->font().overline();

    QFont currentFont = painter.font();

    const bool isCurrentBold = currentFont.weight() >= boldWeight;
    // clang-format off
    if (isCurrentBold != useBold
        || currentFont.underline() != useUnderline
        || currentFont.italic() != useItalic
        || currentFont.strikeOut() != useStrikeOut
        || currentFont.overline() != useOverline)
    { // clang-format on
        currentFont.setWeight(useBold ? boldWeight : normalWeight);
        currentFont.setUnderline(useUnderline);
        currentFont.setItalic(useItalic);
        currentFont.setStrikeOut(useStrikeOut);
        currentFont.setOverline(useOverline);
        painter.setFont(currentFont);
    }

    // setup pen
    const QColor foregroundColor = style.foregroundColor.color(m_parentDisplay->terminalColor()->colorTable());
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
    if (isLineCharString(text) && !m_parentDisplay->terminalFont()->useFontLineCharacters()) {
        int y = rect.y();

        if (lineProperty & LINE_DOUBLEHEIGHT_BOTTOM) {
            y -= m_parentDisplay->terminalFont()->fontHeight() / 2;
        }

        drawLineCharString(m_parentDisplay, painter, rect.x(), y, text, style);
    } else {
        painter.setLayoutDirection(Qt::LeftToRight);
        int y = rect.y() + m_parentDisplay->terminalFont()->fontAscent();

        int shifted = 0;
        if (lineProperty & LINE_DOUBLEHEIGHT_BOTTOM) {
            y -= m_parentDisplay->terminalFont()->fontHeight() / 2;
        } else {
            // We shift half way down here to center
            shifted = m_parentDisplay->terminalFont()->lineSpacing() / 2;
            y += shifted;
        }

        if (m_parentDisplay->bidiEnabled()) {
            painter.drawText(rect.x(), y, text);
        } else {
            painter.drawText(rect.x(), y, LTR_OVERRIDE_CHAR + text);
        }

        if (shifted > 0) {
            // To avoid rounding errors we shift the rest this way
            y += m_parentDisplay->terminalFont()->lineSpacing() - shifted;
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
    if (inputMethodData.preeditString.isEmpty() || !m_parentDisplay->isCursorOnDisplay()) {
        return;
    }

    const QPoint cursorPos = m_parentDisplay->cursorPosition();

    QColor characterColor;
    const QColor background = m_parentDisplay->terminalColor()->colorTable()[DEFAULT_BACK_COLOR];
    const QColor foreground = m_parentDisplay->terminalColor()->colorTable()[DEFAULT_FORE_COLOR];
    const Character style = image[m_parentDisplay->loc(cursorPos.x(), cursorPos.y())];

    drawBackground(painter, rect, background, true);
    drawCursor(painter, rect, foreground, background, characterColor);
    drawCharacters(painter, rect, inputMethodData.preeditString, style, characterColor, 0);

    inputMethodData.previousPreeditRect = rect;
}

}

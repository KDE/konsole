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
#include <QElapsedTimer>
#include <QPainter>
#include <QPainterPath>
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

static inline bool isLineCharString(const QString &string, bool braille)
{
    if (string.length() == 0) {
        return false;
    }
    if (LineBlockCharacters::canDraw(string.at(0).unicode())) {
        return !(braille && LineBlockCharacters::isBraille(string.at(0).unicode()));
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

QColor alphaBlend(const QColor &foreground, const QColor &background)
{
    const auto foregroundAlpha = foreground.alphaF();
    const auto inverseForegroundAlpha = 1.0 - foregroundAlpha;
    const auto backgroundAlpha = background.alphaF();

    if (qFuzzyIsNull(foregroundAlpha)) {
        return background;
    }

    if (qFuzzyCompare(1.0 + backgroundAlpha, 1.0 + 1.0)) {
        return QColor::fromRgb((foregroundAlpha * foreground.red()) + (inverseForegroundAlpha * background.red()),
                               (foregroundAlpha * foreground.green()) + (inverseForegroundAlpha * background.green()),
                               (foregroundAlpha * foreground.blue()) + (inverseForegroundAlpha * background.blue()),
                               0xff);
    } else {
        const auto inverseBackgroundAlpha = (backgroundAlpha * inverseForegroundAlpha);
        const auto finalAlpha = foregroundAlpha + inverseBackgroundAlpha;
        Q_ASSERT(!qFuzzyIsNull(finalAlpha));

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

void TerminalPainter::drawContents(Character *image,
                                   QPainter &paint,
                                   const QRect &rect,
                                   bool printerFriendly,
                                   int imageSize,
                                   bool bidiEnabled,
                                   QVector<LineProperty> lineProperties,
                                   CharacterColor const *ulColorTable)
{
    auto currentProfile = SessionManager::instance()->sessionProfile(m_parentDisplay->session());
    const bool wordMode = currentProfile ? currentProfile->property<bool>(Profile::WordMode) : false;
    const bool wordModeAttr = currentProfile ? currentProfile->property<bool>(Profile::WordModeAttr) : true;
    const bool wordModeAscii = currentProfile ? currentProfile->property<bool>(Profile::WordModeAscii) : true;
    const bool wordModeBrahmic = currentProfile ? currentProfile->property<bool>(Profile::WordModeBrahmic) : true;
    const bool invertedRendition = currentProfile ? currentProfile->property<bool>(Profile::InvertSelectionColors) : false;
    const Enum::Hints semanticHints = currentProfile ? static_cast<Enum::Hints>(currentProfile->semanticHints()) : Enum::HintsNever;
    const Enum::Hints lineNumbers = currentProfile ? static_cast<Enum::Hints>(currentProfile->lineNumbers()) : Enum::HintsNever;
    const Enum::Hints errorBars = currentProfile ? static_cast<Enum::Hints>(currentProfile->property<int>(Profile::ErrorBars)) : Enum::HintsNever;
    const Enum::Hints errorBackground = currentProfile ? static_cast<Enum::Hints>(currentProfile->property<int>(Profile::ErrorBackground)) : Enum::HintsNever;
    const Enum::Hints alternatingBars = currentProfile ? static_cast<Enum::Hints>(currentProfile->property<int>(Profile::AlternatingBars)) : Enum::HintsNever;
    const Enum::Hints alternatingBackground =
        currentProfile ? static_cast<Enum::Hints>(currentProfile->property<int>(Profile::AlternatingBackground)) : Enum::HintsNever;
    const bool showHints = m_parentDisplay->filterChain()->showUrlHint();
#define hintActive(h) const bool h##Active = ((h == Enum::HintsURL && showHints) || h == Enum::HintsAlways)
    hintActive(semanticHints);
    hintActive(lineNumbers);
    hintActive(errorBars);
    hintActive(errorBackground);
    hintActive(alternatingBars);
    hintActive(alternatingBackground);
    QColor red;
    QColor gray;
    if (m_parentDisplay->terminalColor()->backgroundColor().red() > 128) {
        // Bright background
        red = QColor(255, 64, 64);
        gray = QColor(192, 192, 192);
    } else {
        red = QColor(48, 0, 0);
        gray = QColor(40, 40, 40);
    }

    QVector<uint> univec;
    univec.reserve(m_parentDisplay->usedColumns());
    int placementIdx = 0;

    const int leftPadding = m_parentDisplay->contentRect().left() + m_parentDisplay->contentsRect().left();
    const int topPadding = m_parentDisplay->contentRect().top() + m_parentDisplay->contentsRect().top();
    const int fontWidth = m_parentDisplay->terminalFont()->fontWidth();
    const int fontHeight = m_parentDisplay->terminalFont()->fontHeight();
    const QRect textArea(QPoint(leftPadding + fontWidth * rect.x(), topPadding + rect.y() * fontHeight),
                         QSize(rect.width() * fontWidth, rect.height() * fontHeight));
    QRegion sixelRegion = QRegion();
    if (!printerFriendly) {
        drawImagesBelowText(paint, textArea, fontWidth, fontHeight, placementIdx, sixelRegion);
    }

    static const QFont::Weight FontWeights[] = {
        QFont::Thin,
        QFont::Light,
        QFont::Normal,
        QFont::Bold,
        QFont::Black,
    };
    const QFont::Weight normalWeight = m_parentDisplay->font().weight();
    auto it = std::upper_bound(std::begin(FontWeights), std::end(FontWeights), normalWeight);
    const QFont::Weight boldWeight = it != std::end(FontWeights) ? *it : QFont::Black;
    paint.setLayoutDirection(Qt::LeftToRight);
    const QColor *colorTable = m_parentDisplay->terminalColor()->colorTable();

    for (int y = rect.y(); y <= rect.bottom(); y++) {
        int pos = m_parentDisplay->loc(0, y);
        if (pos > imageSize) {
            break;
        }
        int right = rect.right();
        if (pos + right > imageSize) {
            right = imageSize - pos;
        }

        const int textY = topPadding + fontHeight * y;
        bool doubleHeightLinePair = false;
        int x = rect.x();
        LineProperty lineProperty = y < lineProperties.size() ? lineProperties[y] : LineProperty();

        // Search for start of multi-column character
        if (image[m_parentDisplay->loc(rect.x(), y)].isRightHalfOfDoubleWide() && (x != 0)) {
            x--;
        }
        QTransform textScale;
        bool doubleHeight = false;
        bool doubleWidthLine = false;

        if ((lineProperty.flags.f.doublewidth) != 0) {
            textScale.scale(2, 1);
            doubleWidthLine = true;
        }

        doubleHeight = lineProperty.flags.f.doubleheight_top | lineProperty.flags.f.doubleheight_bottom;
        if (doubleHeight) {
            textScale.scale(1, 2);
        }

        if (y < lineProperties.size() - 1) {
            if (((lineProperties[y].flags.f.doubleheight_top) != 0) && ((lineProperties[y + 1].flags.f.doubleheight_bottom) != 0)) {
                doubleHeightLinePair = true;
            }
        }

        // Apply text scaling matrix
        paint.setWorldTransform(textScale, true);
        // Calculate the area in which the text will be drawn
        const int textX = leftPadding + fontWidth * rect.x() * (doubleWidthLine ? 2 : 1);
        const int textWidth = fontWidth * rect.width();
        const int textHeight = doubleHeight && !doubleHeightLinePair ? fontHeight / 2 : fontHeight;

        // move the calculated area to take account of scaling applied to the painter.
        // the position of the area from the origin (0,0) is scaled
        // by the opposite of whatever
        // transformation has been applied to the painter.  this ensures that
        // painting does actually start from textArea.topLeft()
        //(instead of textArea.topLeft() * painter-scale)
        QString line;
#define MAX_LINE_WIDTH 1024
#define vis2log(x) ((bidiEnabled && (x) <= lastNonSpace) ? line2log[vis2line[x]] : (x))
        int log2line[MAX_LINE_WIDTH];
        int line2log[MAX_LINE_WIDTH];
        uint16_t shapemap[MAX_LINE_WIDTH];
        int32_t vis2line[MAX_LINE_WIDTH];
        bool shaped;
        int lastNonSpace = m_parentDisplay->bidiMap(image + pos, line, log2line, line2log, shapemap, vis2line, shaped, bidiEnabled, bidiEnabled);
        const QRect textArea(textScale.inverted().map(QPoint(textX, textY)), QSize(textWidth, textHeight));
        if (!printerFriendly) {
            QColor background = m_parentDisplay->terminalColor()->backgroundColor();
            if (lineProperty.flags.f.error && errorBackgroundActive) {
                background = red;
            } else if ((lineProperty.counter & 1) && alternatingBackgroundActive) {
                background = gray;
            }
            drawBelowText(paint,
                          textArea,
                          image + pos,
                          rect.x(),
                          rect.width(),
                          fontWidth,
                          colorTable,
                          invertedRendition,
                          vis2line,
                          line2log,
                          bidiEnabled,
                          lastNonSpace,
                          background,
                          y,
                          sixelRegion);
        }

        RenditionFlags oldRendition = -1;
        QColor oldColor = QColor();
        int lastCharType = 0;
        QString word_str;
        int word_x = 0;
        int word_log_x = 0;
        for (; x <= right; x++) {
            if (x > lastNonSpace) {
                // What about the cursor?
                // break;
            }
            const int log_x = vis2log(x);
            const int log_next = vis2log(x + 1); // to know if this character is resolved as RTL, e.g. emojis in RTL context

            const Character char_value = image[pos + log_x];
            const bool doubleWidth = image[qMin(pos + log_x + 1, imageSize - 1)].isRightHalfOfDoubleWide(); // East_Asian_Width wide character

            if (!printerFriendly && lastCharType == 0 && char_value.isSpace() && char_value.rendition.f.cursor == 0) {
                continue;
            }

            QString unistr = line.mid(log2line[log_x], log2line[log_x + 1] - log2line[log_x]);
            if (shaped) {
                unistr[0] = shapemap[log2line[log_x]];
            }

            // paint text fragment
            if (unistr.length() && unistr[0] != QChar(0)) {
                int textWidth = fontWidth * (doubleWidth ? 2 : 1);
                int textX = leftPadding + fontWidth * x * (doubleWidthLine ? 2 : 1);
                // East_Asian_Width wide character behaving as RTL, e.g. wide emoji inside RTL context
                if (doubleWidth && log_next < log_x) {
                    textX -= fontWidth * (doubleWidthLine ? 2 : 1);
                }
                if (!printerFriendly && char_value.rendition.f.cursor) {
                    Character style = char_value;
                    m_parentDisplay->setVisualCursorPosition(x);

                    if (style.rendition.f.selected) {
                        if (invertedRendition) {
                            reverseRendition(style);
                        }
                    }

                    QColor foregroundColor = style.foregroundColor.color(colorTable);
                    QColor backgroundColor = style.backgroundColor.color(colorTable);

                    if (style.rendition.f.selected) {
                        if (!invertedRendition) {
                            backgroundColor = calculateBackgroundColor(style, colorTable).value_or(foregroundColor);
                            if (backgroundColor == foregroundColor) {
                                foregroundColor = style.backgroundColor.color(colorTable);
                            }
                        }
                    }
                    drawCursor(paint,
                               QRect(textScale.inverted().map(QPoint(textX, textY)), QSize(textWidth, textHeight)),
                               foregroundColor,
                               backgroundColor,
                               foregroundColor);
                }
                if (wordMode) {
                    int charType = 0;
                    if (wordModeAscii && char_value.flags & EF_ASCII_WORD) {
                        charType = 1;
                    }
                    if (wordModeBrahmic && char_value.flags & EF_BRAHMIC_WORD) {
                        charType = 2;
                    }
                    if (lastCharType != charType || (!wordModeAttr && lastCharType != 0 && char_value.notSameAttributesText(image[pos + vis2log(x - 1)]))) {
                        if (lastCharType != 0) {
                            drawTextCharacters(paint,
                                               QRect(textScale.inverted().map(QPoint(word_x, textY)), QSize(textWidth, textHeight)),
                                               word_str,
                                               image[pos + word_log_x],
                                               colorTable,
                                               invertedRendition,
                                               lineProperty,
                                               printerFriendly,
                                               oldRendition,
                                               oldColor,
                                               normalWeight,
                                               boldWeight);
                            lastCharType = charType;
                        }
                        if (charType != 0) {
                            // start new
                            lastCharType = charType;
                            word_str = QString(unistr);
                            word_x = textX;
                            word_log_x = log_x;
                            continue;
                        }
                    } else if (lastCharType != 0) {
                        word_str += unistr;
                        continue;
                    }
                }
                const QRect textAreaOneChar(textScale.inverted().map(QPoint(textX, textY)), QSize(textWidth, textHeight));
                drawTextCharacters(paint,
                                   textAreaOneChar,
                                   unistr,
                                   image[pos + log_x],
                                   colorTable,
                                   invertedRendition,
                                   lineProperty,
                                   printerFriendly,
                                   oldRendition,
                                   oldColor,
                                   normalWeight,
                                   boldWeight);
            }
        }
        if (wordMode && lastCharType != 0) {
            drawTextCharacters(paint,
                               QRect(textScale.inverted().map(QPoint(word_x, textY)), QSize(textWidth, textHeight)),
                               word_str,
                               image[pos + word_log_x],
                               colorTable,
                               invertedRendition,
                               lineProperty,
                               printerFriendly,
                               oldRendition,
                               oldColor,
                               normalWeight,
                               boldWeight);
        }
        if (!printerFriendly) {
            drawAboveText(paint,
                          textArea,
                          image + pos,
                          rect.x(),
                          rect.width(),
                          fontWidth,
                          colorTable,
                          invertedRendition,
                          vis2line,
                          line2log,
                          bidiEnabled,
                          lastNonSpace,
                          ulColorTable);
        }

        paint.setWorldTransform(textScale.inverted(), true);
        if (lineProperty.flags.f.prompt_start && semanticHintsActive) {
            QPen pen(m_parentDisplay->terminalColor()->foregroundColor());
            paint.setPen(pen);
            paint.drawLine(leftPadding, textY, m_parentDisplay->contentRect().right(), textY);
        }
        auto opacity = paint.opacity();
        if ((lineProperty.counter & 1) && alternatingBarsActive) {
            QPen pen(QColor("dark gray"));
            pen.setWidth(2);
            paint.setPen(pen);
            paint.setOpacity(0.5);
            paint.drawLine(leftPadding + 1, textY + 1, leftPadding + 1, textY + fontHeight - 1);
        }
        if (lineProperty.flags.f.error && errorBarsActive) {
            QPen pen(QColor("red"));
            pen.setWidth(4);
            paint.setPen(pen);
            paint.setOpacity(0.5);
            paint.drawLine(leftPadding + 2, textY + 2, leftPadding + 2, textY + fontHeight - 2);
        }
        paint.setOpacity(opacity);
        if (lineNumbersActive) {
            QRect rect(m_parentDisplay->contentRect().right() - 4 * fontWidth, textY, m_parentDisplay->contentRect().right(), textY + fontHeight);
            QPen pen(QColor(0xC00000));
            paint.setPen(pen);
            QFont currentFont = paint.font();
            currentFont.setWeight(normalWeight);
            currentFont.setItalic(false);
            paint.setFont(currentFont);
            paint.drawText(rect, Qt::AlignLeft, QString::number(y + m_parentDisplay->screenWindow()->currentLine()));
        }

        if (doubleHeightLinePair) {
            y++;
        }
    }
    if (!printerFriendly) {
        drawImagesAboveText(paint, textArea, fontWidth, fontHeight, placementIdx);
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
        new_highlight = highdpi_adjust_rect(new_highlight);
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

void TerminalPainter::drawBackground(QPainter &painter, const QRect &rect, const QColor &backgroundColor, bool useOpacitySetting)
{
    if (!m_parentDisplay->wallpaper()->isNull()
        && m_parentDisplay->wallpaper()->draw(painter, rect, useOpacitySetting ? m_parentDisplay->terminalColor()->opacity() : 1.0, backgroundColor)) {
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

void TerminalPainter::updateCursorTextColor(const QColor &backgroundColor, QColor &characterColor)
{
    if (m_parentDisplay->cursorShape() == Enum::BlockCursor) {
        if (m_parentDisplay->hasFocus()) {
            // invert the color used to draw the text to ensure that the character at
            // the cursor position is readable
            QColor cursorTextColor = m_parentDisplay->terminalColor()->cursorTextColor();

            characterColor = cursorTextColor.isValid() ? cursorTextColor : backgroundColor;
        }
    }
}

void TerminalPainter::drawCursor(QPainter &painter, const QRectF &cursorRect, const QColor &foregroundColor, const QColor &backgroundColor, QColor &characterColor)
{
    if (m_parentDisplay->cursorBlinking()) {
        return;
    }

    QColor color = m_parentDisplay->terminalColor()->cursorColor();
    QColor cursorColor = color.isValid() ? color : foregroundColor;

    QPen pen(cursorColor);
    pen.setJoinStyle(Qt::MiterJoin);
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

            updateCursorTextColor(backgroundColor, characterColor);
        } else {
            // draw the cursor outline, adjusting the area so that
            // it is drawn entirely inside cursorRect
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.drawRect(cursorRect.adjusted(halfWidth, halfWidth, -halfWidth, -halfWidth));
            painter.setRenderHint(QPainter::Antialiasing, false);
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
    if (m_parentDisplay->textBlinking() && (style.rendition.f.blink != 0)) {
        return;
    }

    if (style.rendition.f.conceal != 0) {
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

    const bool useBold = ((style.rendition.f.bold != 0) && m_parentDisplay->terminalFont()->boldIntense());
    const bool useUnderline = (style.rendition.f.underline != 0) || m_parentDisplay->font().underline();
    const bool useItalic = (style.rendition.f.italic != 0) || m_parentDisplay->font().italic();
    const bool useStrikeOut = (style.rendition.f.strikeout != 0) || m_parentDisplay->font().strikeOut();
    const bool useOverline = (style.rendition.f.overline != 0) || m_parentDisplay->font().overline();

    QFont currentFont = painter.font();

    const bool isCurrentBold = currentFont.weight() >= boldWeight;
    // clang-format off
    if (isCurrentBold != useBold
        || currentFont.underline() != useUnderline
        || currentFont.italic() != useItalic
        || currentFont.strikeOut() != useStrikeOut
        || currentFont.overline() != useOverline
    )
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
    // draw text
    if (!m_parentDisplay->terminalFont()->useFontLineCharacters() && isLineCharString(text, m_parentDisplay->terminalFont()->useFontBrailleCharacters())) {
        int y = rect.y();

        if (lineProperty.flags.f.doubleheight_bottom) {
            y -= m_parentDisplay->terminalFont()->fontHeight() / 2;
        }

        drawLineCharString(m_parentDisplay, painter, rect.x(), y, text, style);
    } else {
        painter.setLayoutDirection(Qt::LeftToRight);
        int y = rect.y() + m_parentDisplay->terminalFont()->fontAscent();

        int shifted = 0;
        if (lineProperty.flags.f.doubleheight_bottom) {
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
}

void TerminalPainter::drawLineCharString(TerminalDisplay *display, QPainter &painter, int x, int y, const QString &str, const Character attributes)
{
    // only turn on anti-aliasing during this short time for the "text"
    // for the normal text we have TextAntialiasing on demand on
    // otherwise we have rendering artifacts
    // set https://bugreports.qt.io/browse/QTBUG-66036
    painter.setRenderHint(QPainter::Antialiasing, display->terminalFont()->antialiasText());

    const bool useBoldPen = (attributes.rendition.f.bold) != 0 && display->terminalFont()->boldIntense();
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
    drawCharacters(painter, rect, inputMethodData.preeditString, style, characterColor, LineProperty());

    inputMethodData.previousPreeditRect = rect;
}

void TerminalPainter::drawBelowText(QPainter &painter,
                                    const QRect &rect,
                                    Character *style,
                                    int startX,
                                    int width,
                                    int fontWidth,
                                    const QColor *colorTable,
                                    const bool invertedRendition,
                                    int *vis2line,
                                    int *line2log,
                                    bool bidiEnabled,
                                    int lastNonSpace,
                                    QColor background,
                                    int Y,
                                    QRegion sixelRegion)
{
    // setup painter

    bool first = true;
    QRect constRect(0, 0, 0, 0);
    QColor backgroundColor;
    QColor foregroundColor;
    bool drawBG = false;
    int lastX = 0;

    for (int i = 0;; i++) {
        int x = vis2log(i + startX);

        if (first || i == width || style[x].rendition.all != style[lastX].rendition.all || style[x].foregroundColor != style[lastX].foregroundColor
            || style[x].backgroundColor != style[lastX].backgroundColor) {
            if (first) {
                first = false;
            } else {
                if (drawBG) {
                    painter.fillRect(constRect, backgroundColor);
                }
            }
            if (i == width) {
                return;
            }
            // Sets the text selection colors, either:
            // - using reverseRendition(), which inverts the foreground/background
            //   colors OR
            // - blending the foreground/background colors
            if (style[x].rendition.f.selected && invertedRendition) {
                backgroundColor = style[x].foregroundColor.color(colorTable);
                foregroundColor = style[x].backgroundColor.color(colorTable);
            } else {
                foregroundColor = style[x].foregroundColor.color(colorTable);
                backgroundColor = style[x].backgroundColor.color(colorTable);
            }

            if (style[x].rendition.f.selected) {
                if (!invertedRendition) {
                    backgroundColor = calculateBackgroundColor(style[x], colorTable).value_or(foregroundColor);
                    if (backgroundColor == foregroundColor) {
                        foregroundColor = style[x].backgroundColor.color(colorTable);
                    }
                }
            }
            if (backgroundColor == colorTable[DEFAULT_BACK_COLOR]) {
                backgroundColor = background;
            }
            if (style[x].rendition.f.transparent) {
                drawBG = false;
            } else {
                drawBG = backgroundColor != colorTable[DEFAULT_BACK_COLOR] || sixelRegion.contains(QPoint(i + startX, Y));
            }

            constRect = QRect(rect.x() + fontWidth * i, rect.y(), fontWidth, rect.height());
        } else {
            constRect.setWidth(constRect.width() + fontWidth);
        }
        lastX = x;
    }
}

void TerminalPainter::drawAboveText(QPainter &painter,
                                    const QRect &rect,
                                    Character *style,
                                    int startX,
                                    int width,
                                    int fontWidth,
                                    const QColor *colorTable,
                                    const bool invertedRendition,
                                    int *vis2line,
                                    int *line2log,
                                    bool bidiEnabled,
                                    int lastNonSpace,
                                    CharacterColor const *ulColorTable)
{
    bool first = true;
    QColor backgroundColor;
    QColor foregroundColor;
    int lastX = 0;
    int startUnderline = -1;
    int startOverline = -1;
    int startStrikeOut = -1;

    for (int i = 0;; i++) {
        int x = vis2log(i + startX);

        if (first || i == width || ((style[x].rendition.all ^ style[lastX].rendition.all) & RE_MASK_ABOVE)
            || ((style[x].flags ^ style[lastX].flags) & EF_UNDERLINE_COLOR) || style[x].foregroundColor != style[lastX].foregroundColor
            || style[x].backgroundColor != style[lastX].backgroundColor) {
            if (first) {
                first = false;
            } else {
                if ((i == width || style[x].rendition.f.strikeout == 0) && startStrikeOut >= 0) {
                    QPen pen(foregroundColor);
                    int y = rect.y() + m_parentDisplay->terminalFont()->fontAscent();
                    pen.setWidth(m_parentDisplay->terminalFont()->lineWidth());
                    painter.setPen(pen);
                    painter.drawLine(rect.x() + fontWidth * startStrikeOut,
                                     y - m_parentDisplay->terminalFont()->strikeOutPos(),
                                     rect.x() + fontWidth * i - 1,
                                     y - m_parentDisplay->terminalFont()->strikeOutPos());
                    startStrikeOut = -1;
                }
                if ((i == width || style[x].rendition.f.overline == 0) && startOverline >= 0) {
                    QPen pen(foregroundColor);
                    qreal y = rect.y() + m_parentDisplay->terminalFont()->fontAscent() - m_parentDisplay->terminalFont()->overlinePos()
                        + m_parentDisplay->terminalFont()->lineSpacing() / static_cast<qreal>(2);
                    pen.setWidth(m_parentDisplay->terminalFont()->lineWidth());
                    painter.setPen(pen);
                    painter.drawLine(QLineF(rect.x() + fontWidth * startOverline, y, rect.x() + fontWidth * i - 1, y));
                    startOverline = -1;
                }
                int underline = style[lastX].rendition.f.underline;
                if ((i == width || style[x].rendition.f.underline != underline || ((style[x].flags ^ style[lastX].flags) & EF_UNDERLINE_COLOR))
                    && startUnderline >= 0) {
                    QPen pen(foregroundColor);
                    if (ulColorTable != nullptr && (style[lastX].flags & EF_UNDERLINE_COLOR) != 0) {
                        pen.setColor(ulColorTable[((style[lastX].flags & EF_UNDERLINE_COLOR)) / EF_UNDERLINE_COLOR_1 - 1].color(colorTable));
                    }
                    const int lw = m_parentDisplay->terminalFont()->lineWidth();
                    qreal y = rect.y() + m_parentDisplay->terminalFont()->fontAscent() + m_parentDisplay->terminalFont()->underlinePos()
                        + m_parentDisplay->terminalFont()->lineSpacing() / static_cast<qreal>(2);
                    if (underline == RE_UNDERLINE_DOUBLE || underline == RE_UNDERLINE_CURL) {
                        y = rect.bottom() - 1;
                    }
                    pen.setWidth(lw);
                    if (underline == RE_UNDERLINE_DOT) {
                        pen.setStyle(Qt::DotLine);
                    } else if (underline == RE_UNDERLINE_DASH) {
                        pen.setStyle(Qt::DashLine);
                    }
                    painter.setPen(pen);
                    const int x1 = rect.x() + fontWidth * startUnderline;
                    const int x2 = rect.x() + fontWidth * i - 1;
                    if (underline != RE_UNDERLINE_CURL)
                        painter.drawLine(QLineF(x1, y, x2, y));
                    if (underline == RE_UNDERLINE_DOUBLE || underline == RE_UNDERLINE_CURL) {
                        const int fontHeight = m_parentDisplay->terminalFont()->fontHeight();
                        const qreal amplitude = static_cast<qreal>(fontHeight) / 8.0;

                        if (underline == RE_UNDERLINE_DOUBLE) {
                            painter.drawLine(x1, y - amplitude, x2, y - amplitude);
                        } else {
                            y = std::max(static_cast<qreal>(0), y - amplitude / 2);
                            assert(underline == RE_UNDERLINE_CURL);
                            const int len = x2 - x1;
                            if (len > 0) {
                                const qreal desiredWavelength = fontWidth / 1.2;
                                const int cycles = std::max(1, static_cast<int>(len / desiredWavelength));
                                const qreal wavelength = static_cast<qreal>(len) / cycles;
                                const qreal halfWavelength = wavelength / 2.0;
                                const qreal quarterWavelength = halfWavelength / 2.0;
                                const qreal threeQuarterWavelength = 3 * quarterWavelength;
                                QPainterPath segment;
                                segment.moveTo(0, 0);
                                segment.quadTo(quarterWavelength, -amplitude, halfWavelength, 0);
                                segment.quadTo(threeQuarterWavelength, amplitude, wavelength, 0);
                                QPainterPath path;
                                path.moveTo(x1, y);
                                for (int i = 0; i < cycles; ++i)
                                    path.addPath(segment.translated(x1 + i * wavelength, y));
                                painter.drawPath(path);
                            }
                        }
                    }

                    startUnderline = -1;
                }
            }
            if (i == width) {
                return;
            }
            // Sets the text selection colors, either:
            // - using reverseRendition(), which inverts the foreground/background
            //   colors OR
            // - blending the foreground/background colors
            if (style[x].rendition.f.selected && invertedRendition) {
                backgroundColor = style[x].foregroundColor.color(colorTable);
                foregroundColor = style[x].backgroundColor.color(colorTable);
            } else {
                foregroundColor = style[x].foregroundColor.color(colorTable);
                backgroundColor = style[x].backgroundColor.color(colorTable);
            }

            if (style[x].rendition.f.selected) {
                if (!invertedRendition) {
                    backgroundColor = calculateBackgroundColor(style[x], colorTable).value_or(foregroundColor);
                    if (backgroundColor == foregroundColor) {
                        foregroundColor = style[x].backgroundColor.color(colorTable);
                    }
                }
            }
            if (style[x].rendition.f.strikeout && startStrikeOut == -1) {
                startStrikeOut = i;
            }
            if (style[x].rendition.f.overline && startOverline == -1) {
                startOverline = i;
            }
            if (style[x].rendition.f.underline && startUnderline == -1) {
                startUnderline = i;
            }
            lastX = x;
        }
    }
}

void TerminalPainter::drawImagesBelowText(QPainter &painter, const QRect &rect, int fontWidth, int fontHeight, int &placementIdx, QRegion &sixelRegion)
{
    Screen *screen = m_parentDisplay->screenWindow()->screen();

    placementIdx = 0;
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
            int x = p->col * fontWidth + p->X + m_parentDisplay->contentRect().left();
            int y = p->row * fontHeight + p->Y + m_parentDisplay->contentRect().top();
            QRectF srcRect(0, 0, p->pixmap.width(), p->pixmap.height());
            QRectF dstRect(x, y - scrollDelta, p->pixmap.width(), p->pixmap.height());
            painter.setOpacity(p->opacity);
            painter.drawPixmap(dstRect, p->pixmap, srcRect);
            if (p->source == TerminalGraphicsPlacement_t::Sixel) {
                sixelRegion = sixelRegion.united(QRect(p->col, p->row, p->cols, p->rows));
            }
            placementIdx++;
        }
        painter.setOpacity(opacity);
        painter.setClipRegion(origClipRegion);
        painter.setClipping(origClipping);
    }
}

void TerminalPainter::drawImagesAboveText(QPainter &painter, const QRect &rect, int fontWidth, int fontHeight, int &placementIdx)
{
    // setup painter
    Screen *screen = m_parentDisplay->screenWindow()->screen();

    qreal opacity = painter.opacity();
    int scrollDelta = fontHeight * (m_parentDisplay->screenWindow()->currentLine() - screen->getHistLines());
    const bool origClipping = painter.hasClipping();
    const auto origClipRegion = painter.clipRegion();

    if (screen->hasGraphics()) {
        painter.setClipRect(rect);
        while (1) {
            TerminalGraphicsPlacement_t *p = screen->getGraphicsPlacement(placementIdx);
            if (!p) {
                break;
            }
            QPixmap image = p->pixmap;
            int x = p->col * fontWidth + p->X + m_parentDisplay->contentRect().left();
            int y = p->row * fontHeight + p->Y + m_parentDisplay->contentRect().top();
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

void TerminalPainter::drawTextCharacters(QPainter &painter,
                                         const QRect &rect,
                                         const QString &text,
                                         Character style,
                                         const QColor *colorTable,
                                         const bool invertedRendition,
                                         const LineProperty lineProperty,
                                         bool printerFriendly,
                                         RenditionFlags &oldRendition,
                                         QColor oldColor,
                                         QFont::Weight normalWeight,
                                         QFont::Weight boldWeight)
{
    // setup painter
    if (style.rendition.f.conceal != 0) {
        return;
    }
    QColor characterColor;
    if (!printerFriendly) {
        // Sets the text selection colors, either:
        // - invertedRendition, which inverts the foreground/background colors OR
        // - blending the foreground/background colors
        if (m_parentDisplay->textBlinking() && (style.rendition.f.blink != 0)) {
            return;
        }

        if (style.rendition.f.selected) {
            if (invertedRendition) {
                reverseRendition(style);
            }
        }

        QColor foregroundColor = style.foregroundColor.color(colorTable);
        QColor backgroundColor = style.backgroundColor.color(colorTable);

        if (style.rendition.f.selected) {
            if (!invertedRendition) {
                backgroundColor = calculateBackgroundColor(style, colorTable).value_or(foregroundColor);
                if (backgroundColor == foregroundColor) {
                    foregroundColor = style.backgroundColor.color(colorTable);
                }
            }
        }
        characterColor = foregroundColor;
        if (style.rendition.f.cursor != 0 && !m_parentDisplay->cursorBlinking()) {
            updateCursorTextColor(backgroundColor, characterColor);
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
    } else {
        characterColor = QColor(0, 0, 0);
    }

    // The weight used as bold depends on selected font's weight.
    // "Regular" will use "Bold", but e.g. "Thin" will use "Light".
    // Note that QFont::weight/setWeight() returns/takes an int in Qt5,
    // and a QFont::Weight in Qt6
    QFont savedFont;
    bool restoreFont = false;
    if ((style.flags & EF_EMOJI_REPRESENTATION) && m_parentDisplay->terminalFont()->hasExtraFont(0)) {
        savedFont = painter.font();
        restoreFont = true;
        painter.setFont(m_parentDisplay->terminalFont()->getExtraFont(0));
    } else {
        if (oldRendition != style.rendition.all) {
            const bool useBold = ((style.rendition.f.bold != 0) && m_parentDisplay->terminalFont()->boldIntense());
            const bool useItalic = (style.rendition.f.italic != 0) || m_parentDisplay->font().italic();

            QFont currentFont = painter.font();

            const bool isCurrentBold = currentFont.weight() >= boldWeight;
            if (isCurrentBold != useBold || currentFont.italic() != useItalic) {
                currentFont.setWeight(useBold ? boldWeight : normalWeight);
                currentFont.setItalic(useItalic);
                painter.setFont(currentFont);
            }
        }
    }

    if (characterColor != oldColor) {
        QPen pen = painter.pen();
        if (pen.color() != characterColor) {
            painter.setPen(characterColor);
        }
    }
    // const bool origClipping = painter.hasClipping();
    // const auto origClipRegion = painter.clipRegion();
    // painter.setClipRect(rect);
    // draw text
    if (!m_parentDisplay->terminalFont()->useFontLineCharacters() && isLineCharString(text, m_parentDisplay->terminalFont()->useFontBrailleCharacters())) {
        int y = rect.y();

        if (lineProperty.flags.f.doubleheight_bottom) {
            y -= m_parentDisplay->terminalFont()->fontHeight() / 2;
        }

        drawLineCharString(m_parentDisplay, painter, rect.x(), y, text, style);
    } else {
        int y = rect.y() + m_parentDisplay->terminalFont()->fontAscent();

        if (lineProperty.flags.f.doubleheight_bottom) {
            y -= m_parentDisplay->terminalFont()->fontHeight() / 2;
        } else {
            // We shift half way down here to center
            y += m_parentDisplay->terminalFont()->lineSpacing() / 2;
        }
        painter.drawText(rect.x(), y, text);
        if (0 && text.toUcs4().length() >= 1) {
            fprintf(stderr, " %lli  ", (qint64)text.toUcs4().length());
            for (int i = 0; i < text.toUcs4().length(); i++) {
                fprintf(stderr, " %04x  ", text.toUcs4()[i]);
            }
            fprintf(stderr, "\n");
        }
    }
    if (restoreFont) {
        painter.setFont(savedFont);
    }
}
}

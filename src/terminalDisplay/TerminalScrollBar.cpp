/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "TerminalScrollBar.h"

// Konsole
#include "../characters/Character.h"
#include "TerminalDisplay.h"
#include "TerminalFonts.h"
#include "session/SessionController.h"

// KDE
#include <KMessageWidget>

// Qt
#include <QGuiApplication>
#include <QLabel>
#include <QProxyStyle>
#include <QRect>
#include <QTimer>

namespace Konsole
{
TerminalScrollBar::TerminalScrollBar(QWidget *parent)
    : QScrollBar(parent)
{
    connect(this, &QScrollBar::valueChanged, this, &TerminalScrollBar::scrollBarPositionChanged);
}

void TerminalScrollBar::setScrollBarPosition(Enum::ScrollBarPositionEnum position)
{
    if (_scrollbarLocation == position) {
        return;
    }

    _scrollbarLocation = position;
    applyScrollBarPosition(true);
}

void TerminalScrollBar::setScroll(int cursor, int slines)
{
    const auto display = qobject_cast<TerminalDisplay *>(this->parent());
    // update _scrollBar if the range or value has changed,
    // otherwise return
    //
    // setting the range or value of a _scrollBar will always trigger
    // a repaint, so it should be avoided if it is not necessary
    if (this->minimum() == 0 && this->maximum() == (slines - display->lines()) && this->value() == cursor) {
        return;
    }

    disconnect(this, &QScrollBar::valueChanged, this, &TerminalScrollBar::scrollBarPositionChanged);
    setRange(0, slines - display->lines());
    setSingleStep(1);
    setPageStep(display->lines());
    setValue(cursor);
    connect(this, &QScrollBar::valueChanged, this, &TerminalScrollBar::scrollBarPositionChanged);
}

void TerminalScrollBar::setScrollFullPage(bool fullPage)
{
    _scrollFullPage = fullPage;
}

bool TerminalScrollBar::scrollFullPage() const
{
    return _scrollFullPage;
}

void TerminalScrollBar::setHighlightScrolledLines(bool highlight)
{
    _highlightScrolledLines.setEnabled(highlight);
    _highlightScrolledLines.setTimer(this);
}

bool TerminalScrollBar::alternateScrolling() const
{
    return _alternateScrolling;
}

void TerminalScrollBar::setAlternateScrolling(bool enable)
{
    _alternateScrolling = enable;
}

void TerminalScrollBar::scrollBarPositionChanged(int)
{
    const auto display = qobject_cast<TerminalDisplay *>(this->parent());

    if (display->screenWindow().isNull()) {
        return;
    }

    display->screenWindow()->scrollTo(this->value());

    // if the thumb has been moved to the bottom of the _scrollBar then set
    // the display to automatically track new output,
    // that is, scroll down automatically
    // to how new _lines as they are added
    const bool atEndOfOutput = (this->value() == this->maximum());
    display->screenWindow()->setTrackOutput(atEndOfOutput);

    display->updateImage();
}

void TerminalScrollBar::highlightScrolledLinesEvent()
{
    const auto display = qobject_cast<TerminalDisplay *>(this->parent());
    display->update(_highlightScrolledLines.rect());
}

void TerminalScrollBar::applyScrollBarPosition(bool propagate)
{
    setHidden(_scrollbarLocation == Enum::ScrollBarHidden);

    if (propagate) {
        const auto display = qobject_cast<TerminalDisplay *>(this->parent());
        display->propagateSize();
        display->update();
    }
}

// scrolls the image by 'lines', down if lines > 0 or up otherwise.
//
// the terminal emulation keeps track of the scrolling of the character
// image as it receives input, and when the view is updated, it calls scrollImage()
// with the final scroll amount.  this improves performance because scrolling the
// display is much cheaper than re-rendering all the text for the
// part of the image which has moved up or down.
// Instead only new lines have to be drawn
void TerminalScrollBar::scrollImage(int lines, const QRect &screenWindowRegion, Character *image, int imageSize)
{
    // return if there is nothing to do
    if ((lines == 0) || (image == nullptr)) {
        return;
    }

    const auto display = qobject_cast<TerminalDisplay *>(this->parent());
    // constrain the region to the display
    // the bottom of the region is capped to the number of lines in the display's
    // internal image - 2, so that the height of 'region' is strictly less
    // than the height of the internal image.
    QRect region = screenWindowRegion;
    region.setBottom(qMin(region.bottom(), display->lines() - 2));

    // return if there is nothing to do
    if (!region.isValid() || (region.top() + abs(lines)) >= region.bottom() || display->lines() <= region.bottom()) {
        return;
    }

    // Note:  With Qt 4.4 the left edge of the scrolled area must be at 0
    // to get the correct (newly exposed) part of the widget repainted.
    //
    // The right edge must be before the left edge of the scroll bar to
    // avoid triggering a repaint of the entire widget, the distance is
    // given by SCROLLBAR_CONTENT_GAP
    //
    // Set the QT_FLUSH_PAINT environment variable to '1' before starting the
    // application to monitor repainting.
    //
    const int scrollBarWidth = this->isHidden() ? 0 : this->width();
    const int SCROLLBAR_CONTENT_GAP = 1;
    QRect scrollRect;
    if (_scrollbarLocation == Enum::ScrollBarLeft) {
        scrollRect.setLeft(scrollBarWidth + SCROLLBAR_CONTENT_GAP
                           + (_highlightScrolledLines.isEnabled() ? _highlightScrolledLines.HIGHLIGHT_SCROLLED_LINES_WIDTH : 0));
        scrollRect.setRight(display->width());
    } else {
        scrollRect.setLeft(_highlightScrolledLines.isEnabled() ? _highlightScrolledLines.HIGHLIGHT_SCROLLED_LINES_WIDTH : 0);

        scrollRect.setRight(display->width() - scrollBarWidth - SCROLLBAR_CONTENT_GAP);
    }
    void *firstCharPos = &image[region.top() * display->columns()];
    void *lastCharPos = &image[(region.top() + abs(lines)) * display->columns()];

    const int top = display->contentRect().top() + (region.top() * display->terminalFont()->fontHeight());
    const int linesToMove = region.height() - abs(lines);
    const int bytesToMove = linesToMove * display->columns() * sizeof(Character);

    Q_ASSERT(linesToMove > 0);
    Q_ASSERT(bytesToMove > 0);

    scrollRect.setTop(lines > 0 ? top : top + abs(lines) * display->terminalFont()->fontHeight());
    scrollRect.setHeight(linesToMove * display->terminalFont()->fontHeight());

    if (!scrollRect.isValid() || scrollRect.isEmpty()) {
        return;
    }

    // scroll internal image
    if (lines > 0) {
        // check that the memory areas that we are going to move are valid
        Q_ASSERT((char *)lastCharPos + bytesToMove < (char *)(image + (display->lines() * display->columns())));
        Q_ASSERT((lines * display->columns()) < imageSize);

        // scroll internal image down
        memmove(firstCharPos, lastCharPos, bytesToMove);
    } else {
        // check that the memory areas that we are going to move are valid
        Q_ASSERT((char *)firstCharPos + bytesToMove < (char *)(image + (display->lines() * display->columns())));

        // scroll internal image up
        memmove(lastCharPos, firstCharPos, bytesToMove);
    }

    // scroll the display vertically to match internal _image
    display->scroll(0, display->terminalFont()->fontHeight() * (-lines), scrollRect);
}

void TerminalScrollBar::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::StyleChange) {
        updatePalette(_backgroundMatchingPalette);
    }
    QScrollBar::changeEvent(e);
}

void TerminalScrollBar::updatePalette(const QPalette &pal)
{
    _backgroundMatchingPalette = pal;

    auto proxyStyle = qobject_cast<const QProxyStyle *>(style());
    const QStyle *appStyle = proxyStyle ? proxyStyle->baseStyle() : style();

    // Scrollbars in widget styles like Fusion or Plastique do not work well with custom
    // scrollbar coloring, in particular in conjunction with light terminal background colors.
    // Use custom colors only for widget styles matched by the allowlist below, otherwise
    // fall back to generic widget colors.
    if (appStyle->objectName() == QLatin1String("breeze")) {
        setPalette(_backgroundMatchingPalette);
    } else {
        setPalette(QGuiApplication::palette());
    }
}

} // namespace Konsole

/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TERMINALSCROLLBAR_HPP
#define TERMINALSCROLLBAR_HPP

// Qt
#include <QScrollBar>

// Konsole
#include "Enumeration.h"
#include "ScreenWindow.h"
#include "ScrollState.h"
#include "extras/HighlightScrolledLines.h"
#include "konsoleprivate_export.h"

namespace Konsole
{
class TerminalDisplay;

class KONSOLEPRIVATE_EXPORT TerminalScrollBar : public QScrollBar
{
    Q_OBJECT
public:
    explicit TerminalScrollBar(QWidget *parent);

    /**
     * Specifies whether the terminal display has a vertical scroll bar, and if so whether it
     * is shown on the left or right side of the display.
     */
    void setScrollBarPosition(Enum::ScrollBarPositionEnum position);

    /**
     * Sets the current position and range of the display's scroll bar.
     *
     * @param cursor The position of the scroll bar's thumb.
     * @param slines The maximum value of the scroll bar.
     */
    void setScroll(int cursor, int slines);

    void setScrollFullPage(bool fullPage);
    bool scrollFullPage() const;
    void setHighlightScrolledLines(bool highlight);

    /** See setAlternateScrolling() */
    bool alternateScrolling() const;

    /**
     * Sets the AlternateScrolling profile property which controls whether
     * to emulate up/down key presses for mouse scroll wheel events.
     * For more details, check the documentation of that property in the
     * Profile header.
     * Enabled by default.
     */
    void setAlternateScrolling(bool enable);

    // applies changes to scrollbarLocation to the scroll bar and
    // if @propagate is true, propagates size information
    void applyScrollBarPosition(bool propagate = true);

    // scrolls the image by a number of lines.
    // 'lines' may be positive ( to scroll the image down )
    // or negative ( to scroll the image up )
    // 'region' is the part of the image to scroll - currently only
    // the top, bottom and height of 'region' are taken into account,
    // the left and right are ignored.
    void scrollImage(int lines, const QRect &screenWindowRegion, Character *image, int imageSize);

    Enum::ScrollBarPositionEnum scrollBarPosition() const
    {
        return _scrollbarLocation;
    }

    /**
     * Return the highlight line control
     */
    HighlightScrolledLines &highlightScrolledLines()
    {
        return _highlightScrolledLines;
    }

    void changeEvent(QEvent *e) override;

    void updatePalette(const QPalette &pal);

public Q_SLOTS:

    void scrollBarPositionChanged(int value);
    void highlightScrolledLinesEvent();

private:
    bool _scrollFullPage;
    bool _alternateScrolling;
    Enum::ScrollBarPositionEnum _scrollbarLocation;
    HighlightScrolledLines _highlightScrolledLines;
    QPalette _backgroundMatchingPalette;
};
} // namespace Konsole

#endif

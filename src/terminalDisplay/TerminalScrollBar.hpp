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

#ifndef TERMINALSCROLLBAR_HPP
#define TERMINALSCROLLBAR_HPP

// Qt
#include <QScrollBar>

// Konsole
#include "ScrollState.h"
#include "Enumeration.h"
#include "ScreenWindow.h"
#include "konsoleprivate_export.h"

namespace Konsole
{
    class TerminalDisplay;

    class KONSOLEPRIVATE_EXPORT TerminalScrollBar : public QScrollBar
    {
        Q_OBJECT
    public:
        explicit TerminalScrollBar(TerminalDisplay *display);

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
        void scrollImage(int lines, const QRect &screenWindowRegion);

        Enum::ScrollBarPositionEnum scrollBarPosition() const
        {
            return _scrollbarLocation;
        }
    public Q_SLOTS:

        void scrollBarPositionChanged(int value);
        void highlightScrolledLinesEvent();
    
    private:
        TerminalDisplay *_display;
        bool _scrollFullPage;
        bool _alternateScrolling;
        Enum::ScrollBarPositionEnum _scrollbarLocation;
    };

}

#endif

/*
    This file is part of Konsole, KDE's terminal emulator.

    Copyright 2012 by Jekyll Wu <adaptee@gmail.com>

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

#ifndef ENUMERATION_H
#define ENUMERATION_H

namespace Konsole
{
/**
 * This class serves as a place for putting enum definitions that are
 * used or referenced in multiple places in the code. Keep it small.
 */
class Enum
{
public:
    /**
     * This enum describes the modes available to remember lines of output
     * produced by the terminal.
     */
    enum HistoryModeEnum {
        /** No output is remembered.  As soon as lines of text are scrolled
         * off-screen they are lost.
         */
        NoHistory   = 0,
        /** A fixed number of lines of output are remembered.  Once the
         * limit is reached, the oldest lines are lost.
         */
        FixedSizeHistory = 1,
        /** All output is remembered for the duration of the session.
         * Typically this means that lines are recorded to
         * a file as they are scrolled off-screen.
         */
        UnlimitedHistory = 2
    };

    /**
     * This enum describes the positions where the terminal display's
     * scroll bar may be placed.
     */
    enum ScrollBarPositionEnum {
        /** Show the scroll-bar on the left of the terminal display. */
        ScrollBarLeft   = 0,
        /** Show the scroll-bar on the right of the terminal display. */
        ScrollBarRight  = 1,
        /** Do not show the scroll-bar. */
        ScrollBarHidden = 2
    };

    /** This enum describes the shapes used to draw the cursor in terminal
     * displays.
     */
    enum CursorShapeEnum {
        /** Use a solid rectangular block to draw the cursor. */
        BlockCursor     = 0,
        /** Use an 'I' shape, similar to that used in text editing
         * applications, to draw the cursor.
         */
        IBeamCursor     = 1,
        /** Draw a line underneath the cursor's position. */
        UnderlineCursor = 2
    };

    /** This enum describes the behavior of triple click action . */
    enum TripleClickModeEnum {
        /** Select the whole line underneath the cursor. */
        SelectWholeLine = 0,
        /** Select from the current cursor position to the end of the line. */
        SelectForwardsFromCursor = 1
    };

    /** This enum describes the source from which mouse middle click pastes data . */
    enum MiddleClickPasteModeEnum {
        /** Paste from X11 Selection. */
        PasteFromX11Selection = 0,
        /** Paste from Clipboard. */
        PasteFromClipboard = 1
    };

    /**
     * This enum describes the different types of sounds and visual effects which
     * can be used to alert the user when a 'bell' occurs in the terminal
     * session.
     */
    enum BellModeEnum {
        /** trigger system beep. */
        SystemBeepBell = 0,
        /** trigger system notification. */
        NotifyBell = 1,
        /** trigger visual bell(inverting the display's colors briefly). */
        VisualBell = 2,
        /** No bell effects */
        NoBell = 3
    };
};
}
#endif // ENUMERATION_H


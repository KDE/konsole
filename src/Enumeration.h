/*
    SPDX-FileCopyrightText: 2012 Jekyll Wu <adaptee@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
        NoHistory = 0,
        /** A fixed number of lines of output are remembered.  Once the
         * limit is reached, the oldest lines are lost.
         */
        FixedSizeHistory = 1,
        /** All output is remembered for the duration of the session.
         * Typically this means that lines are recorded to
         * a file as they are scrolled off-screen.
         */
        UnlimitedHistory = 2,
    };

    /**
     * This enum describes the positions where the terminal display's
     * scroll bar may be placed.
     */
    enum ScrollBarPositionEnum {
        /** Show the scroll-bar on the left of the terminal display. */
        ScrollBarLeft = 0,
        /** Show the scroll-bar on the right of the terminal display. */
        ScrollBarRight = 1,
        /** Do not show the scroll-bar. */
        ScrollBarHidden = 2,
    };

    /**
     * This enum describes the amount that Page Up/Down scroll by.
     */
    enum ScrollPageAmountEnum {
        /** Scroll half page */
        ScrollPageHalf = 0,
        /** Scroll full page */
        ScrollPageFull = 1,
    };

    /** This enum describes semantic hints appearance
     */
    enum SemanticHints {
        SemanticHintsNever = 0,
        SemanticHintsURL = 1,
        SemanticHintsAlways = 2,
    };

    /** This enum describes the shapes used to draw the cursor in terminal
     * displays.
     */
    enum CursorShapeEnum {
        /** Use a solid rectangular block to draw the cursor. */
        BlockCursor = 0,
        /** Use an 'I' shape, similar to that used in text editing
         * applications, to draw the cursor.
         */
        IBeamCursor = 1,
        /** Draw a line underneath the cursor's position. */
        UnderlineCursor = 2,
    };

    /** This enum describes the behavior of triple click action . */
    enum TripleClickModeEnum {
        /** Select the whole line underneath the cursor. */
        SelectWholeLine = 0,
        /** Select from the current cursor position to the end of the line. */
        SelectForwardsFromCursor = 1,
    };

    /** This enum describes the source from which mouse middle click pastes data . */
    enum MiddleClickPasteModeEnum {
        /** Paste from X11 Selection. */
        PasteFromX11Selection = 0,
        /** Paste from Clipboard. */
        PasteFromClipboard = 1,
    };

    /**
     * This enum describes the text editor cmd used to open local text file URLs
     * in Konsole, where line and column data are appended to the file URL, e.g.:
     * /path/to/file:123:123
     */
    enum TextEditorCmd {
        Kate = 0,
        KWrite,
        KDevelop,
        QtCreator,
        Gedit,
        gVim,
        CustomTextEditor,
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
        NoBell = 3,
    };

    /**
     * This enum describes the strategies available for searching
     * through the session's output.
     */
    enum SearchDirection {
        /** Searches forwards through the output, starting at the
         * current selection.
         */
        ForwardsSearch,
        /** Searches backwards through the output, starting at the
         * current selection.
         */
        BackwardsSearch,
    };
};
}
#endif // ENUMERATION_H

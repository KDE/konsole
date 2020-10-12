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

#ifndef TERMINALPAINTER_HPP
#define TERMINALPAINTER_HPP

// Qt
#include <QVector>
#include <QWidget>
#include <QPointer>
#include <QScrollBar>

// Konsole
#include "../characters/Character.h"
#include "ScreenWindow.h"
#include "Enumeration.h"
#include "colorscheme/ColorSchemeWallpaper.h"

class QRect;
class QColor;
class QRegion;
class QPainter;
class QString;
class QTimer;

namespace Konsole
{

    class Character;
    class TerminalDisplay;
    
    class TerminalPainter
    {
    public:
        explicit TerminalPainter(TerminalDisplay *display);
        ~TerminalPainter() = default;

        // -- Drawing helpers --

        // divides the part of the display specified by 'rect' into
        // fragments according to their colors and styles and calls
        // drawTextFragment() or drawPrinterFriendlyTextFragment()
        // to draw the fragments
        void drawContents(QPainter &paint, const QRect &rect, bool PrinterFriendly);
        // draw a transparent rectangle over the line of the current match
        void drawCurrentResultRect(QPainter &painter);
        // draw a thin highlight on the left of the screen for lines that have been scrolled into view
        void highlightScrolledLines(QPainter& painter);
        // compute which region need to be repainted for scrolled lines highlight
        QRegion highlightScrolledLinesRegion(bool nothingChanged);

        // draws a section of text, all the text in this section
        // has a common color and style
        void drawTextFragment(QPainter &painter, const QRect &rect, const QString &text,
                            const Character *style);

        void drawPrinterFriendlyTextFragment(QPainter &painter, const QRect &rect, const QString &text,
                                            const Character *style);
        // draws the background for a text fragment
        // if useOpacitySetting is true then the color's alpha value will be set to
        // the display's transparency (set with setOpacity()), otherwise the background
        // will be drawn fully opaque
        void drawBackground(QPainter &painter, const QRect &rect, const QColor &backgroundColor,
                            bool useOpacitySetting);
        // draws the cursor character
        void drawCursor(QPainter &painter, const QRect &rect, const QColor &foregroundColor,
                        const QColor &backgroundColor, QColor &characterColor);
        // draws the characters or line graphics in a text fragment
        void drawCharacters(QPainter &painter, const QRect &rect, const QString &text,
                            const Character *style, const QColor &characterColor);
        // draws a string of line graphics
        void drawLineCharString(QPainter &painter, int x, int y, const QString &str,
                                const Character *attributes);

        // draws the preedit string for input methods
        void drawInputMethodPreeditString(QPainter &painter, const QRect &rect);
    
    private:

        TerminalDisplay *_display;
    };

}

#endif

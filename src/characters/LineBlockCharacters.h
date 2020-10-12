/*
    This file is part of Konsole, a terminal emulator for KDE.

    Copyright 2019 by Mariusz Glebocki <mglb@arccos-1.net>

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

#ifndef LINEBLOCKCHARACTERS_H
#define LINEBLOCKCHARACTERS_H

// Qt
#include <QPainter>

namespace Konsole {

/**
 * Helper functions for drawing characters from "Box Drawing" and "Block Elements" Unicode blocks.
 */
namespace LineBlockCharacters {

    /**
     * Returns true if the character can be drawn by draw() function.
     *
     * @param ucs4cp Character to test's UCS4 code point
     */
    inline static bool canDraw(uint ucs4cp) {
        return (0x2500 <= ucs4cp && ucs4cp <= 0x259F);
    }

    /**
     * Draws character.
     *
     * @param paint QPainter to draw on
     * @param cellRect Rectangle to draw in
     * @param chr Character to be drawn
     * @param bold Whether the character should be boldface
     */
    void draw(QPainter &paint, const QRect &cellRect, const QChar &chr, bool bold);

} // namespace LineBlockCharacters
} // namespace Konsole

#endif // LINEBLOCKCHARACTERS_H

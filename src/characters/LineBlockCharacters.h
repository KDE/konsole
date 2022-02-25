/*
    SPDX-FileCopyrightText: 2019 Mariusz Glebocki <mglb@arccos-1.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef LINEBLOCKCHARACTERS_H
#define LINEBLOCKCHARACTERS_H

// Qt
#include <QPainter>

namespace Konsole
{
/**
 * Helper functions for drawing characters from "Box Drawing" and "Block Elements" Unicode blocks.
 */
namespace LineBlockCharacters
{
/**
 * Returns true if the character can be drawn by draw() function.
 *
 * @param ucs4cp Character to test's UCS4 code point
 */
inline static bool canDraw(uint ucs4cp)
{
    return (0x2500 <= ucs4cp && ucs4cp <= 0x259F);
}

/**
 * Returns true if the character is a Legacy Computing Symbol and can be drawn by draw() function.
 *
 * @param ucs4cp Character to test's UCS4 code point
 */
inline static bool isLegacyComputingSymbol(uint ucs4cp)
{
    return (0x1fb00 <= ucs4cp && ucs4cp <= 0x1fb8b);
}

/**
 * Draws character.
 *
 * @param paint QPainter to draw on
 * @param cellRect Rectangle to draw in
 * @param chr Character to be drawn
 * @param bold Whether the character should be boldface
 */
void draw(QPainter &paint, const QRect &cellRect, const uint &chr, bool bold);

} // namespace LineBlockCharacters
} // namespace Konsole

#endif // LINEBLOCKCHARACTERS_H

/*
    Copyright 2013 by Kurt Hindenburg <kurt.hindenburg@gmail.com>

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

// Own
#include "TerminalTest.h"

#include "qtest.h"

// Konsole
#include "../TerminalDisplay.h"
#include "../CharacterColor.h"
#include "../ColorScheme.h"

using namespace Konsole;

void TerminalTest::testScrollBarPositions()
{
    auto display = new TerminalDisplay(nullptr);

    // ScrollBar Positions
    display->setScrollBarPosition(Enum::ScrollBarLeft);
    QCOMPARE(display->scrollBarPosition(), Enum::ScrollBarLeft);
    display->setScrollBarPosition(Enum::ScrollBarRight);
    QCOMPARE(display->scrollBarPosition(), Enum::ScrollBarRight);
    display->setScrollBarPosition(Enum::ScrollBarHidden);
    QCOMPARE(display->scrollBarPosition(), Enum::ScrollBarHidden);

    delete display;
}

void TerminalTest::testColorTable()
{
    // These are from ColorScheme.cpp but they can be anything to test
    const ColorEntry defaultTable[TABLE_COLORS] = {
        ColorEntry(0x00, 0x00, 0x00), ColorEntry(0xFF, 0xFF, 0xFF),
        ColorEntry(0x00, 0x00, 0x00), ColorEntry(0xB2, 0x18, 0x18),
        ColorEntry(0x18, 0xB2, 0x18), ColorEntry(0xB2, 0x68, 0x18),
        ColorEntry(0x18, 0x18, 0xB2), ColorEntry(0xB2, 0x18, 0xB2),
        ColorEntry(0x18, 0xB2, 0xB2), ColorEntry(0xB2, 0xB2, 0xB2),
        ColorEntry(0x00, 0x00, 0x00), ColorEntry(0xFF, 0xFF, 0xFF),
        ColorEntry(0x68, 0x68, 0x68), ColorEntry(0xFF, 0x54, 0x54),
        ColorEntry(0x54, 0xFF, 0x54), ColorEntry(0xFF, 0xFF, 0x54),
        ColorEntry(0x54, 0x54, 0xFF), ColorEntry(0xFF, 0x54, 0xFF),
        ColorEntry(0x54, 0xFF, 0xFF), ColorEntry(0x00, 0xFF, 0xFF)
    };

    auto display = new TerminalDisplay(nullptr);

    display->setColorTable(defaultTable);

    const ColorEntry *colorTable = display->colorTable();

    for (int i = 0; i < TABLE_COLORS; i++) {
        QCOMPARE(colorTable[i], defaultTable[i]);
    }

    ColorEntry colorEntry = ColorEntry(0x00, 0x00, 0x00);
    QVERIFY(colorTable[1] != colorEntry);

    delete display;
}

void TerminalTest::testSize()
{
    auto display = new TerminalDisplay(nullptr);

    QCOMPARE(display->columns(), 1);
    QCOMPARE(display->lines(), 1);

    // TODO: setSize doesn't change size...
    //display->setSize(80, 25);

    delete display;
}

QTEST_MAIN(TerminalTest)

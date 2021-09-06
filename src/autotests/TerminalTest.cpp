/*
    SPDX-FileCopyrightText: 2013 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "TerminalTest.h"

#include "qtest.h"

// Konsole
#include "../terminalDisplay/TerminalColor.h"
#include "../terminalDisplay/TerminalDisplay.h"
#include "../terminalDisplay/TerminalScrollBar.h"

#include "../characters/CharacterColor.h"
#include "colorscheme/ColorScheme.h"

using namespace Konsole;

void TerminalTest::testScrollBarPositions()
{
    auto display = new TerminalDisplay(nullptr);

    // ScrollBar Positions
    display->scrollBar()->setScrollBarPosition(Enum::ScrollBarLeft);
    QCOMPARE(display->scrollBar()->scrollBarPosition(), Enum::ScrollBarLeft);
    display->scrollBar()->setScrollBarPosition(Enum::ScrollBarRight);
    QCOMPARE(display->scrollBar()->scrollBarPosition(), Enum::ScrollBarRight);
    display->scrollBar()->setScrollBarPosition(Enum::ScrollBarHidden);
    QCOMPARE(display->scrollBar()->scrollBarPosition(), Enum::ScrollBarHidden);

    delete display;
}

void TerminalTest::testColorTable()
{
    // These are from ColorScheme.cpp but they can be anything to test
    const QColor defaultTable[TABLE_COLORS] = {QColor(0x00, 0x00, 0x00), QColor(0xFF, 0xFF, 0xFF), QColor(0x00, 0x00, 0x00), QColor(0xB2, 0x18, 0x18),
                                               QColor(0x18, 0xB2, 0x18), QColor(0xB2, 0x68, 0x18), QColor(0x18, 0x18, 0xB2), QColor(0xB2, 0x18, 0xB2),
                                               QColor(0x18, 0xB2, 0xB2), QColor(0xB2, 0xB2, 0xB2), QColor(0x00, 0x00, 0x00), QColor(0xFF, 0xFF, 0xFF),
                                               QColor(0x68, 0x68, 0x68), QColor(0xFF, 0x54, 0x54), QColor(0x54, 0xFF, 0x54), QColor(0xFF, 0xFF, 0x54),
                                               QColor(0x54, 0x54, 0xFF), QColor(0xFF, 0x54, 0xFF), QColor(0x54, 0xFF, 0xFF), QColor(0x00, 0xFF, 0xFF)};

    auto display = new TerminalDisplay(nullptr);

    display->terminalColor()->setColorTable(defaultTable);

    const QColor *colorTable = display->terminalColor()->colorTable();

    for (int i = 0; i < TABLE_COLORS; i++) {
        QCOMPARE(colorTable[i], defaultTable[i]);
    }

    QColor colorEntry = QColor(0x00, 0x00, 0x00);
    QVERIFY(colorTable[1] != colorEntry);

    delete display;
}

void TerminalTest::testSize()
{
    auto display = new TerminalDisplay(nullptr);

    QCOMPARE(display->columns(), 1);
    QCOMPARE(display->lines(), 1);

    // TODO: setSize doesn't change size...
    // display->setSize(80, 25);

    delete display;
}

QTEST_MAIN(TerminalTest)

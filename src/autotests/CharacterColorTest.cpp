/*
    Copyright 2008 by Robert Knight <robertknight@gmail.com>

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
#include "CharacterColorTest.h"

// Qt
#include <QtCore/QSize>
#include <QtCore/QStringList>

// KDE
#include <qtest.h>

using namespace Konsole;

const ColorEntry CharacterColorTest::DefaultColorTable[TABLE_COLORS] = {
    ColorEntry(QColor(0x00, 0x00, 0x00)), // Dfore
    ColorEntry(QColor(0xFF, 0xFF, 0xFF)), // Dback
    ColorEntry(QColor(0x00, 0x00, 0x00)), // Black
    ColorEntry(QColor(0xB2, 0x18, 0x18)), // Red
    ColorEntry(QColor(0x18, 0xB2, 0x18)), // Green
    ColorEntry(QColor(0xB2, 0x68, 0x18)), // Yellow
    ColorEntry(QColor(0x18, 0x18, 0xB2)), // Blue
    ColorEntry(QColor(0xB2, 0x18, 0xB2)), // Magenta
    ColorEntry(QColor(0x18, 0xB2, 0xB2)), // Cyan
    ColorEntry(QColor(0xB2, 0xB2, 0xB2)), // White
    // intensive versions
    ColorEntry(QColor(0x00, 0x00, 0x00)),
    ColorEntry(QColor(0xFF, 0xFF, 0xFF)),
    ColorEntry(QColor(0x68, 0x68, 0x68)),
    ColorEntry(QColor(0xFF, 0x54, 0x54)),
    ColorEntry(QColor(0x54, 0xFF, 0x54)),
    ColorEntry(QColor(0xFF, 0xFF, 0x54)),
    ColorEntry(QColor(0x54, 0x54, 0xFF)),
    ColorEntry(QColor(0xFF, 0x54, 0xFF)),
    ColorEntry(QColor(0x54, 0xFF, 0xFF)),
    ColorEntry(QColor(0xFF, 0xFF, 0xFF))
};

void CharacterColorTest::init()
{
}

void CharacterColorTest::cleanup()
{
}

void CharacterColorTest::testColorEntry()
{
    ColorEntry black = ColorEntry(QColor(0x00, 0x00, 0x00));
    ColorEntry white = ColorEntry(QColor(0xFF, 0xFF, 0xFF));
    ColorEntry red   = ColorEntry(QColor(0xB2, 0x18, 0x18));
    ColorEntry green = ColorEntry(QColor(0x18, 0xB2, 0x18));

    // Test operator== operator!=
    QCOMPARE(black == white, false);
    QCOMPARE(black != white, true);
    QCOMPARE(red == green, false);
    QCOMPARE(red != green, true);

    ColorEntry blackBold = ColorEntry(QColor(0x00, 0x00, 0x00), ColorEntry::FontWeight::Bold);
    ColorEntry blackNormal = ColorEntry(QColor(0x00, 0x00, 0x00), ColorEntry::FontWeight::Normal);
    ColorEntry blackUseCurrentFormat = ColorEntry(QColor(0x00, 0x00, 0x00), ColorEntry::FontWeight::UseCurrentFormat);

    QCOMPARE(blackBold == white, false);
    QCOMPARE(blackBold != white, true);
    QCOMPARE(blackBold == black, false);
    QCOMPARE(blackBold != black, true);

    QCOMPARE(blackNormal == white, false);
    QCOMPARE(blackNormal != white, true);
    QCOMPARE(blackNormal == black, false);
    QCOMPARE(blackNormal != black, true);

    QCOMPARE(blackUseCurrentFormat == white, false);
    QCOMPARE(blackUseCurrentFormat != white, true);
    QCOMPARE(blackUseCurrentFormat == black, true);    // UseCurrentFormat is default
    QCOMPARE(blackUseCurrentFormat != black, false);

    QCOMPARE(blackUseCurrentFormat == blackBold, false);
    QCOMPARE(blackUseCurrentFormat != blackBold, true);

    QCOMPARE(blackUseCurrentFormat == blackNormal, false);
    QCOMPARE(blackUseCurrentFormat != blackNormal, true);

    QCOMPARE(blackBold == blackNormal, false);
    QCOMPARE(blackBold != blackNormal, true);

    QCOMPARE(red == green, false);
    QCOMPARE(red != green, true);

    // Test operator=
    ColorEntry tmpColorEntry;
    ColorEntry redBold = ColorEntry(QColor(0xB2, 0x18, 0x18), ColorEntry::FontWeight::Bold);
    tmpColorEntry = red;
    QCOMPARE(tmpColorEntry == red, true);
    QCOMPARE(red == tmpColorEntry, true);
    tmpColorEntry = redBold;
    QCOMPARE(tmpColorEntry == red, false);
    QCOMPARE(red == tmpColorEntry, false);
    QCOMPARE(tmpColorEntry != red, true);
    QCOMPARE(red != tmpColorEntry, true);

    // Test ColorEntry()
    ColorEntry defaultColorEntry = ColorEntry();
    QCOMPARE(defaultColorEntry != green, true);
    QCOMPARE(defaultColorEntry == black, true);
    QCOMPARE(defaultColorEntry != black, false);
    QCOMPARE(defaultColorEntry == blackUseCurrentFormat, true);

}

void CharacterColorTest::testDummyConstructor()
{
    CharacterColor charColor;
    QCOMPARE(charColor.isValid(), false);
}

void CharacterColorTest::testColorSpaceDefault_data()
{
    QTest::addColumn<int>("colorValue");
    QTest::addColumn<QColor>("expected");

    QTest::newRow("color 0") << 0 << DefaultColorTable[0].color;
    QTest::newRow("color 1") << 1 << DefaultColorTable[1].color;
}

void CharacterColorTest::testColorSpaceDefault()
{
    QFETCH(int, colorValue);
    QFETCH(QColor, expected);

    CharacterColor charColor(COLOR_SPACE_DEFAULT, colorValue);
    const QColor result = charColor.color(DefaultColorTable);

    QCOMPARE(result, expected);
}

void CharacterColorTest::testColorSpaceSystem_data()
{
    QTest::addColumn<int>("colorValue");
    QTest::addColumn<QColor>("expected");

    QTest::newRow("color 0") << 0 << DefaultColorTable[2 + 0].color;
    QTest::newRow("color 1") << 1 << DefaultColorTable[2 + 1].color;
    QTest::newRow("color 7") << 7 << DefaultColorTable[2 + 7].color;
}

void CharacterColorTest::testColorSpaceSystem()
{
    QFETCH(int, colorValue);
    QFETCH(QColor, expected);

    CharacterColor charColor(COLOR_SPACE_SYSTEM, colorValue);
    const QColor result = charColor.color(DefaultColorTable);

    QCOMPARE(result, expected);

    //CharacterColor charColor(COLOR_SPACE_SYSTEM, 5);
    //const QColor result = charColor.color(DefaultColorTable);
    //const QColor expected = DefaultColorTable[7].color;
    //QCOMPARE(result, expected);
}

QTEST_GUILESS_MAIN(CharacterColorTest)


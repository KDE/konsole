/*
    SPDX-FileCopyrightText: 2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "CharacterColorTest.h"

// Qt

// KDE
#include <qtest.h>

using namespace Konsole;

const QColor CharacterColorTest::DefaultColorTable[TABLE_COLORS] = {
    QColor(0x00, 0x00, 0x00), // Dfore
    QColor(0xFF, 0xFF, 0xFF), // Dback
    QColor(0x00, 0x00, 0x00), // Black
    QColor(0xB2, 0x18, 0x18), // Red
    QColor(0x18, 0xB2, 0x18), // Green
    QColor(0xB2, 0x68, 0x18), // Yellow
    QColor(0x18, 0x18, 0xB2), // Blue
    QColor(0xB2, 0x18, 0xB2), // Magenta
    QColor(0x18, 0xB2, 0xB2), // Cyan
    QColor(0xB2, 0xB2, 0xB2), // White
    // intensive versions
    QColor(0x00, 0x00, 0x00),
    QColor(0xFF, 0xFF, 0xFF),
    QColor(0x68, 0x68, 0x68),
    QColor(0xFF, 0x54, 0x54),
    QColor(0x54, 0xFF, 0x54),
    QColor(0xFF, 0xFF, 0x54),
    QColor(0x54, 0x54, 0xFF),
    QColor(0xFF, 0x54, 0xFF),
    QColor(0x54, 0xFF, 0xFF),
    QColor(0xFF, 0xFF, 0xFF),
    // Here are faint intensities, which may not be good.
    // faint versions
    QColor(0x00, 0x00, 0x00),
    QColor(0xFF, 0xFF, 0xFF),
    QColor(0x00, 0x00, 0x00),
    QColor(0x65, 0x00, 0x00),
    QColor(0x00, 0x65, 0x00),
    QColor(0x65, 0x5E, 0x00),
    QColor(0x00, 0x00, 0x65),
    QColor(0x65, 0x00, 0x65),
    QColor(0x00, 0x65, 0x65),
    QColor(0x65, 0x65, 0x65),
};

void CharacterColorTest::init()
{
}

void CharacterColorTest::cleanup()
{
}

void CharacterColorTest::testColorEntry()
{
    QColor black = QColor(0x00, 0x00, 0x00);
    QColor white = QColor(0xFF, 0xFF, 0xFF);
    QColor red = QColor(0xB2, 0x18, 0x18);
    QColor green = QColor(0x18, 0xB2, 0x18);

    // Test operator== operator!=
    QCOMPARE(black == white, false);
    QCOMPARE(black != white, true);
    QCOMPARE(red == green, false);
    QCOMPARE(red != green, true);

    QCOMPARE(red == green, false);
    QCOMPARE(red != green, true);

    // Test operator=
    QColor tmpColorEntry;
    tmpColorEntry = red;
    QCOMPARE(tmpColorEntry == red, true);
    QCOMPARE(red == tmpColorEntry, true);

    // Test QColor()
    QColor defaultColorEntry = QColor();
    QCOMPARE(defaultColorEntry != green, true);
    QCOMPARE(defaultColorEntry != black, true);
    QCOMPARE(defaultColorEntry.isValid(), false);
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

    QTest::newRow("color 0") << 0 << DefaultColorTable[0];
    QTest::newRow("color 1") << 1 << DefaultColorTable[1];
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

    QTest::newRow("color 0") << 0 << DefaultColorTable[2 + 0];
    QTest::newRow("color 1") << 1 << DefaultColorTable[2 + 1];
    QTest::newRow("color 7") << 7 << DefaultColorTable[2 + 7];
}

void CharacterColorTest::testColorSpaceSystem()
{
    QFETCH(int, colorValue);
    QFETCH(QColor, expected);

    CharacterColor charColor(COLOR_SPACE_SYSTEM, colorValue);
    const QColor result = charColor.color(DefaultColorTable);

    QCOMPARE(result, expected);
}

void CharacterColorTest::testColorSpaceRGB_data()
{
    QTest::addColumn<int>("colorValue");
    QTest::addColumn<QColor>("expected");

    // Pick colors to test or test all if needed
    for (const int i : {0, 1, 64, 127, 128, 215, 255}) {
        const QString name = QStringLiteral("color %1").arg(i);
        QTest::newRow(qPrintable(name)) << i << QColor(i >> 16, i >> 8, i);
    }
}

void CharacterColorTest::testColorSpaceRGB()
{
    QFETCH(int, colorValue);
    QFETCH(QColor, expected);

    CharacterColor charColor(COLOR_SPACE_RGB, colorValue);
    const QColor result = charColor.color(DefaultColorTable);

    QCOMPARE(result, expected);
}
void CharacterColorTest::testColor256_data()
{
    QTest::addColumn<int>("colorValue");
    QTest::addColumn<QColor>("expected");

    // This might be overkill
    for (int i = 0; i < 8; ++i) {
        const QString name = QStringLiteral("color256 color %1").arg(i);
        QTest::newRow(qPrintable(name)) << i << DefaultColorTable[i + 2];
    }
    for (int i = 8; i < 16; ++i) {
        const QString name = QStringLiteral("color256 color %1").arg(i);
        QTest::newRow(qPrintable(name)) << i << DefaultColorTable[i + 2 + 10 - 8];
    }
    for (int i = 16; i < 232; ++i) {
        const QString name = QStringLiteral("color256 color %1").arg(i);
        const auto u = i - 16;
        const auto color = QColor(((u / 36) % 6) != 0 ? (40 * ((u / 36) % 6) + 55) : 0,
                                  ((u / 6) % 6) != 0 ? (40 * ((u / 6) % 6) + 55) : 0,
                                  ((u / 1) % 6) != 0 ? (40 * ((u / 1) % 6) + 55) : 0);
        QTest::newRow(qPrintable(name)) << i << color;
    }
    for (int i = 232; i < 256; ++i) {
        const QString name = QStringLiteral("color256 color %1").arg(i);
        const auto gray = (i - 232) * 10 + 8;
        QTest::newRow(qPrintable(name)) << i << QColor(gray, gray, gray);
    }
}

void CharacterColorTest::testColor256()
{
    QFETCH(int, colorValue);
    QFETCH(QColor, expected);

    const QColor result = color256(colorValue, DefaultColorTable);
    QCOMPARE(result, expected);
}

QTEST_GUILESS_MAIN(CharacterColorTest)

/*
    SPDX-FileCopyrightText: 2019 Tomaz Canabrava <tomaz.canabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "CharacterTest.h"
#include "Character.h"

#include <QTest>
#include <cstdint>

void Konsole::CharacterTest::testCanBeGrouped()
{
    // Test for Capital Latin
    for (uint32_t c = U'A'; c <= U'Z'; ++c) {
        Konsole::Character uppercase_latin_char(c);
        const bool res = uppercase_latin_char.canBeGrouped(false, false);
        QVERIFY(res);
    }

    // Test for Non Capital Latin
    for (uint32_t c = U'a'; c <= U'z'; ++c) {
        Konsole::Character lowercase_latin_char(c);
        const bool res = lowercase_latin_char.canBeGrouped(false, false);
        QVERIFY(res);
    }

    // Test for Braille
    for (uint32_t c = 0x2800; c <= 0x28ff; ++c) {
        Konsole::Character braille_char(c);
        const bool res = braille_char.canBeGrouped(false, false);
        QCOMPARE(res, false);

        // TEST FOR REGRESSION - This was failing with different bidirectional modes.
        Konsole::Character braille_char_bidi_enabled(c);
        const bool res2 = braille_char_bidi_enabled.canBeGrouped(true, false);
        QCOMPARE(res2, false);
    }
}

QTEST_GUILESS_MAIN(Konsole::CharacterTest)

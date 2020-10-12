/*
    Copyright 2014 by Kurt Hindenburg <kurt.hindenburg@gmail.com>

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
#include "CharacterWidthTest.h"

// KDE
#include <qtest.h>

#include "../characters/Character.h"
#include "konsoleprivate_export.h"

using namespace Konsole;

void CharacterWidthTest::testWidth_data()
{
    QTest::addColumn<uint>("character");
    QTest::addColumn<int>("width");

    /* This is a work in progress.... */

    /* konsole_wcwidth uses -1 C0/C1 and DEL */
    QTest::newRow("0x007F") << uint(0x007F) << -1;

    QTest::newRow("0x0300") << uint(0x0300) << 0;
    QTest::newRow("0x070F") << uint(0x070F) << 0;
    QTest::newRow("0x1160") << uint(0x1160) << 0;
    QTest::newRow("0x10300") << uint(0x10300) << 1;

    QTest::newRow("a") << uint('a') << 1;
    QTest::newRow("0x00AD") << uint(0x00AD) << 1;
    QTest::newRow("0x00A0") << uint(0x00A0) << 1;
    QTest::newRow("0x10FB") << uint(0x10FB) << 1;
    QTest::newRow("0xFF76") << uint(0xFF76) << 1;
    QTest::newRow("0x103A0") << uint(0x103A0) << 1;
    QTest::newRow("0x10A06") << uint(0x10A06) << 0;

    QTest::newRow("0x3000") << uint(0x3000) << 2;
    QTest::newRow("0x300a") << uint(0x300a) << 2;
    QTest::newRow("0x300b") << uint(0x300b) << 2;
    QTest::newRow("0xFF01") << uint(0xFF01) << 2;
    QTest::newRow("0xFF5F") << uint(0xFF5F) << 2;
    QTest::newRow("0xFF60") << uint(0xFF60) << 2;
    QTest::newRow("0xFFe0") << uint(0xFFe6) << 2;

    QTest::newRow("0x1F943 tumbler glass") << uint(0x1F943) << 2;
    QTest::newRow("0x1F944 spoon") << uint(0x1F944) << 2;

    QTest::newRow("0x26A1  high voltage sign (BUG 378124)") << uint(0x026A1) << 2;
    QTest::newRow("0x2615  hot beverage (BUG 392171)") << uint(0x02615) << 2;
    QTest::newRow("0x26EA  church (BUG 392171)") << uint(0x026EA) << 2;
    QTest::newRow("0x1D11E musical symbol g clef (BUG 339439)") << uint(0x1D11E) << 1;

}

void CharacterWidthTest::testWidth()
{
    QFETCH(uint, character);

    QTEST(Character::width(character), "width");
}

QTEST_GUILESS_MAIN(CharacterWidthTest)

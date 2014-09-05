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
#include <qtest_kde.h>

#include "../konsole_wcwidth.h"
#include "../konsole_export.h"

using namespace Konsole;

void CharacterWidthTest::testWidth_data()
{
    QTest::addColumn<quint16>("character");
    QTest::addColumn<int>("width");

    /* This is a work in progress.... */

    /* konsole_wcwidth uses -1 C0/C1 and DEL */
    QTest::newRow("0xE007F") << quint16(0xE007F) << -1;

    QTest::newRow("0x0300") << quint16(0x0300) << 0;
    QTest::newRow("0x070F") << quint16(0x070F) << 0;
    QTest::newRow("0x1160") << quint16(0x1160) << 0;
    QTest::newRow("0x10300") << quint16(0x10300) << 0;

    QTest::newRow("a") << quint16('a') << 1;
    QTest::newRow("0x00AD") << quint16(0x00AD) << 1;
    QTest::newRow("0x00A0") << quint16(0x00A0) << 1;
    QTest::newRow("0x10FB") << quint16(0x10FB) << 1;
    QTest::newRow("0xFF76") << quint16(0xFF76) << 1;
    QTest::newRow("0x103A0") << quint16(0x103A0) << 1;
    QTest::newRow("0x10A06") << quint16(0x10A06) << 1;

    QTest::newRow("0x3000") << quint16(0x3000) << 2;
    QTest::newRow("0x300a") << quint16(0x300a) << 2;
    QTest::newRow("0x300b") << quint16(0x300b) << 2;
    QTest::newRow("0xFF01") << quint16(0xFF01) << 2;
    QTest::newRow("0xFF5F") << quint16(0xFF5F) << 2;
    QTest::newRow("0xFF60") << quint16(0xFF60) << 2;
    QTest::newRow("0xFFe0") << quint16(0xFFe6) << 2;
}

void CharacterWidthTest::testWidth()
{
    QFETCH(quint16, character);

    QTEST(konsole_wcwidth(character), "width");
}

QTEST_KDEMAIN_CORE(CharacterWidthTest)


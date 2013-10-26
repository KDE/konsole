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
#include "KeyboardTranslatorTest.h"

// KDE
#include <qtest_kde.h>

using namespace Konsole;

void KeyboardTranslatorTest::testEntryTextWildcards_data()
{
    // Shift   = 1 + (1 << 0) = 2
    // Alt     = 1 + (1 << 2) = 3
    // Control = 1 + (1 << 4) = 5

    QTest::addColumn<QByteArray>("text");
    QTest::addColumn<QByteArray>("result");
    QTest::addColumn<bool>("wildcards");
    QTest::addColumn<quint64>("modifiers"); // Qt::KeyboardModifers doesn't work here

    QTest::newRow("Home no wildcards no modifiers")<< QByteArray("Home") << QByteArray("Home") << false << static_cast<quint64>(Qt::NoModifier);
    QTest::newRow("Home no wildcards Shift modifiers")<< QByteArray("Home") << QByteArray("Home") << false << static_cast<quint64>(Qt::ShiftModifier);
    QTest::newRow("Home no wildcards Alt modifiers")<< QByteArray("Home") << QByteArray("Home") << false << static_cast<quint64>(Qt::AltModifier);
    QTest::newRow("Home no wildcards Control modifiers")<< QByteArray("Home") << QByteArray("Home") << false << static_cast<quint64>(Qt::ControlModifier);

    QTest::newRow("Home yes wildcards no modifiers")<< QByteArray("Home") << QByteArray("Home") << true << static_cast<quint64>(Qt::NoModifier);
    QTest::newRow("Home yes wildcards Shift modifiers")<< QByteArray("Home") << QByteArray("Home") << true << static_cast<quint64>(Qt::ShiftModifier);
    QTest::newRow("Home yes wildcards Alt modifiers")<< QByteArray("Home") << QByteArray("Home") << true << static_cast<quint64>(Qt::AltModifier);
    QTest::newRow("Home yes wildcards Control modifiers")<< QByteArray("Home") << QByteArray("Home") << true << static_cast<quint64>(Qt::ControlModifier);


    // text, results: no mod, shift, alt, control
    QList<QByteArray> entry;
    entry << QByteArray("E*") << QByteArray("E1") << QByteArray("E2") << QByteArray("E3") << QByteArray("E5");
    QTest::newRow("E* yes wildcards no modifiers")<< entry[0] << entry[1] << true << static_cast<quint64>(Qt::NoModifier);
    QTest::newRow("E* yes wildcards Shift modifiers")<< entry[0] << entry[2] << true << static_cast<quint64>(Qt::ShiftModifier);
    QTest::newRow("E* yes wildcards Alt modifiers")<< entry[0] << entry[3] << true << static_cast<quint64>(Qt::AltModifier);
    QTest::newRow("E* yes wildcards Control modifiers")<< entry[0] << entry[4] << true << static_cast<quint64>(Qt::ControlModifier);

    // combinations
    entry.clear();;
    entry << QByteArray("E*") << QByteArray("E4") << QByteArray("E6") << QByteArray("E8") << QByteArray("E7");
    QTest::newRow("E* yes wildcards Shift+Alt modifiers")<< entry[0] << entry[1] << true << static_cast<quint64>(Qt::ShiftModifier | Qt::AltModifier);
    QTest::newRow("E* yes wildcards Shift+Control modifiers")<< entry[0] << entry[2] << true << static_cast<quint64>(Qt::ShiftModifier | Qt::ControlModifier);
    QTest::newRow("E* yes wildcards Shift+Alt+Control modifiers")<< entry[0] << entry[3] << true << static_cast<quint64>(Qt::ShiftModifier | Qt::AltModifier | Qt::ControlModifier);
    QTest::newRow("E* yes wildcards Alt+Control modifiers")<< entry[0] << entry[4] << true << static_cast<quint64>(Qt::AltModifier | Qt::ControlModifier);

    // text, results: no mod, shift, alt, control
    entry.clear();;
    entry << QByteArray("\E[24;*~") << QByteArray("\E[24;1~") << QByteArray("\E[24;2~") << QByteArray("\E[24;3~") << QByteArray("\E[24;5~");
    QTest::newRow("\E[24;*~ yes wildcards no modifiers")<< entry[0] << entry[1] << true << static_cast<quint64>(Qt::NoModifier);
    QTest::newRow("\E[24;*~ yes wildcards Shift modifiers")<< entry[0] << entry[2] << true << static_cast<quint64>(Qt::ShiftModifier);
    QTest::newRow("\E[24;*~ yes wildcards Alt modifiers")<< entry[0] << entry[3] << true << static_cast<quint64>(Qt::AltModifier);
    QTest::newRow("\E[24;*~ yes wildcards Control modifiers")<< entry[0] << entry[4] << true << static_cast<quint64>(Qt::ControlModifier);

    // combinations
    entry.clear();;
    entry << QByteArray("\E[24;*~") << QByteArray("\E[24;4~") << QByteArray("\E[24;6~") << QByteArray("\E[24;8~") << QByteArray("\E[24;7~");
    QTest::newRow("\E[24;*~ yes wildcards Shift+Alt modifiers")<< entry[0] << entry[1] << true << static_cast<quint64>(Qt::ShiftModifier | Qt::AltModifier);
    QTest::newRow("\E[24;*~ yes wildcards Shift+Control modifiers")<< entry[0] << entry[2] << true << static_cast<quint64>(Qt::ShiftModifier | Qt::ControlModifier);
    QTest::newRow("\E[24;*~ yes wildcards Shift+Alt+Control modifiers")<< entry[0] << entry[3] << true << static_cast<quint64>(Qt::ShiftModifier | Qt::AltModifier | Qt::ControlModifier);
    QTest::newRow("\E[24;*~ yes wildcards Alt+Control modifiers")<< entry[0] << entry[4] << true << static_cast<quint64>(Qt::AltModifier | Qt::ControlModifier);

}

void KeyboardTranslatorTest::testEntryTextWildcards()
{
    QFETCH(QByteArray, text);
    QFETCH(QByteArray, result);
    QFETCH(bool, wildcards);
    QFETCH(quint64, modifiers);

    KeyboardTranslator::Entry entry;
    Qt::KeyboardModifiers keyboardModifiers = static_cast<Qt::KeyboardModifiers>(modifiers);
    entry.setText(text);

    QCOMPARE(entry.text(wildcards, keyboardModifiers), result);
}

QTEST_KDEMAIN_CORE(KeyboardTranslatorTest)

#include "KeyboardTranslatorTest.moc"


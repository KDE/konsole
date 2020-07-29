/*
    Copyright 2013, 2018 by Kurt Hindenburg <kurt.hindenburg@gmail.com>

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

#include "keyboardtranslator/FallbackKeyboardTranslator.h"
#include "keyboardtranslator/KeyboardTranslatorReader.h"

// KDE
#include <qtest.h>

using namespace Konsole;

Q_DECLARE_METATYPE(Qt::KeyboardModifiers)

void KeyboardTranslatorTest::testEntryTextWildcards_data()
{
    // Shift   = 1 + (1 << 0) = 2
    // Alt     = 1 + (1 << 2) = 3
    // Control = 1 + (1 << 4) = 5

    QTest::addColumn<QByteArray>("text");
    QTest::addColumn<QByteArray>("result");
    QTest::addColumn<bool>("wildcards");
    QTest::addColumn<Qt::KeyboardModifiers>("modifiers");

    QTest::newRow("Home no wildcards no modifiers")<< QByteArray("Home") << QByteArray("Home") << false << Qt::KeyboardModifiers(Qt::NoModifier);
    QTest::newRow("Home no wildcards Shift modifiers")<< QByteArray("Home") << QByteArray("Home") << false << Qt::KeyboardModifiers(Qt::ShiftModifier);
    QTest::newRow("Home no wildcards Alt modifiers")<< QByteArray("Home") << QByteArray("Home") << false << Qt::KeyboardModifiers(Qt::AltModifier);
    QTest::newRow("Home no wildcards Control modifiers")<< QByteArray("Home") << QByteArray("Home") << false << Qt::KeyboardModifiers(Qt::ControlModifier);

    QTest::newRow("Home yes wildcards no modifiers")<< QByteArray("Home") << QByteArray("Home") << true << Qt::KeyboardModifiers(Qt::NoModifier);
    QTest::newRow("Home yes wildcards Shift modifiers")<< QByteArray("Home") << QByteArray("Home") << true << Qt::KeyboardModifiers(Qt::ShiftModifier);
    QTest::newRow("Home yes wildcards Alt modifiers")<< QByteArray("Home") << QByteArray("Home") << true << Qt::KeyboardModifiers(Qt::AltModifier);
    QTest::newRow("Home yes wildcards Control modifiers")<< QByteArray("Home") << QByteArray("Home") << true << Qt::KeyboardModifiers(Qt::ControlModifier);


    // text, results: no mod, shift, alt, control
    QList<QByteArray> entry;
    entry << QByteArray("E*") << QByteArray("E1") << QByteArray("E2") << QByteArray("E3") << QByteArray("E5");
    QTest::newRow("E* yes wildcards no modifiers")<< entry[0] << entry[1] << true << Qt::KeyboardModifiers(Qt::NoModifier);
    QTest::newRow("E* yes wildcards Shift modifiers")<< entry[0] << entry[2] << true << Qt::KeyboardModifiers(Qt::ShiftModifier);
    QTest::newRow("E* yes wildcards Alt modifiers")<< entry[0] << entry[3] << true << Qt::KeyboardModifiers(Qt::AltModifier);
    QTest::newRow("E* yes wildcards Control modifiers")<< entry[0] << entry[4] << true << Qt::KeyboardModifiers(Qt::ControlModifier);

    // combinations
    entry.clear();
    entry << QByteArray("E*") << QByteArray("E4") << QByteArray("E6") << QByteArray("E8") << QByteArray("E7");
    QTest::newRow("E* yes wildcards Shift+Alt modifiers")<< entry[0] << entry[1] << true << Qt::KeyboardModifiers(Qt::ShiftModifier | Qt::AltModifier);
    QTest::newRow("E* yes wildcards Shift+Control modifiers")<< entry[0] << entry[2] << true << Qt::KeyboardModifiers(Qt::ShiftModifier | Qt::ControlModifier);
    QTest::newRow("E* yes wildcards Shift+Alt+Control modifiers")<< entry[0] << entry[3] << true << Qt::KeyboardModifiers(Qt::ShiftModifier | Qt::AltModifier | Qt::ControlModifier);
    QTest::newRow("E* yes wildcards Alt+Control modifiers")<< entry[0] << entry[4] << true << Qt::KeyboardModifiers(Qt::AltModifier | Qt::ControlModifier);

    // text, results: no mod, shift, alt, control
    entry.clear();
    entry << QByteArray("\033[24;*~") << QByteArray("\033[24;1~") << QByteArray("\033[24;2~") << QByteArray("\033[24;3~") << QByteArray("\033[24;5~");
    QTest::newRow("\033[24;*~ yes wildcards no modifiers")<< entry[0] << entry[1] << true << Qt::KeyboardModifiers(Qt::NoModifier);
    QTest::newRow("\033[24;*~ yes wildcards Shift modifiers")<< entry[0] << entry[2] << true << Qt::KeyboardModifiers(Qt::ShiftModifier);
    QTest::newRow("\033[24;*~ yes wildcards Alt modifiers")<< entry[0] << entry[3] << true << Qt::KeyboardModifiers(Qt::AltModifier);
    QTest::newRow("\033[24;*~ yes wildcards Control modifiers")<< entry[0] << entry[4] << true << Qt::KeyboardModifiers(Qt::ControlModifier);

    // combinations
    entry.clear();
    entry << QByteArray("\033[24;*~") << QByteArray("\033[24;4~") << QByteArray("\033[24;6~") << QByteArray("\033[24;8~") << QByteArray("\033[24;7~");
    QTest::newRow("\033[24;*~ yes wildcards Shift+Alt modifiers")<< entry[0] << entry[1] << true << Qt::KeyboardModifiers(Qt::ShiftModifier | Qt::AltModifier);
    QTest::newRow("\033[24;*~ yes wildcards Shift+Control modifiers")<< entry[0] << entry[2] << true << Qt::KeyboardModifiers(Qt::ShiftModifier | Qt::ControlModifier);
    QTest::newRow("\033[24;*~ yes wildcards Shift+Alt+Control modifiers")<< entry[0] << entry[3] << true << Qt::KeyboardModifiers(Qt::ShiftModifier | Qt::AltModifier | Qt::ControlModifier);
    QTest::newRow("\033[24;*~ yes wildcards Alt+Control modifiers")<< entry[0] << entry[4] << true << Qt::KeyboardModifiers(Qt::AltModifier | Qt::ControlModifier);

}

void KeyboardTranslatorTest::testEntryTextWildcards()
{
    QFETCH(QByteArray, text);
    QFETCH(QByteArray, result);
    QFETCH(bool, wildcards);
    QFETCH(Qt::KeyboardModifiers, modifiers);

    KeyboardTranslator::Entry entry;
    entry.setText(text);

    QCOMPARE(entry.text(wildcards, modifiers), result);
}

// Use FallbackKeyboardTranslator to test basic functionality
void KeyboardTranslatorTest::testFallback()
{
    auto fallback = new FallbackKeyboardTranslator();

    QCOMPARE(QStringLiteral("fallback"), fallback->name());
    QCOMPARE(QStringLiteral("Fallback Keyboard Translator"), fallback->description());

    auto entries = fallback->entries();
    QCOMPARE(1, entries.size());
    auto entry = fallback->findEntry(Qt::Key_Tab, Qt::NoModifier);
    QVERIFY(!entry.isNull());
    QCOMPARE(FallbackKeyboardTranslator::Command::NoCommand, entry.command());
    QCOMPARE(int(Qt::Key_Tab), entry.keyCode());
    QCOMPARE(QByteArray("\t"), entry.text());
    QCOMPARE(QByteArray("\\t"), entry.escapedText());
    QCOMPARE(Qt::KeyboardModifiers(Qt::NoModifier), entry.modifiers());
    QCOMPARE(Qt::KeyboardModifiers(Qt::NoModifier), entry.modifierMask());
    QCOMPARE(KeyboardTranslator::States(KeyboardTranslator::NoState), entry.state());
    QCOMPARE(QStringLiteral("Tab"), entry.conditionToString());
    QCOMPARE(QStringLiteral("\\t"), entry.resultToString());
    QVERIFY(entry.matches(Qt::Key_Tab, Qt::NoModifier, KeyboardTranslator::NoState));
    QVERIFY(entry == fallback->findEntry(Qt::Key_Tab, Qt::NoModifier));

    delete fallback;
}

void KeyboardTranslatorTest::testHexKeys()
{
    QFile linuxkeytab(QFINDTESTDATA(QStringLiteral("data/test.keytab")));
    QVERIFY(linuxkeytab.exists());
    QVERIFY(linuxkeytab.open(QIODevice::ReadOnly));

    KeyboardTranslator* translator = new KeyboardTranslator(QStringLiteral("testtranslator"));

    KeyboardTranslatorReader reader(&linuxkeytab);
    while (reader.hasNextEntry()) {
        translator->addEntry(reader.nextEntry());
    }
    linuxkeytab.close();

    // A worthless check ATM
    if (reader.parseError()) {
        delete translator;
        QFAIL("Parse failure");
    }

    QCOMPARE(QStringLiteral("testtranslator"), translator->name());
    QCOMPARE(QString(), translator->description());

    auto entry = translator->findEntry(Qt::Key_Backspace, Qt::NoModifier);
    QVERIFY(!entry.isNull());
    QCOMPARE(FallbackKeyboardTranslator::Command::NoCommand, entry.command());
    QCOMPARE(int(Qt::Key_Backspace), entry.keyCode());
    QCOMPARE(QByteArray("\x7F"), entry.text());
    QCOMPARE(QByteArray("\\x7f"), entry.escapedText());
    QCOMPARE(Qt::KeyboardModifiers(Qt::NoModifier), entry.modifiers());
    QCOMPARE(Qt::KeyboardModifiers(Qt::NoModifier), entry.modifierMask());
    QCOMPARE(KeyboardTranslator::States(KeyboardTranslator::NoState), entry.state());
    QCOMPARE(QStringLiteral("Backspace"), entry.conditionToString());
    QCOMPARE(QStringLiteral("\\x7f"), entry.resultToString());
    QVERIFY(entry.matches(Qt::Key_Backspace, Qt::NoModifier, KeyboardTranslator::NoState));
    QVERIFY(entry == translator->findEntry(Qt::Key_Backspace, Qt::NoModifier));

    entry = translator->findEntry(Qt::Key_Delete, Qt::NoModifier);
    QVERIFY(!entry.isNull());
    QCOMPARE(FallbackKeyboardTranslator::Command::NoCommand, entry.command());
    QCOMPARE(int(Qt::Key_Delete), entry.keyCode());
    QCOMPARE(QByteArray("\x08"), entry.text());
    QCOMPARE(QByteArray("\\b"), entry.escapedText());
    QCOMPARE(Qt::KeyboardModifiers(Qt::NoModifier), entry.modifiers());
    QCOMPARE(Qt::KeyboardModifiers(Qt::NoModifier), entry.modifierMask());
    QCOMPARE(KeyboardTranslator::States(KeyboardTranslator::NoState), entry.state());
    QCOMPARE(QStringLiteral("Del"), entry.conditionToString());
    QCOMPARE(QStringLiteral("\\b"), entry.resultToString());
    QVERIFY(entry.matches(Qt::Key_Delete, Qt::NoModifier, KeyboardTranslator::NoState));
    QVERIFY(!entry.matches(Qt::Key_Backspace, Qt::NoModifier, KeyboardTranslator::NoState));
    QVERIFY(!(entry == translator->findEntry(Qt::Key_Backspace, Qt::NoModifier)));

    entry = translator->findEntry(Qt::Key_Space, Qt::NoModifier);
    QVERIFY(!entry.isNull());
    QCOMPARE(FallbackKeyboardTranslator::Command::NoCommand, entry.command());
    QCOMPARE(int(Qt::Key_Space), entry.keyCode());
    QEXPECT_FAIL("", "Several keytabs use x00 as Space +Control;  text() fails", Continue);
    QCOMPARE(QByteArray("\x00"), entry.text());
    QCOMPARE(QByteArray("\\x00"), entry.escapedText());
    QCOMPARE(Qt::KeyboardModifiers(Qt::NoModifier), entry.modifiers());
    QCOMPARE(Qt::KeyboardModifiers(Qt::NoModifier), entry.modifierMask());
    QCOMPARE(KeyboardTranslator::States(KeyboardTranslator::NoState), entry.state());
    QCOMPARE(QStringLiteral("Space"), entry.conditionToString());
    QCOMPARE(QStringLiteral("\\x00"), entry.resultToString());
    QVERIFY(entry.matches(Qt::Key_Space, Qt::NoModifier, KeyboardTranslator::NoState));
    QVERIFY(entry == translator->findEntry(Qt::Key_Space, Qt::NoModifier));
    QVERIFY(!entry.matches(Qt::Key_Backspace, Qt::NoModifier, KeyboardTranslator::NoState));
    QVERIFY(!(entry == translator->findEntry(Qt::Key_Backspace, Qt::NoModifier)));

    delete translator;
}

QTEST_GUILESS_MAIN(KeyboardTranslatorTest)


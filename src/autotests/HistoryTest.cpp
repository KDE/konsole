/*
    SPDX-FileCopyrightText: 2013 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "HistoryTest.h"

#include "qtest.h"

// Konsole
#include "../Emulation.h"
#include "../history/HistoryScrollFile.h"
#include "../history/HistoryScrollNone.h"
#include "../history/HistoryTypeFile.h"
#include "../history/HistoryTypeNone.h"
#include "../history/compact/CompactHistoryScroll.h"
#include "../history/compact/CompactHistoryType.h"
#include "../session/Session.h"

using namespace Konsole;

void HistoryTest::testHistoryNone()
{
    HistoryType *history;

    history = new HistoryTypeNone();
    QCOMPARE(history->isEnabled(), false);
    QCOMPARE(history->isUnlimited(), false);
    QCOMPARE(history->maximumLineCount(), 0);
    delete history;
}

void HistoryTest::testHistoryFile()
{
    HistoryType *history;

    history = new HistoryTypeFile();
    QCOMPARE(history->isEnabled(), true);
    QCOMPARE(history->isUnlimited(), true);
    QCOMPARE(history->maximumLineCount(), -1);
    delete history;
}

void HistoryTest::testCompactHistory()
{
    HistoryType *history;

    history = new CompactHistoryType(42);
    QCOMPARE(history->isEnabled(), true);
    QCOMPARE(history->isUnlimited(), false);
    QCOMPARE(history->maximumLineCount(), 42);
    delete history;
}

void HistoryTest::testEmulationHistory()
{
    auto session = new Session();
    Emulation *emulation = session->emulation();

    const HistoryType &historyTypeDefault = emulation->history();
    QCOMPARE(historyTypeDefault.isEnabled(), false);
    QCOMPARE(historyTypeDefault.isUnlimited(), false);
    QCOMPARE(historyTypeDefault.maximumLineCount(), 0);

    emulation->setHistory(HistoryTypeNone());
    const HistoryType &historyTypeNone = emulation->history();
    QCOMPARE(historyTypeNone.isEnabled(), false);
    QCOMPARE(historyTypeNone.isUnlimited(), false);
    QCOMPARE(historyTypeNone.maximumLineCount(), 0);

    emulation->setHistory(HistoryTypeFile());
    const HistoryType &historyTypeFile = emulation->history();
    QCOMPARE(historyTypeFile.isEnabled(), true);
    QCOMPARE(historyTypeFile.isUnlimited(), true);
    QCOMPARE(historyTypeFile.maximumLineCount(), -1);

    emulation->setHistory(CompactHistoryType(42));
    const HistoryType &compactHistoryType = emulation->history();
    QCOMPARE(compactHistoryType.isEnabled(), true);
    QCOMPARE(compactHistoryType.isUnlimited(), false);
    QCOMPARE(compactHistoryType.maximumLineCount(), 42);

    delete session;
}

void HistoryTest::testHistoryScroll()
{
    HistoryScroll *historyScroll;

    // None
    historyScroll = new HistoryScrollNone();
    QVERIFY(!historyScroll->hasScroll());
    QCOMPARE(historyScroll->getLines(), 0);

    const HistoryType &historyTypeNone = historyScroll->getType();
    QCOMPARE(historyTypeNone.isEnabled(), false);
    QCOMPARE(historyTypeNone.isUnlimited(), false);
    QCOMPARE(historyTypeNone.maximumLineCount(), 0);

    delete historyScroll;

    // File
    historyScroll = new HistoryScrollFile();
    QVERIFY(historyScroll->hasScroll());
    QCOMPARE(historyScroll->getLines(), 0);

    const HistoryType &historyTypeFile = historyScroll->getType();
    QCOMPARE(historyTypeFile.isEnabled(), true);
    QCOMPARE(historyTypeFile.isUnlimited(), true);
    QCOMPARE(historyTypeFile.maximumLineCount(), -1);

    delete historyScroll;

    // Compact
    historyScroll = new CompactHistoryScroll(42);
    QVERIFY(historyScroll->hasScroll());
    QCOMPARE(historyScroll->getLines(), 0);

    const HistoryType &compactHistoryType = historyScroll->getType();
    QCOMPARE(compactHistoryType.isEnabled(), true);
    QCOMPARE(compactHistoryType.isUnlimited(), false);
    QCOMPARE(compactHistoryType.maximumLineCount(), 42);

    delete historyScroll;
}

void HistoryTest::testHistoryReflow()
{
    HistoryScroll *historyScroll;
    const char testString[] = "abcdefghijklmnopqrstuvwxyz1234567890";
    const int testStringSize = sizeof(testString) / sizeof(char) - 1;
    auto testImage = new Character[testStringSize];
    Character testChar;
    for (int i = 0; i < testStringSize; i++) {
        testImage[i] = Character((uint)testString[i]);
    }

    // None
    historyScroll = new HistoryScrollNone();
    QCOMPARE(historyScroll->getMaxLines(), 0);
    QCOMPARE(historyScroll->reflowLines(10), 0);

    delete historyScroll;

    // Compact
    historyScroll = new CompactHistoryScroll(10);
    QCOMPARE(historyScroll->getMaxLines(), 10);
    historyScroll->addCells(testImage, testStringSize);
    historyScroll->addLine(false);
    QCOMPARE(historyScroll->getLines(), 1);
    QCOMPARE(historyScroll->reflowLines(10), 0);
    QCOMPARE(historyScroll->getLines(), 4);
    QCOMPARE(historyScroll->reflowLines(1), 26);
    QCOMPARE(historyScroll->getLines(), 10);
    QCOMPARE(historyScroll->getLineLen(5), 1);
    historyScroll->getCells(3, 0, 1, &testChar);
    QCOMPARE(testChar, testImage[testStringSize - 7]);
    historyScroll->getCells(0, 0, 1, &testChar);
    QCOMPARE(testChar, testImage[testStringSize - 10]);
    historyScroll->getCells(9, 0, 1, &testChar);
    QCOMPARE(testChar, testImage[testStringSize - 1]);

    delete historyScroll;

    // File
    historyScroll = new HistoryScrollFile();
    QCOMPARE(historyScroll->getMaxLines(), 0);
    historyScroll->addCells(testImage, testStringSize);
    historyScroll->addLine(false);
    QCOMPARE(historyScroll->getLines(), 1);
    QCOMPARE(historyScroll->getMaxLines(), 1);
    QCOMPARE(historyScroll->reflowLines(10), 0);
    QCOMPARE(historyScroll->getLines(), 4);
    QCOMPARE(historyScroll->getMaxLines(), 4);
    QCOMPARE(historyScroll->reflowLines(1), 0);
    QCOMPARE(historyScroll->getLines(), testStringSize);
    QCOMPARE(historyScroll->getLineLen(5), 1);
    historyScroll->getCells(3, 0, 1, &testChar);
    QCOMPARE(testChar, testImage[3]);
    historyScroll->getCells(0, 0, 1, &testChar);
    QCOMPARE(testChar, testImage[0]);
    historyScroll->getCells(testStringSize - 1, 0, 1, &testChar);
    QCOMPARE(testChar, testImage[testStringSize - 1]);

    delete historyScroll;
    delete[] testImage;
}

void HistoryTest::testHistoryTypeChange()
{
    std::unique_ptr<HistoryScroll> historyScroll(nullptr);

    const char testString[] = "abcdefghijklmnopqrstuvwxyz1234567890";
    const int testStringSize = sizeof(testString) / sizeof(char) - 1;
    auto testImage = new Character[testStringSize];
    Character testChar;
    for (int i = 0; i < testStringSize; i++) {
        testImage[i] = Character((uint)testString[i]);
    }

    // None
    auto historyTypeNone = new HistoryTypeNone();
    historyTypeNone->scroll(historyScroll);

    // None to File
    auto historyTypeFile = new HistoryTypeFile();
    historyTypeFile->scroll(historyScroll);

    historyScroll->addCells(testImage, testStringSize);
    historyScroll->addLine(false);
    QCOMPARE(historyScroll->reflowLines(1), 0);
    QCOMPARE(historyScroll->getLines(), testStringSize);
    historyScroll->getCells(0, 0, 1, &testChar);
    QCOMPARE(testChar, testImage[0]);

    // File to Compact
    auto compactHistoryType = new CompactHistoryType(10);
    compactHistoryType->scroll(historyScroll);

    QCOMPARE(historyScroll->getLines(), 10);
    historyScroll->getCells(0, 0, 1, &testChar);
    QCOMPARE(testChar, testImage[testStringSize - 10]);

    // Compact to File
    historyTypeFile->scroll(historyScroll);
    QCOMPARE(historyScroll->getLines(), 10);
    historyScroll->getCells(0, 0, 1, &testChar);
    QCOMPARE(testChar, testImage[testStringSize - 10]);

    // File to None
    historyTypeNone->scroll(historyScroll);
    QCOMPARE(historyScroll->getLines(), 0);

    delete historyTypeFile;
    delete historyTypeNone;
    delete compactHistoryType;
    delete[] testImage;
}

QTEST_MAIN(HistoryTest)

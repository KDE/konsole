/*
    SPDX-FileCopyrightText: 2013 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "HistoryTest.h"

#include <QTest>

// Konsole
#include "../Emulation.h"
#include "../session/Session.h"

using namespace Konsole;

void HistoryTest::initTestCase()
{
}

void HistoryTest::cleanupTestCase()
{
    delete[] testImage;
}

void HistoryTest::testHistoryNone()
{
    std::unique_ptr<HistoryType> historyTypeNone = std::make_unique<HistoryTypeNone>();
    QCOMPARE(historyTypeNone->isEnabled(), false);
    QCOMPARE(historyTypeNone->isUnlimited(), false);
    QCOMPARE(historyTypeNone->maximumLineCount(), 0);
}

void HistoryTest::testHistoryFile()
{
    std::unique_ptr<HistoryType> historyTypeFile = std::make_unique<HistoryTypeFile>();
    QCOMPARE(historyTypeFile->isEnabled(), true);
    QCOMPARE(historyTypeFile->isUnlimited(), true);
    QCOMPARE(historyTypeFile->maximumLineCount(), -1);
}

void HistoryTest::testCompactHistory()
{
    std::unique_ptr<HistoryType> historyTypeCompact = std::make_unique<CompactHistoryType>(42);
    QCOMPARE(historyTypeCompact->isEnabled(), true);
    QCOMPARE(historyTypeCompact->isUnlimited(), false);
    QCOMPARE(historyTypeCompact->maximumLineCount(), 42);
}

void HistoryTest::testEmulationHistory()
{
    auto session = std::make_unique<Session>();
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
    testImage = new Character[testStringSize];
    Character testChar;
    for (int i = 0; i < testStringSize; i++) {
        testImage[i] = Character((uint)testString[i]);
    }

    // None
    auto historyScrollNone = std::unique_ptr<HistoryScrollNone>(new HistoryScrollNone());
    QCOMPARE(historyScrollNone->getMaxLines(), 0);
    QCOMPARE(historyScrollNone->reflowLines(10), 0);

    // Compact
    auto compactHistoryScroll = std::unique_ptr<CompactHistoryScroll>(new CompactHistoryScroll(10));

    QCOMPARE(compactHistoryScroll->getMaxLines(), 10);
    compactHistoryScroll->addCells(testImage, testStringSize);
    compactHistoryScroll->addLine(false);
    QCOMPARE(compactHistoryScroll->getLines(), 1);
    QCOMPARE(compactHistoryScroll->reflowLines(10), 0);
    QCOMPARE(compactHistoryScroll->getLines(), 4);
    QCOMPARE(compactHistoryScroll->reflowLines(1), 26);
    QCOMPARE(compactHistoryScroll->getLines(), 10);
    QCOMPARE(compactHistoryScroll->getLineLen(5), 1);
    compactHistoryScroll->getCells(3, 0, 1, &testChar);
    QCOMPARE(testChar, testImage[testStringSize - 7]);
    compactHistoryScroll->getCells(0, 0, 1, &testChar);
    QCOMPARE(testChar, testImage[testStringSize - 10]);
    compactHistoryScroll->getCells(9, 0, 1, &testChar);
    QCOMPARE(testChar, testImage[testStringSize - 1]);

    // File
    auto historyScrollFile = std::unique_ptr<HistoryScrollFile>(new HistoryScrollFile());

    QCOMPARE(historyScrollFile->getMaxLines(), 0);
    historyScrollFile->addCells(testImage, testStringSize);
    historyScrollFile->addLine(false);
    QCOMPARE(historyScrollFile->getLines(), 1);
    QCOMPARE(historyScrollFile->getMaxLines(), 1);
    QCOMPARE(historyScrollFile->reflowLines(10), 0);
    QCOMPARE(historyScrollFile->getLines(), 4);
    QCOMPARE(historyScrollFile->getMaxLines(), 4);
    QCOMPARE(historyScrollFile->reflowLines(1), 0);
    QCOMPARE(historyScrollFile->getLines(), testStringSize);
    QCOMPARE(historyScrollFile->getLineLen(5), 1);
    historyScrollFile->getCells(3, 0, 1, &testChar);
    QCOMPARE(testChar, testImage[3]);
    historyScrollFile->getCells(0, 0, 1, &testChar);
    QCOMPARE(testChar, testImage[0]);
    historyScrollFile->getCells(testStringSize - 1, 0, 1, &testChar);
    QCOMPARE(testChar, testImage[testStringSize - 1]);
}

void HistoryTest::testHistoryTypeChange()
{
    std::unique_ptr<HistoryScroll> historyScroll(nullptr);

    const char testString[] = "abcdefghijklmnopqrstuvwxyz1234567890";
    const int testStringSize = sizeof(testString) / sizeof(char) - 1;
    auto testImage = std::make_unique<Character[]>(testStringSize);
    Character testChar;
    for (int i = 0; i < testStringSize; i++) {
        testImage[i] = Character((uint)testString[i]);
    }

    // None
    auto historyTypeNone = std::make_unique<HistoryTypeNone>();
    historyTypeNone->scroll(historyScroll);

    // None to File
    auto historyTypeFile = std::make_unique<HistoryTypeFile>();
    historyTypeFile->scroll(historyScroll);

    historyScroll->addCells(testImage.get(), testStringSize);
    historyScroll->addLine(false);
    QCOMPARE(historyScroll->reflowLines(1), 0);
    QCOMPARE(historyScroll->getLines(), testStringSize);
    historyScroll->getCells(0, 0, 1, &testChar);
    QCOMPARE(testChar, testImage[0]);

    // File to Compact
    auto compactHistoryType = std::make_unique<CompactHistoryType>(10);
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
}

QTEST_MAIN(HistoryTest)

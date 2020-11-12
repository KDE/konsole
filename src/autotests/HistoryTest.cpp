/*
    SPDX-FileCopyrightText: 2013 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "HistoryTest.h"

#include "qtest.h"

// Konsole
#include "../session/Session.h"
#include "../Emulation.h"
#include "../history/HistoryTypeNone.h"
#include "../history/HistoryTypeFile.h"
#include "../history/compact/CompactHistoryType.h"
#include "../history/HistoryScrollNone.h"
#include "../history/HistoryScrollFile.h"
#include "../history/compact/CompactHistoryScroll.h"

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
    QCOMPARE(historyScroll->getLineLen(0), 0);
    QCOMPARE(historyScroll->getLineLen(10), 0);

    const HistoryType &historyTypeNone = historyScroll->getType();
    QCOMPARE(historyTypeNone.isEnabled(), false);
    QCOMPARE(historyTypeNone.isUnlimited(), false);
    QCOMPARE(historyTypeNone.maximumLineCount(), 0);

    delete historyScroll;

    // File
    historyScroll = new HistoryScrollFile();
    QVERIFY(historyScroll->hasScroll());
    QCOMPARE(historyScroll->getLines(), 0);
    QCOMPARE(historyScroll->getLineLen(0), 0);
    QCOMPARE(historyScroll->getLineLen(10), 0);

    const HistoryType &historyTypeFile = historyScroll->getType();
    QCOMPARE(historyTypeFile.isEnabled(), true);
    QCOMPARE(historyTypeFile.isUnlimited(), true);
    QCOMPARE(historyTypeFile.maximumLineCount(), -1);

    delete historyScroll;

    // Compact
    historyScroll = new CompactHistoryScroll(42);
    QVERIFY(historyScroll->hasScroll());
    QCOMPARE(historyScroll->getLines(), 0);
    QCOMPARE(historyScroll->getLineLen(0), 0);
    QCOMPARE(historyScroll->getLineLen(10), 0);

    const HistoryType &compactHistoryType = historyScroll->getType();
    QCOMPARE(compactHistoryType.isEnabled(), true);
    QCOMPARE(compactHistoryType.isUnlimited(), false);
    QCOMPARE(compactHistoryType.maximumLineCount(), 42);

    delete historyScroll;
}

QTEST_MAIN(HistoryTest)

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
#include "HistoryTest.h"

#include "qtest_kde.h"

// Konsole
#include "../Session.h"
#include "../Emulation.h"
#include "../History.h"

using namespace Konsole;

void HistoryTest::testHistoryNone()
{
    HistoryType* history;

    history = new HistoryTypeNone();
    QCOMPARE(history->isEnabled(), false);
    QCOMPARE(history->isUnlimited(), false);
    QCOMPARE(history->maximumLineCount(), 0);
    delete history;
}

void HistoryTest::testHistoryFile()
{
    HistoryType* history;

    history = new HistoryTypeFile();
    QCOMPARE(history->isEnabled(), true);
    QCOMPARE(history->isUnlimited(), true);
    QCOMPARE(history->maximumLineCount(), -1);
    delete history;
}

void HistoryTest::testCompactHistory()
{
    HistoryType* history;

    history = new CompactHistoryType(42);
    QCOMPARE(history->isEnabled(), true);
    QCOMPARE(history->isUnlimited(), false);
    QCOMPARE(history->maximumLineCount(), 42);
    delete history;
}

void HistoryTest::testEmulationHistory()
{
    Session* session = new Session();
    Emulation* emulation = session->emulation();

    const HistoryType& historyTypeDefault = emulation->history();
    QCOMPARE(historyTypeDefault.isEnabled(), false);
    QCOMPARE(historyTypeDefault.isUnlimited(), false);
    QCOMPARE(historyTypeDefault.maximumLineCount(), 0);

    emulation->setHistory(HistoryTypeNone());
    const HistoryType& historyTypeNone = emulation->history();
    QCOMPARE(historyTypeNone.isEnabled(), false);
    QCOMPARE(historyTypeNone.isUnlimited(), false);
    QCOMPARE(historyTypeNone.maximumLineCount(), 0);

    emulation->setHistory(HistoryTypeFile());
    const HistoryType& historyTypeFile = emulation->history();
    QCOMPARE(historyTypeFile.isEnabled(), true);
    QCOMPARE(historyTypeFile.isUnlimited(), true);
    QCOMPARE(historyTypeFile.maximumLineCount(), -1);

    emulation->setHistory(CompactHistoryType(42));
    const HistoryType& compactHistoryType = emulation->history();
    QCOMPARE(compactHistoryType.isEnabled(), true);
    QCOMPARE(compactHistoryType.isUnlimited(), false);
    QCOMPARE(compactHistoryType.maximumLineCount(), 42);


    delete session;
}

void HistoryTest::testHistoryScroll()
{
    HistoryScroll* historyScroll;

    // None
    historyScroll = new HistoryScrollNone();
    QVERIFY(!historyScroll->hasScroll());
    QCOMPARE(historyScroll->getLines(), 0);
    QCOMPARE(historyScroll->getLineLen(0), 0);
    QCOMPARE(historyScroll->getLineLen(10), 0);

    const HistoryType& historyTypeNone = historyScroll->getType();
    QCOMPARE(historyTypeNone.isEnabled(), false);
    QCOMPARE(historyTypeNone.isUnlimited(), false);
    QCOMPARE(historyTypeNone.maximumLineCount(), 0);

    // File
    historyScroll = new HistoryScrollFile(QString("test.log"));
    QVERIFY(historyScroll->hasScroll());
    QCOMPARE(historyScroll->getLines(), 0);
    QCOMPARE(historyScroll->getLineLen(0), 0);
    QCOMPARE(historyScroll->getLineLen(10), 0);

    const HistoryType& historyTypeFile = historyScroll->getType();
    QCOMPARE(historyTypeFile.isEnabled(), true);
    QCOMPARE(historyTypeFile.isUnlimited(), true);
    QCOMPARE(historyTypeFile.maximumLineCount(), -1);

    // Compact
    historyScroll = new CompactHistoryScroll(42);
    QVERIFY(historyScroll->hasScroll());
    QCOMPARE(historyScroll->getLines(), 0);
    QCOMPARE(historyScroll->getLineLen(0), 0);
    QCOMPARE(historyScroll->getLineLen(10), 0);

    const HistoryType& compactHistoryType = historyScroll->getType();
    QCOMPARE(compactHistoryType.isEnabled(), true);
    QCOMPARE(compactHistoryType.isUnlimited(), false);
    QCOMPARE(compactHistoryType.maximumLineCount(), 42);

    delete historyScroll;
}

QTEST_KDEMAIN(HistoryTest , GUI)


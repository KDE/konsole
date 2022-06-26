/*
    SPDX-FileCopyrightText: 2013 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef HISTORYTEST_H
#define HISTORYTEST_H

#include <kde_terminal_interface.h>

#include "../characters/Character.h"
#include "../history/HistoryScrollFile.h"
#include "../history/HistoryScrollNone.h"
#include "../history/HistoryTypeFile.h"
#include "../history/HistoryTypeNone.h"
#include "../history/compact/CompactHistoryScroll.h"
#include "../history/compact/CompactHistoryType.h"

namespace Konsole
{
class HistoryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testHistoryNone();
    void testHistoryFile();
    void testCompactHistory();
    void testEmulationHistory();
    void testHistoryScroll();
    void testHistoryReflow();
    void testHistoryTypeChange();

private:
    static constexpr const char testString[] = "abcdefghijklmnopqrstuvwxyz1234567890";
    static constexpr const int testStringSize = sizeof(testString) / sizeof(char) - 1;
    Character *testImage = nullptr;
};

}

#endif // HISTORYTEST_H

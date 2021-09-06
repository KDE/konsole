/*
    SPDX-FileCopyrightText: 2013 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef HISTORYTEST_H
#define HISTORYTEST_H

#include <kde_terminal_interface.h>

namespace Konsole
{
class HistoryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testHistoryNone();
    void testHistoryFile();
    void testCompactHistory();
    void testEmulationHistory();
    void testHistoryScroll();
    void testHistoryReflow();
    void testHistoryTypeChange();

private:
};

}

#endif // HISTORYTEST_H

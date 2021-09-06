/*
    SPDX-FileCopyrightText: 2013 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TERMINALTEST_H
#define TERMINALTEST_H

#include <kde_terminal_interface.h>

namespace Konsole
{
class TerminalTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testScrollBarPositions();
    void testColorTable();
    void testSize();

private:
};

}

#endif // TERMINALTEST_H

/*
    SPDX-FileCopyrightText: 2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PTYTEST_H
#define PTYTEST_H

#include "../Pty.h"

namespace Konsole
{
class PtyTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();

    void testFlowControl();
    void testEraseChar();
    void testUseUtmp();
    void testWindowSize();

    void testRunProgram();
};

}

#endif // PTYTEST_H

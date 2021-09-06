/*
    SPDX-FileCopyrightText: 2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "PtyTest.h"

// Qt
#include <QSize>
#include <QStringList>

// KDE
#include <kcoreaddons_version.h>
#include <qtest.h>

using namespace Konsole;

void PtyTest::init()
{
}

void PtyTest::cleanup()
{
}

void PtyTest::testFlowControl()
{
    Pty pty;
    const bool input = true;
    pty.setFlowControlEnabled(input);
    const bool output = pty.flowControlEnabled();
    QCOMPARE(output, input);
}

void PtyTest::testEraseChar()
{
    Pty pty;
    const char input = 'x';
    pty.setEraseChar(input);
    const char output = pty.eraseChar();
    QCOMPARE(output, input);
}

void PtyTest::testUseUtmp()
{
    Pty pty;
    const bool input = true;
    pty.setUseUtmp(input);
    const bool output = pty.isUseUtmp();
    QCOMPARE(output, input);
}

void PtyTest::testWindowSize()
{
    Pty pty;
    QSize input(80, 40);
    pty.setWindowSize(input.width(), input.height());
    QSize output = pty.windowSize();
    QCOMPARE(output, input);
}

void PtyTest::testRunProgram()
{
    Pty pty;
    QString program = QStringLiteral("sh");
    QStringList arguments;
    arguments << program;
    QStringList environments;
    const int result = pty.start(program, arguments, environments);

    QCOMPARE(result, 0);

    QCOMPARE(pty.foregroundProcessGroup(), pty.processId());
}

QTEST_GUILESS_MAIN(PtyTest)

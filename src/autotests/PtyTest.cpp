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
#include <qtest.h>
#include <kcoreaddons_version.h>

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

    // since there is no other processes using this pty, the two methods
    // should return the same pid.
#if KCOREADDONS_VERSION >= QT_VERSION_CHECK(5, 78, 0)
    QCOMPARE(pty.foregroundProcessGroup(), pty.processId());
#else
    QCOMPARE(pty.foregroundProcessGroup(), pty.pid());
#endif
}

QTEST_GUILESS_MAIN(PtyTest)

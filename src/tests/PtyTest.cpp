/*
    Copyright 2008 by Robert Knight <robertknight@gmail.com>

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
#include "PtyTest.h"

// Qt
#include <QtCore/QSize>
#include <QtCore/QStringList>

// KDE
#include <qtest_kde.h>

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
    QString program = "sh";
    QStringList arguments ;
    arguments << program;
    QStringList environments;
    const int result = pty.start(program, arguments, environments);

    QCOMPARE(result, 0);

    // since there is no other processes using this pty, the two methods
    // should return the same pid.
    QCOMPARE(pty.foregroundProcessGroup(), pty.pid());
}

QTEST_KDEMAIN_CORE(PtyTest)

#include "PtyTest.moc"


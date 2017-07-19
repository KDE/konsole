/*
    Copyright 2008 by Robert Knight <robertknight@gmail.com>
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
#include "ShellCommandTest.h"

// Qt
#include <QStringList>

// KDE
#include <qtest.h>

using namespace Konsole;

void ShellCommandTest::init()
{
}

void ShellCommandTest::cleanup()
{
}

void ShellCommandTest::testConstructorWithOneArguemnt()
{
    const QString fullCommand(QStringLiteral("sudo apt-get update"));
    ShellCommand shellCommand(fullCommand);
    QCOMPARE(shellCommand.command(), QStringLiteral("sudo"));
    QCOMPARE(shellCommand.fullCommand(), fullCommand);
}

void ShellCommandTest::testConstructorWithTwoArguments()
{
    const QString command(QStringLiteral("wc"));
    QStringList arguments;
    arguments << QStringLiteral("wc") << QStringLiteral("-l") << QStringLiteral("*.cpp");

    ShellCommand shellCommand(command, arguments);
    QCOMPARE(shellCommand.command(), command);
    QCOMPARE(shellCommand.arguments(), arguments);
    QCOMPARE(shellCommand.fullCommand(), arguments.join(QLatin1String(" ")));
}

void ShellCommandTest::testExpandEnvironmentVariable()
{
    QString text = QStringLiteral("PATH=$PATH:~/bin");
    const QString env = QStringLiteral("PATH");
    const QString value = QStringLiteral("/usr/sbin:/sbin:/usr/local/bin:/usr/bin:/bin");

    qputenv(env.toLocal8Bit().constData(), value.toLocal8Bit());
    const QString result = ShellCommand::expand(text);
    const QString expected = text.replace(QLatin1Char('$') + env, value);
    QCOMPARE(result, expected);

    text = QStringLiteral("PATH=$PATH:\\$ESCAPED:~/bin");
    qputenv(env.toLocal8Bit().constData(), value.toLocal8Bit());
    const QString result2 = ShellCommand::expand(text);
    const QString expected2 = text.replace(QLatin1Char('$') + env, value);
    QCOMPARE(result2, expected2);

    text = QStringLiteral("$ABC \"$ABC\" '$ABC'");
    qputenv("ABC", "123");
    const QString result3 = ShellCommand::expand(text);
    const QString expected3 = QStringLiteral("123 \"123\" '$ABC'");
    QEXPECT_FAIL("", "Bug 361835", Continue);
    QCOMPARE(result3, expected3);
}

void ShellCommandTest::testValidEnvCharacter()
{
    QChar validChar(QLatin1Char('A'));
    const bool result = ShellCommand::isValidEnvCharacter(validChar);
    QCOMPARE(result, true);
}

void ShellCommandTest::testValidLeadingEnvCharacter()
{
    QChar invalidChar(QLatin1Char('9'));
    const bool result = ShellCommand::isValidLeadingEnvCharacter(invalidChar);
    QCOMPARE(result, false);
}

void ShellCommandTest::testArgumentsWithSpaces()
{
    const QString command(QStringLiteral("dir"));
    QStringList arguments;
    arguments << QStringLiteral("dir") << QStringLiteral("c:\\Program Files") << QStringLiteral("System") << QStringLiteral("*.ini");
    const QString expected_arg(QStringLiteral("dir \"c:\\Program Files\" System *.ini"));

    ShellCommand shellCommand(command, arguments);
    QCOMPARE(shellCommand.command(), command);
    QCOMPARE(shellCommand.arguments(), arguments);
    QCOMPARE(shellCommand.fullCommand(), expected_arg);
}

void ShellCommandTest::testEmptyCommand()
{
    const QString command = QString();
    ShellCommand shellCommand(command);
    QCOMPARE(shellCommand.command(), QString());
    QCOMPARE(shellCommand.arguments(), QStringList());
    QCOMPARE(shellCommand.fullCommand(), QString());
}

QTEST_GUILESS_MAIN(ShellCommandTest)

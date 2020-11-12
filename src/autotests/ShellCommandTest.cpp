/*
    SPDX-FileCopyrightText: 2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2013 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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

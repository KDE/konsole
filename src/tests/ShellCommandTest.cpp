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
#include <QtCore/QStringList>
#include <QtGlobal>

// KDE
#include <qtest_kde.h>

using namespace Konsole;

void ShellCommandTest::init()
{
}

void ShellCommandTest::cleanup()
{
}

void ShellCommandTest::testConstructorWithOneArguemnt()
{
    const QString fullCommand("sudo apt-get update");
    ShellCommand shellCommand(fullCommand);
    QCOMPARE(shellCommand.command(), QString("sudo"));
    QCOMPARE(shellCommand.fullCommand(), fullCommand);

}

void ShellCommandTest::testConstructorWithTwoArguments()
{
    const QString command("wc");
    QStringList arguments;
    arguments << "wc" << "-l" << "*.cpp" ;

    ShellCommand shellCommand(command, arguments);
    QCOMPARE(shellCommand.command(), command);
    QCOMPARE(shellCommand.arguments(), arguments);
    QCOMPARE(shellCommand.fullCommand(), arguments.join(" "));
}

void ShellCommandTest::testExpandEnvironmentVariable()
{
    QString text = "PATH=$PATH:~/bin";
    const QString env = "PATH";
    const QString value = "/usr/sbin:/sbin:/usr/local/bin:/usr/bin:/bin";
    qputenv(env.toLocal8Bit(), value.toLocal8Bit());
    QString result = ShellCommand::expand(text);
    QString expected = text.replace('$' + env, value);
    QCOMPARE(result, expected);

    text = "PATH=$PATH:\\$ESCAPED:~/bin";
    qputenv(env.toLocal8Bit(), value.toLocal8Bit());
    result = ShellCommand::expand(text);
    expected = text.replace('$' + env, value);
    QCOMPARE(result, expected);
}

void ShellCommandTest::testValidEnvCharacter()
{
    QChar validChar('A');
    const bool result = ShellCommand::isValidEnvCharacter(validChar);
    QCOMPARE(result, true);
}

void ShellCommandTest::testValidLeadingEnvCharacter()
{
    QChar invalidChar('9');
    const bool result = ShellCommand::isValidLeadingEnvCharacter(invalidChar);
    QCOMPARE(result, false);
}

void ShellCommandTest::testArgumentsWithSpaces()
{
    const QString command("dir");
    QStringList arguments;
    arguments << "dir" << "c:\\Program Files" << "System" << "*.ini" ;
    const QString expected_arg("dir \"c:\\Program Files\" System *.ini");

    ShellCommand shellCommand(command, arguments);
    QCOMPARE(shellCommand.command(), command);
    QCOMPARE(shellCommand.arguments(), arguments);
    QCOMPARE(shellCommand.fullCommand(), expected_arg);
}

void ShellCommandTest::testEmptyCommand()
{
    const QString command("");
    ShellCommand shellCommand(command);
    QCOMPARE(shellCommand.command(), QString());
    QCOMPARE(shellCommand.arguments(), QStringList());
    QCOMPARE(shellCommand.fullCommand(), QString());
}

QTEST_KDEMAIN_CORE(ShellCommandTest)

#include "ShellCommandTest.moc"


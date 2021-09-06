/*
    SPDX-FileCopyrightText: 2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2013 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SHELLCOMMANDTEST_H
#define SHELLCOMMANDTEST_H

#include <QObject>

#include "../ShellCommand.h"

namespace Konsole
{
class ShellCommandTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();

    void testConstructorWithOneArguemnt();
    void testConstructorWithTwoArguments();
    void testExpandEnvironmentVariable();
    void testValidEnvCharacter();
    void testValidLeadingEnvCharacter();
    void testArgumentsWithSpaces();
    void testEmptyCommand();
};

}

#endif // SHELLCOMMANDTEST_H

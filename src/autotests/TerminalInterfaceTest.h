/*
    SPDX-FileCopyrightText: 2014 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TERMINALINTERFACETEST_H
#define TERMINALINTERFACETEST_H

#include <KParts/Part>
#include <kde_terminal_interface.h>

namespace Konsole
{
class TerminalInterfaceTest : public QObject
{
    Q_OBJECT

public:
private Q_SLOTS:
    void initTestCase();
    void testTerminalInterfaceNoShell();
    void testTerminalInterface();
    void testTerminalInterfaceV2();

private:
    KParts::Part *createPart();

    KParts::Part *_terminalPart;
};

}

#endif // TERMINALINTERFACETEST_H

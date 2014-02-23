/*
    Copyright 2014 by Kurt Hindenburg <kurt.hindenburg@gmail.com>

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

#ifndef TERMINALINTERFACETEST_H
#define TERMINALINTERFACETEST_H

#include <kde_terminal_interface.h>
#include <KParts/Part>

namespace Konsole
{

class TerminalInterfaceTest : public QObject
{
    Q_OBJECT

public:

private slots:
    void testTerminalInterface();

private:
    KParts::Part* createPart();
    void sleep(int msecs = 10000);

    KParts::Part* _terminalPart;

};

}

#endif // TERMINALINTERFACETEST_H


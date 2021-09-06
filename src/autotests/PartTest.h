/*
    SPDX-FileCopyrightText: 2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PARTTEST_H
#define PARTTEST_H

#include <KParts/Part>
#include <kde_terminal_interface.h>

namespace Konsole
{
class PartTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void testFdShell();
    void testFdStandalone();

private:
    void testFd(bool runShell);
    KParts::Part *createPart();
};

}

#endif // PARTTEST_H

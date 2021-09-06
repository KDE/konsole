/*
    SPDX-FileCopyrightText: 2013 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SESSIONTEST_H
#define SESSIONTEST_H

#include <kde_terminal_interface.h>

namespace Konsole
{
class SessionTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testNoProfile();
    void testEmulation();

private:
};

}

#endif // SESSIONTEST_H

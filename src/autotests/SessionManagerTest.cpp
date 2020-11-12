/*
    SPDX-FileCopyrightText: 2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "session/SessionManagerTest.h"

// KDE
#include <qtest.h>

using namespace Konsole;

void SessionManagerTest::testWarnNotImplemented()
{
    qWarning() << "SessionManager tests not implemented";
}

void SessionManagerTest::init()
{
}

void SessionManagerTest::cleanup()
{
}

QTEST_MAIN_CORE(SessionManagerTest)

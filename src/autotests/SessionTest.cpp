/*
    SPDX-FileCopyrightText: 2013 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "SessionTest.h"

#include "qtest.h"

// Konsole
#include "../Emulation.h"
#include "../session/Session.h"

using namespace Konsole;

void SessionTest::testNoProfile()
{
    auto session = new Session();

    // No profile loaded, nothing to run
    QCOMPARE(session->isRunning(), false);
    QCOMPARE(session->sessionId(), 1);
    QCOMPARE(session->isRemote(), false);
    QCOMPARE(session->program(), QString());
    QCOMPARE(session->arguments(), QStringList());
    QCOMPARE(session->tabTitleFormat(Session::LocalTabTitle), QString());
    QCOMPARE(session->tabTitleFormat(Session::RemoteTabTitle), QString());

    delete session;
}

void SessionTest::testEmulation()
{
    auto session = new Session();

    Emulation *emulation = session->emulation();

    QCOMPARE(emulation->lineCount(), 40);

    delete session;
}

QTEST_MAIN(SessionTest)

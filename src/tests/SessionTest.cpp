/*
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
#include "SessionTest.h"

#include "qtest_kde.h"

// Konsole
#include "../Session.h"
#include "../Emulation.h"
#include "../History.h"

using namespace Konsole;

void SessionTest::testNoProfile()
{
    Session* session = new Session();

    // No profile loaded, nothing to run
    QCOMPARE(session->isRunning(), false);
    QCOMPARE(session->sessionId(), 1);
    QCOMPARE(session->isRemote(), false);
    QCOMPARE(session->program(), QString(""));
    QCOMPARE(session->arguments(), QStringList());
    QCOMPARE(session->tabTitleFormat(Session::LocalTabTitle), QString(""));
    QCOMPARE(session->tabTitleFormat(Session::RemoteTabTitle), QString(""));

    delete session;
}

void SessionTest::testEmulation()
{
    Session* session = new Session();

    Emulation* emulation = session->emulation();

    QCOMPARE(emulation->lineCount(), 40);

    delete session;
}

QTEST_KDEMAIN(SessionTest , GUI)


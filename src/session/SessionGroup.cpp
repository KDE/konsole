/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>
    SPDX-FileCopyrightText: 2009 Thomas Dreibholz <dreibh@iem.uni-due.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Konsole
// TODO: Own header should be first, but this breaks compiling.
#include "SessionGroup.h"
#include "Emulation.h"
#include "Session.h"

namespace Konsole
{
SessionGroup::SessionGroup(QObject *parent)
    : QObject(parent)
    , _masterMode(0)
{
}

SessionGroup::~SessionGroup() = default;

QList<Session *> SessionGroup::sessions() const
{
    return _sessions.keys();
}

void SessionGroup::addSession(Session *session)
{
    connect(session, &Konsole::Session::finished, this, &Konsole::SessionGroup::sessionFinished);
    _sessions.insert(session, false);
}

void SessionGroup::removeSession(Session *session)
{
    disconnect(session, &Konsole::Session::finished, this, &Konsole::SessionGroup::sessionFinished);
    setMasterStatus(session, false);
    _sessions.remove(session);
}

void SessionGroup::sessionFinished()
{
    auto *session = qobject_cast<Session *>(sender());
    Q_ASSERT(session);
    removeSession(session);
}

void SessionGroup::setMasterMode(int mode)
{
    _masterMode = mode;
}

void SessionGroup::setMasterStatus(Session *session, bool master)
{
    const bool wasMaster = _sessions[session];

    if (wasMaster == master) {
        // No status change -> nothing to do.
        return;
    }
    _sessions[session] = master;

    if (master) {
        connect(session->emulation(), &Konsole::Emulation::sendData, this, &Konsole::SessionGroup::forwardData);
    } else {
        disconnect(session->emulation(), &Konsole::Emulation::sendData, this, &Konsole::SessionGroup::forwardData);
    }
}

void SessionGroup::forwardData(const QByteArray &data)
{
    static bool _inForwardData = false;
    if (_inForwardData) { // Avoid recursive calls among session groups!
        // A recursive call happens when a master in group A calls forwardData()
        // in group B. If one of the destination sessions in group B is also a
        // master of a group including the master session of group A, this would
        // again call forwardData() in group A, and so on.
        return;
    }

    _inForwardData = true;
    const QList<Session *> sessionsKeys = _sessions.keys();
    for (Session *other : sessionsKeys) {
        if (!_sessions[other]) {
            other->emulation()->sendString(data);
        }
    }
    _inForwardData = false;
}

} // namespace konsole

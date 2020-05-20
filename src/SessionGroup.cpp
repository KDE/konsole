/*
    This file is part of Konsole, an X terminal.

    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 1997,1998 by Lars Doelle <lars.doelle@on-line.de>
    Copyright 2009 by Thomas Dreibholz <dreibh@iem.uni-due.de>

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

// Konsole
#include "Session.h"
#include "SessionGroup.h"
#include "Emulation.h"

namespace Konsole {

SessionGroup::SessionGroup(QObject* parent)
    : QObject(parent), _masterMode(0)
{
}

SessionGroup::~SessionGroup() = default;

QList<Session*> SessionGroup::sessions() const
{
    return _sessions.keys();
}

void SessionGroup::addSession(Session* session)
{
    connect(session, &Konsole::Session::finished, this, &Konsole::SessionGroup::sessionFinished);
    _sessions.insert(session, false);
}

void SessionGroup::removeSession(Session* session)
{
    disconnect(session, &Konsole::Session::finished, this, &Konsole::SessionGroup::sessionFinished);
    setMasterStatus(session, false);
    _sessions.remove(session);
}

void SessionGroup::sessionFinished()
{
    auto* session = qobject_cast<Session*>(sender());
    Q_ASSERT(session);
    removeSession(session);
}

void SessionGroup::setMasterMode(int mode)
{
    _masterMode = mode;
}

void SessionGroup::setMasterStatus(Session* session , bool master)
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
        disconnect(session->emulation(), &Konsole::Emulation::sendData,
                   this, &Konsole::SessionGroup::forwardData);
    }
}

void SessionGroup::forwardData(const QByteArray& data)
{
    static bool _inForwardData = false;
    if (_inForwardData) {  // Avoid recursive calls among session groups!
        // A recursive call happens when a master in group A calls forwardData()
        // in group B. If one of the destination sessions in group B is also a
        // master of a group including the master session of group A, this would
        // again call forwardData() in group A, and so on.
        return;
    }

    _inForwardData = true;
    const QList<Session*> sessionsKeys = _sessions.keys();
    for (Session *other : sessionsKeys) {
        if (!_sessions[other]) {
            other->emulation()->sendString(data);
        }
    }
    _inForwardData = false;
}

}       // namespace konsole

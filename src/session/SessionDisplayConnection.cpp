/*
    Copyright 2020-2020 by Gustavo Carneiro <gcarneiroa@hotmail.com>

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
#include "SessionDisplayConnection.h"

// Konsole
#include "Session.h"
#include "terminalDisplay/TerminalDisplay.h"

namespace Konsole
{

SessionDisplayConnection::SessionDisplayConnection(Session *session, TerminalDisplay *view, QObject *parent)
    : QObject(parent)
{
    _session = session;
    _view = view;
}

QPointer<Session> SessionDisplayConnection::session()
{
    return _session;
}

QPointer<TerminalDisplay> SessionDisplayConnection::view()
{
    return _view;
}

bool SessionDisplayConnection::isValid() const
{
    return !_session.isNull() && !_view.isNull();
}

}   // namespace Konsole

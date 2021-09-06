/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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

} // namespace Konsole

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

#ifndef SESSIONDISPLAYCONNECTION_H
#define SESSIONDISPLAYCONNECTION_H

// Qt
#include <QPointer>

// Konsole
#include "konsolesession_export.h"

namespace Konsole
{

class Session;
class TerminalDisplay;

class KONSOLESESSION_EXPORT SessionDisplayConnection : public QObject
{
    Q_OBJECT
public:
    SessionDisplayConnection(Session *session, TerminalDisplay *view, QObject *parent = nullptr);
    ~SessionDisplayConnection() = default;

    QPointer<Session> session();
    QPointer<TerminalDisplay> view();

    /**
     * Returns true if the session and view is valid.
     * A valid connection and view is one which has a non-null session() and view().
     *
     * Equivalent to "!session().isNull() && !view().isNull()"
     */
    bool isValid() const;

private:
    QPointer<Session> _session;
    QPointer<TerminalDisplay> _view;
};

}

#endif

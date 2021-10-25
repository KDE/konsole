/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
    ~SessionDisplayConnection() override = default;

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

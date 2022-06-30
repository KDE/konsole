/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2009 Thomas Dreibholz <dreibh@iem.uni-due.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "SessionTask.h"

namespace Konsole
{
SessionTask::SessionTask(QObject *parent)
    : QObject(parent)
{
}

void SessionTask::setAutoDelete(bool enable)
{
    _autoDelete = enable;
}

bool SessionTask::autoDelete() const
{
    return _autoDelete;
}

void SessionTask::addSession(Session *session)
{
    _sessions.append(session);
}

QList<QPointer<Session>> SessionTask::sessions() const
{
    return _sessions;
}

}

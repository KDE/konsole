/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2009 Thomas Dreibholz <dreibh@iem.uni-due.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SESSIONTASK_H
#define SESSIONTASK_H

#include <QList>
#include <QObject>
#include <QPointer>

#include "Session.h"

namespace Konsole
{
/**
 * Abstract class representing a task which can be performed on a group of sessions.
 *
 * Create a new instance of the appropriate sub-class for the task you want to perform and
 * call the addSession() method to add each session which needs to be processed.
 *
 * Finally, call the execute() method to perform the sub-class specific action on each
 * of the sessions.
 */
class SessionTask : public QObject
{
    Q_OBJECT

public:
    explicit SessionTask(QObject *parent = nullptr);

    /**
     * Sets whether the task automatically deletes itself when the task has been finished.
     * Depending on whether the task operates synchronously or asynchronously, the deletion
     * may be scheduled immediately after execute() returns or it may happen some time later.
     */
    void setAutoDelete(bool enable);
    /** Returns true if the task automatically deletes itself.  See setAutoDelete() */
    bool autoDelete() const;

    /** Adds a new session to the group */
    void addSession(Session *session);

    /**
     * Executes the task on each of the sessions in the group.
     * The completed() signal is emitted when the task is finished, depending on the specific sub-class
     * execute() may be synchronous or asynchronous
     */
    virtual void execute() = 0;

Q_SIGNALS:
    /**
     * Emitted when the task has completed.
     * Depending on the task this may occur just before execute() returns, or it
     * may occur later
     *
     * @param success Indicates whether the task completed successfully or not
     */
    void completed(bool success);

protected:
    /** Returns a list of sessions in the group */
    QList<QPointer<Session>> sessions() const;

private:
    bool _autoDelete;
    QList<QPointer<Session>> _sessions;
};

}
#endif

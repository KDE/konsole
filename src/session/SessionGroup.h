/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>
    SPDX-FileCopyrightText: 2009 Thomas Dreibholz <dreibh@iem.uni-due.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SESSIONGROUP_H
#define SESSIONGROUP_H

// Qt
#include <QHash>
#include <QList>
#include <QObject>

namespace Konsole
{
class Session;

/**
 * Provides a group of sessions which is divided into master and slave sessions.
 * Activity in master sessions can be propagated to all sessions within the group.
 * The type of activity which is propagated and method of propagation is controlled
 * by the masterMode() flags.
 */
class SessionGroup : public QObject
{
    Q_OBJECT

public:
    /** Constructs an empty session group. */
    explicit SessionGroup(QObject *parent);
    /** Destroys the session group and removes all connections between master and slave sessions. */
    ~SessionGroup() override;

    /** Adds a session to the group. */
    void addSession(Session *session);
    /** Removes a session from the group. */
    void removeSession(Session *session);

    /** Returns the list of sessions currently in the group. */
    QList<Session *> sessions() const;

    /**
     * Sets whether a particular session is a master within the group.
     * Changes or activity in the group's master sessions may be propagated
     * to all the sessions in the group, depending on the current masterMode()
     *
     * @param session The session whose master status should be changed.
     * @param master True to make this session a master or false otherwise
     */
    void setMasterStatus(Session *session, bool master);

    /**
     * This enum describes the options for propagating certain activity or
     * changes in the group's master sessions to all sessions in the group.
     */
    enum MasterMode {
        /**
         * Any input key presses in the master sessions are sent to all
         * sessions in the group.
         */
        CopyInputToAll = 1,
    };

    /**
     * Specifies which activity in the group's master sessions is propagated
     * to all sessions in the group.
     *
     * @param mode A bitwise OR of MasterMode flags.
     */
    void setMasterMode(int mode);

private Q_SLOTS:
    void sessionFinished();
    void forwardData(const QByteArray &data);

private:
    // maps sessions to their master status
    QHash<Session *, bool> _sessions;

    int _masterMode;
};
} // namespace Konsole

#endif // SESSIONGROUP_H

/*
    This source file is part of Konsole, a terminal emulator.

    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>

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

#ifndef SESSIONLISTMODEL_H
#define SESSIONLISTMODEL_H

// Qt
#include <QAbstractListModel>
#include <QVariant>

namespace Konsole {
class Session;

/**
 * Item-view model which contains a flat list of sessions.
 * After constructing the model, call setSessions() to set the sessions displayed
 * in the list.  When a session ends (after emitting the finished() signal) it is
 * automatically removed from the list.
 *
 * The internal pointer for each item in the model (index.internalPointer()) is the
 * associated Session*
 */
class SessionListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit SessionListModel(QObject *parent = nullptr);

    /**
     * Sets the list of sessions displayed in the model.
     * To display all sessions that are currently running in the list,
     * call setSessions(SessionManager::instance()->sessions())
     */
    void setSessions(const QList<Session *> &sessions);

    // reimplemented from QAbstractItemModel
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    int columnCount(const QModelIndex &parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &index) const override;

protected:
    virtual void sessionRemoved(Session *)
    {
    }

private Q_SLOTS:
    void sessionFinished();

private:
    QList<Session *> _sessions;
};
}

#endif //SESSIONLISTMODEL_H

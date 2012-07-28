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

// Own
#include "SessionListModel.h"

// KDE
#include <KIcon>
#include <KLocalizedString>

// Konsole
#include "Session.h"

using Konsole::Session;
using Konsole::SessionListModel;

SessionListModel::SessionListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void SessionListModel::setSessions(const QList<Session*>& sessions)
{
    _sessions = sessions;

    foreach(Session * session, sessions) {
        connect(session, SIGNAL(finished()), this, SLOT(sessionFinished()));
    }

    reset();
}

QVariant SessionListModel::data(const QModelIndex& index, int role) const
{
    Q_ASSERT(index.isValid());

    int row = index.row();
    int column = index.column();

    Q_ASSERT(row >= 0 && row < _sessions.count());
    Q_ASSERT(column >= 0 && column < 2);

    switch (role) {
    case Qt::DisplayRole:
        if (column == 1) {
            // This code is duplicated from SessionController.cpp
            QString title = _sessions[row]->title(Session::DisplayedTitleRole);

            // special handling for the "%w" marker which is replaced with the
            // window title set by the shell
            title.replace("%w", _sessions[row]->userTitle());
            // special handling for the "%#" marker which is replaced with the
            // number of the shell
            title.replace("%#", QString::number(_sessions[row]->sessionId()));
            return title;
        } else if (column == 0) {
            return _sessions[row]->sessionId();
        }
        break;
    case Qt::DecorationRole:
        if (column == 1)
            return KIcon(_sessions[row]->iconName());
        else
            return QVariant();
    }

    return QVariant();
}

QVariant SessionListModel::headerData(int section, Qt::Orientation orientation,
                                      int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Vertical)
        return QVariant();
    else {
        switch (section) {
        case 0:
            return i18nc("@item:intable The session index", "Number");
        case 1:
            return i18nc("@item:intable The session title", "Title");
        default:
            return QVariant();
        }
    }
}

int SessionListModel::columnCount(const QModelIndex&) const
{
    return 2;
}

int SessionListModel::rowCount(const QModelIndex&) const
{
    return _sessions.count();
}

QModelIndex SessionListModel::parent(const QModelIndex&) const
{
    return QModelIndex();
}

void SessionListModel::sessionFinished()
{
    Session* session = qobject_cast<Session*>(sender());
    int row = _sessions.indexOf(session);

    if (row != -1) {
        beginRemoveRows(QModelIndex(), row, row);
        sessionRemoved(session);
        _sessions.removeAt(row);
        endRemoveRows();
    }
}

QModelIndex SessionListModel::index(int row, int column, const QModelIndex& parent) const
{
    if (hasIndex(row, column, parent))
        return createIndex(row, column, _sessions[row]);
    else
        return QModelIndex();
}

#include "SessionListModel.moc"

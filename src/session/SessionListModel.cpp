/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "SessionListModel.h"

// KDE
#include <KLocalizedString>

#include <QIcon>

// Konsole
#include "Session.h"

using Konsole::Session;
using Konsole::SessionListModel;

SessionListModel::SessionListModel(QObject *parent)
    : QAbstractListModel(parent)
    , _sessions(QList<Session *>())
{
}

void SessionListModel::setSessions(const QList<Session *> &sessions)
{
    beginResetModel();
    _sessions = sessions;

    for (Session *session : sessions) {
        connect(session, &Konsole::Session::finished, this, &Konsole::SessionListModel::sessionFinished);
    }

    endResetModel();
}

QVariant SessionListModel::data(const QModelIndex &index, int role) const
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
            title.replace(QLatin1String("%w"), _sessions[row]->userTitle());
            // special handling for the "%#" marker which is replaced with the
            // number of the shell
            title.replace(QLatin1String("%#"), QString::number(_sessions[row]->sessionId()));
            return title;
        } else if (column == 0) {
            return _sessions[row]->sessionId();
        }
        break; // Due to the above 'column' constraints, this is never reached.
    case Qt::DecorationRole:
        if (column == 1) {
            return QIcon::fromTheme(_sessions[row]->iconName());
        } else {
            return QVariant();
        }
    }

    return QVariant();
}

QVariant SessionListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation == Qt::Vertical) {
        return QVariant();
    }
    switch (section) {
    case 0:
        return i18nc("@item:intable The session index", "Number");
    case 1:
        return i18nc("@item:intable The session title", "Title");
    default:
        return QVariant();
    }
}

int SessionListModel::columnCount(const QModelIndex &) const
{
    return 2;
}

int SessionListModel::rowCount(const QModelIndex &) const
{
    return _sessions.count();
}

QModelIndex SessionListModel::parent(const QModelIndex &) const
{
    return {};
}

void SessionListModel::sessionFinished()
{
    auto *session = qobject_cast<Session *>(sender());
    int row = _sessions.indexOf(session);

    if (row != -1) {
        beginRemoveRows(QModelIndex(), row, row);
        sessionRemoved(session);
        _sessions.removeAt(row);
        endRemoveRows();
    }
}

QModelIndex SessionListModel::index(int row, int column, const QModelIndex &parent) const
{
    if (hasIndex(row, column, parent)) {
        return createIndex(row, column, _sessions[row]);
    }
    return {};
}

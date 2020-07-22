/*
    Copyright 2008 by Robert Knight <robertknight@gmail.com>

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

#include "CheckableSessionModel.h"

using namespace Konsole;

CheckableSessionModel::CheckableSessionModel(QObject *parent) :
    SessionListModel(parent),
    _checkedSessions(QSet<Session *>()),
    _fixedSessions(QSet<Session *>()),
    _checkColumn(0)
{
}

void CheckableSessionModel::setCheckColumn(int column)
{
    beginResetModel();
    _checkColumn = column;
    endResetModel();
}

Qt::ItemFlags CheckableSessionModel::flags(const QModelIndex &index) const
{
    auto *session = static_cast<Session *>(index.internalPointer());

    if (_fixedSessions.contains(session)) {
        return SessionListModel::flags(index) & ~Qt::ItemIsEnabled;
    }
   return SessionListModel::flags(index) | Qt::ItemIsUserCheckable;
}

QVariant CheckableSessionModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::CheckStateRole && index.column() == _checkColumn) {
        auto *session = static_cast<Session *>(index.internalPointer());
        return QVariant::fromValue(static_cast<int>(
            _checkedSessions.contains(session) ? Qt::Checked : Qt::Unchecked)
        );
    }
    return SessionListModel::data(index, role);
}

bool CheckableSessionModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role == Qt::CheckStateRole && index.column() == _checkColumn) {
        auto *session = static_cast<Session *>(index.internalPointer());

        if (_fixedSessions.contains(session)) {
            return false;
        }

        if (value.toInt() == Qt::Checked) {
            _checkedSessions.insert(session);
        } else {
            _checkedSessions.remove(session);
        }

        emit dataChanged(index, index);
        return true;
    }
    return SessionListModel::setData(index, value, role);
}

void CheckableSessionModel::setCheckedSessions(const QSet<Session *> &sessions)
{
    beginResetModel();
    _checkedSessions = sessions;
    endResetModel();
}

QSet<Session *> CheckableSessionModel::checkedSessions() const
{
    return _checkedSessions;
}

void CheckableSessionModel::setCheckable(Session *session, bool checkable)
{
    beginResetModel();

    if (!checkable) {
        _fixedSessions.insert(session);
    } else {
        _fixedSessions.remove(session);
    }

    endResetModel();
}

void CheckableSessionModel::sessionRemoved(Session *session)
{
    _checkedSessions.remove(session);
    _fixedSessions.remove(session);
}

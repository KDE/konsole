/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>

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

#ifndef PROFILE_MODEL_H
#define PROFILE_MODEL_H

#include "konsoleprofile_export.h"

#include <QAbstractTableModel>
#include <QExplicitlySharedDataPointer>
#include <QList>

namespace Konsole {
class Profile;

class KONSOLEPROFILE_EXPORT ProfileModel : public QAbstractTableModel {
    Q_OBJECT
public:
    static ProfileModel* instance();

    enum Roles { ProfilePtrRole = Qt::UserRole + 1 };
    enum Column { NAME, SHORTCUT, PROFILE, COLUMNS };
    void populate();
    void add(QExplicitlySharedDataPointer<Profile> profile);
    void remove(QExplicitlySharedDataPointer<Profile> profile);
    void update(QExplicitlySharedDataPointer<Profile> profile);
    void setDefault(QExplicitlySharedDataPointer<Profile> profile);

    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& idx, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
private:
    QList<QExplicitlySharedDataPointer<Profile>> m_profiles;
    ProfileModel();
};

}

#endif

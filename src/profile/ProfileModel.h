/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PROFILE_MODEL_H
#define PROFILE_MODEL_H

#include "konsoleprofile_export.h"

#include <QAbstractTableModel>
#include <QExplicitlySharedDataPointer>
#include <QList>

namespace Konsole
{
class Profile;

class KONSOLEPROFILE_EXPORT ProfileModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    static ProfileModel *instance();

    enum Roles {
        ProfilePtrRole = Qt::UserRole + 1,
    };
    enum Column {
        NAME,
        SHORTCUT,
        PROFILE,
        COLUMNS,
    };
    void populate();
    void add(QExplicitlySharedDataPointer<Profile> profile);
    void remove(QExplicitlySharedDataPointer<Profile> profile);
    void update(QExplicitlySharedDataPointer<Profile> profile);
    void setDefault(QExplicitlySharedDataPointer<Profile> profile);

    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &idx, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

private:
    QList<QExplicitlySharedDataPointer<Profile>> m_profiles;
    ProfileModel();
};

}

#endif

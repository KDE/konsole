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

#include "ProfileModel.h"
#include "ProfileManager.h"
#include "Profile.h"

#include <KLocalizedString>

#include <QIcon>
#include <QDebug>

using namespace Konsole;

ProfileModel* ProfileModel::instance()
{
    static ProfileModel self;
    return &self;
}

ProfileModel::ProfileModel()
{
    connect(ProfileManager::instance(), &ProfileManager::profileAdded,
            this, &ProfileModel::add);
    connect(ProfileManager::instance(), &ProfileManager::profileRemoved,
            this, &ProfileModel::remove);
    connect(ProfileManager::instance(), &ProfileManager::profileChanged,
            this, &ProfileModel::update);
    populate();
}
int ProfileModel::rowCount(const QModelIndex &unused) const
{
    Q_UNUSED(unused)

    // All profiles  plus the default profile that's always at index 0 on
    return m_profiles.count();
}

int ProfileModel::columnCount(const QModelIndex& unused) const
{
    Q_UNUSED(unused)
    return COLUMNS;
}

QVariant ProfileModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Vertical) {
        return {};
    }
    if (role != Qt::DisplayRole) {
        return {};
    }

    switch(section) {
        case NAME: return i18nc("@title:column Profile name", "Name");
        case SHORTCUT: return i18nc("@title:column Profile keyboard shortcut", "Shortcut");
    }
    return {};
}

QVariant ProfileModel::data(const QModelIndex& idx, int role) const
{
    if (!idx.isValid()) {
        return {};
    }

    if (idx.row() > m_profiles.count()) {
        return {};
    }

    auto profile = m_profiles.at(idx.row());

    switch (idx.column()) {
    case NAME: {
        switch (role) {
        case Qt::DisplayRole: {
            return QStringLiteral("%1%2").arg(
                profile->name(),
                ProfileManager::instance()->defaultProfile() == profile ? i18n(" (Default)") : QString());
        }
        case Qt::DecorationRole: return QIcon::fromTheme(profile->icon());
        case Qt::FontRole: {
                if (ProfileManager::instance()->defaultProfile() == profile ) {
                    QFont font;
                    font.setItalic(true);
                    return font;
                }
            }
        }
    }
    break;
    case SHORTCUT: {
        auto shortcut = ProfileManager::instance()->shortcut(profile);
        switch (role) {
            case Qt::DisplayRole: return shortcut;
            case Qt::EditRole: return shortcut;
            case Qt::ToolTipRole: return i18nc("@info:tooltip", "Double click to change shortcut");
        }
        break;
    }
    case PROFILE: {
        switch(role) {
            case ProfilePtrRole: return QVariant::fromValue(profile);
        }
        break;
    }
    }

    return {};
}

Qt::ItemFlags ProfileModel::flags(const QModelIndex& idx) const
{
    auto currentFlags = QAbstractTableModel::flags(idx);

    switch(idx.column()) {
        case NAME: return currentFlags & (~Qt::ItemIsEditable);
        case SHORTCUT: return currentFlags | Qt::ItemIsEditable;
        default: ;
    }
    return currentFlags;
}

bool ProfileModel::setData(const QModelIndex &idx, const QVariant &value, int role)
{
    if (!idx.isValid()) {
        return false;
    }

    if (idx.column() != SHORTCUT) {
        return false;
    }

    if (role != Qt::EditRole && role != Qt::DisplayRole) {
        return false;
    }

    auto profile = m_profiles.at(idx.row());
    if (idx.column() == SHORTCUT) {
        auto sequence = QKeySequence::fromString(value.toString());
        ProfileManager::instance()->setShortcut(profile, sequence);
        emit dataChanged(idx, idx, {Qt::DisplayRole});
        return true;
    }

    return false;
}

void ProfileModel::populate()
{
    beginResetModel();
    m_profiles = ProfileManager::instance()->allProfiles();
    ProfileManager::instance()->sortProfiles(m_profiles);
    endResetModel();
}

void ProfileModel::add(QExplicitlySharedDataPointer<Profile> profile)
{
    // The model is too small for this to matter.
    Q_UNUSED(profile)
    populate();
}

void ProfileModel::remove(QExplicitlySharedDataPointer<Profile> profile)
{
    // The model is too small for this to matter.
    Q_UNUSED(profile)
    populate();
}

void ProfileModel::setDefault(QExplicitlySharedDataPointer<Profile> profile)
{
    Q_UNUSED(profile)
    emit dataChanged(index(0, 0), index(0, COLUMNS-1), {Qt::DisplayRole});
}

void ProfileModel::update(QExplicitlySharedDataPointer<Profile> profile)
{
    int row = m_profiles.indexOf(profile);
    dataChanged(index(row, 0), index(row, COLUMNS-1));
}

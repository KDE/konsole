#include "ProfileModel.h"
#include "ProfileManager.h"
#include "Profile.h"

#include <KLocalizedString>

#include <QIcon>

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
    Q_UNUSED(unused);
    return m_profiles.count();
}

int ProfileModel::columnCount(const QModelIndex& unused) const
{
    Q_UNUSED(unused);
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

    QExplicitlySharedDataPointer<Profile> profile = m_profiles.at(idx.row());

    switch (idx.column()) {
    case NAME: {
        switch (role) {
        case Qt::DisplayRole: return QStringLiteral("%1%2").arg(profile->name(), (idx.row() == 0 ? i18n("(Default)") : QString()));
        case Qt::DecorationRole: return profile->icon();
        case Qt::FontRole: {
                // Default Profile
                if (idx.row() == 0) {
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
        default: return currentFlags;
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
    m_profiles.prepend(ProfileManager::instance()->defaultProfile());
    endResetModel();
}

void ProfileModel::add(QExplicitlySharedDataPointer<Konsole::Profile> profile)
{
    // The model is too small for this to matter.
    Q_UNUSED(profile);
    populate();
}

void ProfileModel::remove(QExplicitlySharedDataPointer<Konsole::Profile> profile)
{
    // The model is too small for this to matter.
    Q_UNUSED(profile);
    populate();
}

void ProfileModel::update(QExplicitlySharedDataPointer<Konsole::Profile> profile)
{
    int row = m_profiles.indexOf(profile);
    dataChanged(index(row, 0), index(row, COLUMNS-1));
}

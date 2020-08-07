#include "ProfileModel.h"
#include "ProfileManager.h"
#include "Profile.h"

#include <KLocalizedString>

#include <QIcon>

using namespace Konsole;

ProfileModel::ProfileModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    connect(ProfileManager::instance(), &ProfileManager::profileAdded,
            this, &ProfileModel::add);
    connect(ProfileManager::instance(), &ProfileManager::profileRemoved,
            this, &ProfileModel::remove);
    connect(ProfileManager::instance(), &ProfileManager::profileChanged,
            this, &ProfileModel::update);
    connect(ProfileManager::instance(), &ProfileManager::favoriteStatusChanged,
            this, &ProfileModel::update);

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
    const auto isEnabled = ProfileManager::instance()->findFavorites().contains(profile);

    switch (idx.column()) {
    case VISIBILITY: {
        switch(role) {
        case Qt::ToolTipRole:
            return i18nc("@info:tooltip List item's checkbox for making item (profile) visible in a menu",
            "Show profile in menu");
        case Qt::DecorationRole:
            return QIcon::fromTheme(QStringLiteral("visibility"));
        case Qt::CheckStateRole:
            return isEnabled ? Qt::Checked : Qt::Unchecked;
        }
    }
    break;
    break;
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
        switch (role) {
            case Qt::DisplayRole: return ProfileManager::instance()->shortcut(profile).toString();
            case Qt::ToolTipRole: return isEnabled
                ? i18nc("@info:tooltip", "Double click to change shortcut")
                : i18nc("@info:tooltip", "Shortcut won't work while the profile is not marked as visible.");
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
        case VISIBILITY : return currentFlags | Qt::ItemIsUserCheckable;
        case NAME: return currentFlags & (~Qt::ItemIsEditable);
        case SHORTCUT: {
            QExplicitlySharedDataPointer<Profile> profile = m_profiles.at(idx.row());
            const auto isEnabled = ProfileManager::instance()->findFavorites().contains(profile);
            if (!isEnabled) {
                return currentFlags & (~Qt::ItemIsEnabled);
            }
        } break;
        default: return currentFlags;
    }
    return currentFlags;
}

bool ProfileModel::setData(const QModelIndex &idx, const QVariant &value, int role)
{
    if (!idx.isValid()) {
        return false;
    }

    if (idx.row() != VISIBILITY || idx.row() != SHORTCUT) {
        return false;
    }

    if (role != Qt::EditRole) {
        return false;
    }

    auto profile = m_profiles.at(idx.row());
    if (idx.row() == VISIBILITY) {
        profile->setHidden(value.toBool());
        emit dataChanged(idx, idx, {Qt::CheckStateRole});
    } else if (idx.row() == SHORTCUT) {
        QKeySequence sequence = QKeySequence::fromString(value.toString());
        ProfileManager::instance()->setShortcut(profile, sequence);
        emit dataChanged(idx, idx, {Qt::DisplayRole});
    }

    return true;
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

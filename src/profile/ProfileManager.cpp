/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "ProfileManager.h"
#include "PopStackOnExit.h"

#include "konsoledebug.h"

// Qt
#include <QDir>
#include <QFileInfo>
#include <QString>

// KDE
#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>

// Konsole
#include "ProfileGroup.h"
#include "ProfileModel.h"
#include "ProfileReader.h"
#include "ProfileWriter.h"

using namespace Konsole;

static bool stringLessThan(const QString &p1, const QString &p2)
{
    return QString::localeAwareCompare(p1, p2) < 0;
}

static bool profileNameLessThan(const Profile::Ptr &p1, const Profile::Ptr &p2)
{
    // Always put the Default/fallback profile at the top
    if (p1->isFallback()) {
        return true;
    } else if (p2->isFallback()) {
        return false;
    }

    return stringLessThan(p1->name(), p2->name());
}

ProfileManager::ProfileManager()
    : m_config(KSharedConfig::openConfig())
{
    // load fallback profile
    initFallbackProfile();
    _defaultProfile = _fallbackProfile;

    // lookup the default profile specified in <App>rc
    // For stand-alone Konsole, m_config is just "konsolerc"
    // For konsolepart, m_config might be "yakuakerc", "dolphinrc", "katerc"...
    KConfigGroup group = m_config->group("Desktop Entry");
    QString defaultProfileFileName = group.readEntry("DefaultProfile", "");

    // if the hosting application of konsolepart does not specify its own
    // default profile, use the default profile of stand-alone Konsole.
    if (defaultProfileFileName.isEmpty()) {
        KSharedConfigPtr konsoleConfig = KSharedConfig::openConfig(QStringLiteral("konsolerc"));
        group = konsoleConfig->group("Desktop Entry");
        defaultProfileFileName = group.readEntry("DefaultProfile", "");
    }

    loadAllProfiles(defaultProfileFileName);
    loadShortcuts();

    Q_ASSERT(_profiles.size() > 0);
    Q_ASSERT(_defaultProfile);
}

ProfileManager::~ProfileManager() = default;

Q_GLOBAL_STATIC(ProfileManager, theProfileManager)
ProfileManager *ProfileManager::instance()
{
    return theProfileManager;
}

ProfileManager::Iterator ProfileManager::findProfile(const Profile::Ptr &profile) const
{
    return std::find(_profiles.cbegin(), _profiles.cend(), profile);
}

void ProfileManager::initFallbackProfile()
{
    _fallbackProfile = Profile::Ptr(new Profile());
    _fallbackProfile->useFallback();
    addProfile(_fallbackProfile);
}

Profile::Ptr ProfileManager::loadProfile(const QString &shortPath)
{
    // the fallback profile has a 'special' path name, "FALLBACK/"
    if (shortPath == _fallbackProfile->path()) {
        return _fallbackProfile;
    }

    QString path = shortPath;

    // add a suggested suffix and relative prefix if missing
    QFileInfo fileInfo(path);

    if (fileInfo.isDir()) {
        return Profile::Ptr();
    }

    if (fileInfo.suffix() != QLatin1String("profile")) {
        path.append(QLatin1String(".profile"));
    }
    if (fileInfo.path().isEmpty() || fileInfo.path() == QLatin1String(".")) {
        path.prepend(QLatin1String("konsole") + QDir::separator());
    }

    // if the file is not an absolute path, look it up
    if (!fileInfo.isAbsolute()) {
        path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, path);
    }

    // if the file is not found, return immediately
    if (path.isEmpty()) {
        return Profile::Ptr();
    }

    // check that we have not already loaded this profile
    for (const Profile::Ptr &profile : _profiles) {
        if (profile->path() == path) {
            return profile;
        }
    }

    // guard to prevent problems if a profile specifies itself as its parent
    // or if there is recursion in the "inheritance" chain
    // (eg. two profiles, A and B, specifying each other as their parents)
    static QStack<QString> recursionGuard;
    PopStackOnExit<QString> popGuardOnExit(recursionGuard);

    if (recursionGuard.contains(path)) {
        qCDebug(KonsoleDebug) << "Ignoring attempt to load profile recursively from" << path;
        return _fallbackProfile;
    }
    recursionGuard.push(path);

    // load the profile
    ProfileReader reader;

    Profile::Ptr newProfile = Profile::Ptr(new Profile(fallbackProfile()));
    newProfile->setProperty(Profile::Path, path);

    QString parentProfilePath;
    bool result = reader.readProfile(path, newProfile, parentProfilePath);

    if (!parentProfilePath.isEmpty()) {
        Profile::Ptr parentProfile = loadProfile(parentProfilePath);
        newProfile->setParent(parentProfile);
    }

    if (!result) {
        qCDebug(KonsoleDebug) << "Could not load profile from " << path;
        return Profile::Ptr();
    } else if (newProfile->name().isEmpty()) {
        qCWarning(KonsoleDebug) << path << " does not have a valid name, ignoring.";
        return Profile::Ptr();
    } else {
        addProfile(newProfile);
        return newProfile;
    }
}
QStringList ProfileManager::availableProfilePaths() const
{
    ProfileReader reader;

    QStringList paths;
    paths += reader.findProfiles();

    std::stable_sort(paths.begin(), paths.end(), stringLessThan);

    return paths;
}

QStringList ProfileManager::availableProfileNames() const
{
    QStringList names;

    const QList<Profile::Ptr> allProfiles = ProfileManager::instance()->allProfiles();
    for (const Profile::Ptr &profile : allProfiles) {
        if (!profile->isHidden()) {
            names.push_back(profile->name());
        }
    }

    std::stable_sort(names.begin(), names.end(), stringLessThan);

    return names;
}

void ProfileManager::loadAllProfiles(const QString &defaultProfileFileName)
{
    const QStringList &paths = availableProfilePaths();
    for (const QString &path : paths) {
        Profile::Ptr profile = loadProfile(path);
        if (profile && !defaultProfileFileName.isEmpty() && path.endsWith(QLatin1Char('/') + defaultProfileFileName)) {
            _defaultProfile = profile;
        }
    }
}

void ProfileManager::saveSettings()
{
    saveShortcuts();
}

void ProfileManager::sortProfiles()
{
    std::sort(_profiles.begin(), _profiles.end(), profileNameLessThan);
}

QList<Profile::Ptr> ProfileManager::allProfiles()
{
    sortProfiles();
    return loadedProfiles();
}

QList<Profile::Ptr> ProfileManager::loadedProfiles() const
{
    return {_profiles.cbegin(), _profiles.cend()};
}

Profile::Ptr ProfileManager::defaultProfile() const
{
    return _defaultProfile;
}
Profile::Ptr ProfileManager::fallbackProfile() const
{
    return _fallbackProfile;
}

QString ProfileManager::generateUniqueName() const
{
    const QStringList existingProfileNames = availableProfileNames();
    int nameSuffix = 1;
    QString uniqueProfileName;
    do {
        uniqueProfileName = QStringLiteral("Profile ") + QString::number(nameSuffix);
        ++nameSuffix;
    } while (existingProfileNames.contains(uniqueProfileName));

    return uniqueProfileName;
}

QString ProfileManager::saveProfile(const Profile::Ptr &profile)
{
    ProfileWriter writer;

    QString newPath = writer.getPath(profile);

    if (!writer.writeProfile(newPath, profile)) {
        KMessageBox::sorry(nullptr, i18n("Konsole does not have permission to save this profile to %1", newPath));
    }

    return newPath;
}

void ProfileManager::changeProfile(Profile::Ptr profile, QHash<Profile::Property, QVariant> propertyMap, bool persistent)
{
    Q_ASSERT(profile);

    const QString origPath = profile->path();
    const QKeySequence origShortcut = shortcut(profile);
    const bool isDefaultProfile = profile == defaultProfile();

    const QString uniqueProfileName = generateUniqueName();

    // Don't save a profile with an empty name on disk
    persistent = persistent && !profile->name().isEmpty();

    bool messageShown = false;
    bool isNameChanged = false;
    // Insert the changes into the existing Profile instance
    for (auto it = propertyMap.cbegin(); it != propertyMap.cend(); ++it) {
        const auto property = it.key();
        auto value = it.value();

        isNameChanged = property == Profile::Name || property == Profile::UntranslatedName;

        // "Default" is reserved for the fallback profile, override it;
        // The message is only shown if the user manually typed "Default"
        // in the name box in the edit profile dialog; i.e. saving the
        // fallback profile where the user didn't change the name at all,
        // the uniqueProfileName is used silently a couple of lines above.
        if (isNameChanged && value == QLatin1String("Default")) {
            value = uniqueProfileName;
            if (!messageShown) {
                KMessageBox::sorry(nullptr,
                                   i18n("The name \"Default\" is reserved for the built-in"
                                        " fallback profile;\nthe profile is going to be"
                                        " saved as \"%1\"",
                                        uniqueProfileName));
                messageShown = true;
            }
        }

        profile->setProperty(property, value);
    }

    // when changing a group, iterate through the profiles
    // in the group and call changeProfile() on each of them
    //
    // this is so that each profile in the group, the profile is
    // applied, a change notification is emitted and the profile
    // is saved to disk
    ProfileGroup::Ptr group = profile->asGroup();
    if (group) {
        const QList<Profile::Ptr> profiles = group->profiles();
        for (const Profile::Ptr &groupProfile : profiles) {
            changeProfile(groupProfile, propertyMap, persistent);
        }
        return;
    }

    // save changes to disk, unless the profile is hidden, in which case
    // it has no file on disk
    if (persistent && !profile->isHidden()) {
        profile->setProperty(Profile::Path, saveProfile(profile));
    }

    if (isNameChanged) { // Renamed?
        // origPath is empty when saving a new profile
        if (!origPath.isEmpty()) {
            // Delete the old/redundant .profile from disk
            QFile::remove(origPath);

            // Change the default profile name to the new one
            if (isDefaultProfile) {
                setDefaultProfile(profile);
            }

            // If the profile had a shortcut, re-assign it to the profile
            if (!origShortcut.isEmpty()) {
                setShortcut(profile, origShortcut);
            }
        }

        sortProfiles();
    }

    // Notify the world about the change
    Q_EMIT profileChanged(profile);
}

void ProfileManager::addProfile(const Profile::Ptr &profile)
{
    if (_profiles.empty()) {
        _defaultProfile = profile;
    }

    if (findProfile(profile) == _profiles.cend()) {
        _profiles.push_back(profile);
        Q_EMIT profileAdded(profile);
    }
}

bool ProfileManager::deleteProfile(Profile::Ptr profile)
{
    bool wasDefault = (profile == defaultProfile());

    if (profile) {
        // try to delete the config file
        if (profile->isPropertySet(Profile::Path) && QFile::exists(profile->path())) {
            if (!QFile::remove(profile->path())) {
                qCDebug(KonsoleDebug) << "Could not delete profile: " << profile->path() << "The file is most likely in a directory which is read-only.";

                return false;
            }
        }

        setShortcut(profile, QKeySequence());
        if (auto it = findProfile(profile); it != _profiles.end()) {
            _profiles.erase(it);
        }

        // mark the profile as hidden so that it does not show up in the
        // Manage Profiles dialog and is not saved to disk
        profile->setHidden(true);
    }

    // If we just deleted the default profile, replace it with the first
    // profile in the list.
    if (wasDefault) {
        const QList<Profile::Ptr> existingProfiles = allProfiles();
        setDefaultProfile(existingProfiles.at(0));
    }

    Q_EMIT profileRemoved(profile);

    return true;
}

void ProfileManager::setDefaultProfile(const Profile::Ptr &profile)
{
    Q_ASSERT(findProfile(profile) != _profiles.cend());

    const auto oldDefault = _defaultProfile;
    _defaultProfile = profile;
    ProfileModel::instance()->setDefault(profile);

    saveDefaultProfile();

    // Setting/unsetting a profile as the default is a sort of a
    // "profile change", useful for updating the icon/font of the
    // "default profile in e.g. 'File -> New Tab' menu.
    Q_EMIT profileChanged(oldDefault);
    Q_EMIT profileChanged(profile);
}

void ProfileManager::saveDefaultProfile()
{
    QString path = _defaultProfile->path();
    ProfileWriter writer;

    if (path.isEmpty()) {
        path = writer.getPath(_defaultProfile);
    }

    KConfigGroup group = m_config->group("Desktop Entry");
    group.writeEntry("DefaultProfile", QUrl::fromLocalFile(path).fileName());
    m_config->sync();
}

void ProfileManager::loadShortcuts()
{
    KConfigGroup shortcutGroup = m_config->group("Profile Shortcuts");

    const QLatin1String suffix(".profile");
    auto findByName = [this, suffix](const QString &name) {
        return std::find_if(_profiles.cbegin(), _profiles.cend(), [&name, suffix](const Profile::Ptr &p) {
            return p->name() == name //
                || (p->name() + suffix) == name; // For backwards compatibility
        });
    };

    const QMap<QString, QString> entries = shortcutGroup.entryMap();
    for (auto it = entries.cbegin(), endIt = entries.cend(); it != endIt; ++it) {
        auto profileIt = findByName(it.value());
        if (profileIt == _profiles.cend()) {
            continue;
        }

        _shortcuts.push_back({*profileIt, QKeySequence::fromString(it.key())});
    }
}

void ProfileManager::saveShortcuts()
{
    if (_profileShortcutsChanged) {
        _profileShortcutsChanged = false;

        KConfigGroup shortcutGroup = m_config->group("Profile Shortcuts");
        shortcutGroup.deleteGroup();

        for (const auto &[profile, keySeq] : _shortcuts) {
            shortcutGroup.writeEntry(keySeq.toString(), profile->name());
        }

        m_config->sync();
    }
}

void ProfileManager::setShortcut(Profile::Ptr profile, const QKeySequence &keySequence)
{
    _profileShortcutsChanged = true;
    QKeySequence existingShortcut = shortcut(profile);

    auto profileIt = std::find_if(_shortcuts.begin(), _shortcuts.end(), [&profile](const ShortcutData &data) {
        return data.profileKey == profile;
    });
    if (profileIt != _shortcuts.end()) {
        // There is a previous shortcut for this profile, replace it with the new one
        profileIt->keySeq = keySequence;
        Q_EMIT shortcutChanged(profileIt->profileKey, profileIt->keySeq);
    } else {
        // No previous shortcut for this profile
        const ShortcutData &newData = _shortcuts.emplace_back(ShortcutData{profile, keySequence});
        Q_EMIT shortcutChanged(newData.profileKey, newData.keySeq);
    }

    auto keySeqIt = std::find_if(_shortcuts.begin(), _shortcuts.end(), [&keySequence, &profile](const ShortcutData &data) {
        return data.profileKey != profile && data.keySeq == keySequence;
    });
    if (keySeqIt != _shortcuts.end()) {
        // There is a profile with shortcut "keySequence" which has been
        // associated with another profile >>> unset it
        Q_EMIT shortcutChanged(keySeqIt->profileKey, {});
        _shortcuts.erase(keySeqIt);
    }
}

QKeySequence ProfileManager::shortcut(Profile::Ptr profile) const
{
    auto it = std::find_if(_shortcuts.cbegin(), _shortcuts.cend(), [&profile](const ShortcutData &data) {
        return data.profileKey == profile;
    });

    return it != _shortcuts.cend() ? it->keySeq : QKeySequence{};
}

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
#include "ProfileManager.h"

// Qt
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QList>
#include <QtCore/QString>

// KDE
#include <KConfig>
#include <KGlobal>
#include <KDebug>
#include <KConfigGroup>
#include <KStandardDirs>

// Konsole
#include "ProfileReader.h"
#include "ProfileWriter.h"

using namespace Konsole;

static bool profileIndexLessThan(const Profile::Ptr& p1, const Profile::Ptr& p2)
{
    return p1->menuIndexAsInt() <= p2->menuIndexAsInt();
}

static bool profileNameLessThan(const Profile::Ptr& p1, const Profile::Ptr& p2)
{
    return QString::localeAwareCompare(p1->name(), p2->name()) <= 0;
}

static bool stringLessThan(const QString& p1, const QString& p2)
{
    return QString::localeAwareCompare(p1, p2) <= 0;
}

static void sortByIndexProfileList(QList<Profile::Ptr>& list)
{
    qStableSort(list.begin(), list.end(), profileIndexLessThan);
}

static void sortByNameProfileList(QList<Profile::Ptr>& list)
{
    qStableSort(list.begin(), list.end(), profileNameLessThan);
}

ProfileManager::ProfileManager()
    : _loadedAllProfiles(false)
    , _loadedFavorites(false)
{
    //load fallback profile
    _fallbackProfile = Profile::Ptr(new FallbackProfile);
    addProfile(_fallbackProfile);

    // lookup the default profile specified in <App>rc
    // for stand-alone Konsole, appConfig is just konsolerc
    // for konsolepart, appConfig might be yakuakerc, dolphinrc, katerc...
    KSharedConfigPtr appConfig = KGlobal::config();
    KConfigGroup group = appConfig->group("Desktop Entry");
    QString defaultProfileFileName = group.readEntry("DefaultProfile", "");

    // if the hosting application of konsolepart does not specify its own
    // default profile, use the default profile of stand-alone Konsole.
    if (defaultProfileFileName.isEmpty()) {
        KSharedConfigPtr konsoleConfig = KSharedConfig::openConfig("konsolerc");
        group = konsoleConfig->group("Desktop Entry");
        defaultProfileFileName = group.readEntry("DefaultProfile", "Shell.profile");
    }

    // load the default profile
    const QString path = KStandardDirs::locate("data", "konsole/" + defaultProfileFileName);
    if (!path.isEmpty()) {
        Profile::Ptr profile = loadProfile(path);
        if (profile)
            _defaultProfile = profile;
    }

    Q_ASSERT(_profiles.count() > 0);
    Q_ASSERT(_defaultProfile);

    // get shortcuts and paths of profiles associated with
    // them - this doesn't load the shortcuts themselves,
    // that is done on-demand.
    loadShortcuts();
}

ProfileManager::~ProfileManager()
{
}

K_GLOBAL_STATIC(ProfileManager , theProfileManager)
ProfileManager* ProfileManager::instance()
{
    return theProfileManager;
}

Profile::Ptr ProfileManager::loadProfile(const QString& shortPath)
{
    // the fallback profile has a 'special' path name, "FALLBACK/"
    if (shortPath == _fallbackProfile->path())
        return _fallbackProfile;

    QString path = shortPath;

    // add a suggested suffix and relative prefix if missing
    QFileInfo fileInfo(path);

    if (fileInfo.isDir())
        return Profile::Ptr();

    if (fileInfo.suffix() != "profile")
        path.append(".profile");
    if (fileInfo.path().isEmpty() || fileInfo.path() == ".")
        path.prepend(QString("konsole") + QDir::separator());

    // if the file is not an absolute path, look it up
    if (!fileInfo.isAbsolute())
        path = KStandardDirs::locate("data", path);

    // if the file is not found, return immediately
    if (path.isEmpty()) {
        return Profile::Ptr();
    }

    // check that we have not already loaded this profile
    foreach(const Profile::Ptr& profile, _profiles) {
        if (profile->path() == path)
            return profile;
    }

    // guard to prevent problems if a profile specifies itself as its parent
    // or if there is recursion in the "inheritance" chain
    // (eg. two profiles, A and B, specifying each other as their parents)
    static QStack<QString> recursionGuard;
    PopStackOnExit<QString> popGuardOnExit(recursionGuard);

    if (recursionGuard.contains(path)) {
        kWarning() << "Ignoring attempt to load profile recursively from" << path;
        return _fallbackProfile;
    } else {
        recursionGuard.push(path);
    }

    // load the profile
    ProfileReader* reader = new KDE4ProfileReader;

    Profile::Ptr newProfile = Profile::Ptr(new Profile(fallbackProfile()));
    newProfile->setProperty(Profile::Path, path);

    QString parentProfilePath;
    bool result = reader->readProfile(path, newProfile, parentProfilePath);

    if (!parentProfilePath.isEmpty()) {
        Profile::Ptr parentProfile = loadProfile(parentProfilePath);
        newProfile->setParent(parentProfile);
    }

    delete reader;

    if (!result) {
        kWarning() << "Could not load profile from " << path;
        return Profile::Ptr();
    } else {
        addProfile(newProfile);
        return newProfile;
    }
}
QStringList ProfileManager::availableProfilePaths() const
{
    KDE4ProfileReader kde4Reader;

    QStringList paths;
    paths += kde4Reader.findProfiles();

    qStableSort(paths.begin(), paths.end(), stringLessThan);

    return paths;
}

QStringList ProfileManager::availableProfileNames() const
{
    QStringList names;

    foreach(Profile::Ptr profile, ProfileManager::instance()->allProfiles()) {
        if (!profile->isHidden()) {
            names.push_back(profile->name());
        }
    }

    qStableSort(names.begin(), names.end(), stringLessThan);

    return names;
}

void ProfileManager::loadAllProfiles()
{
    if (_loadedAllProfiles)
        return;

    const QStringList& paths = availableProfilePaths();
    foreach(const QString& path, paths) {
        loadProfile(path);
    }

    _loadedAllProfiles = true;
}

void ProfileManager::sortProfiles(QList<Profile::Ptr>& list)
{
    QList<Profile::Ptr> lackingIndices;
    QList<Profile::Ptr> havingIndices;

    for (int i = 0; i < list.size(); ++i) {
        // dis-regard the fallback profile
        if (list.at(i)->path() == _fallbackProfile->path())
            continue;

        if (list.at(i)->menuIndexAsInt() == 0)
            lackingIndices.append(list.at(i));
        else
            havingIndices.append(list.at(i));
    }

    // sort by index
    sortByIndexProfileList(havingIndices);

    // sort alphabetically those w/o an index
    sortByNameProfileList(lackingIndices);

    // Put those with indices in sequential order w/o any gaps
    int i = 0;
    for (i = 0; i < havingIndices.size(); ++i) {
        Profile::Ptr tempProfile = havingIndices.at(i);
        tempProfile->setProperty(Profile::MenuIndex, QString::number(i + 1));
        havingIndices.replace(i, tempProfile);
    }
    // Put those w/o indices in sequential order
    for (int j = 0; j < lackingIndices.size(); ++j) {
        Profile::Ptr tempProfile = lackingIndices.at(j);
        tempProfile->setProperty(Profile::MenuIndex, QString::number(j + 1 + i));
        lackingIndices.replace(j, tempProfile);
    }

    // combine the 2 list: first those who had indices
    list.clear();
    list.append(havingIndices);
    list.append(lackingIndices);
}

void ProfileManager::saveSettings()
{
    // save default profile
    saveDefaultProfile();

    // save shortcuts
    saveShortcuts();

    // save favorites
    saveFavorites();

    // ensure default/favorites/shortcuts settings are synced into disk
    KSharedConfigPtr appConfig = KGlobal::config();
    appConfig->sync();
}

QList<Profile::Ptr> ProfileManager::sortedFavorites()
{
    QList<Profile::Ptr> favorites = findFavorites().toList();

    sortProfiles(favorites);
    return favorites;
}

QList<Profile::Ptr> ProfileManager::allProfiles()
{
    loadAllProfiles();

    return _profiles.toList();
}

QList<Profile::Ptr> ProfileManager::loadedProfiles() const
{
    return _profiles.toList();
}

Profile::Ptr ProfileManager::defaultProfile() const
{
    return _defaultProfile;
}
Profile::Ptr ProfileManager::fallbackProfile() const
{
    return _fallbackProfile;
}

QString ProfileManager::saveProfile(Profile::Ptr profile)
{
    ProfileWriter* writer = new KDE4ProfileWriter;

    QString newPath = writer->getPath(profile);

    writer->writeProfile(newPath, profile);

    delete writer;

    return newPath;
}

void ProfileManager::changeProfile(Profile::Ptr profile,
                                   QHash<Profile::Property, QVariant> propertyMap, bool persistent)
{
    Q_ASSERT(profile);

    // insert the changes into the existing Profile instance
    QListIterator<Profile::Property> iter(propertyMap.keys());
    while (iter.hasNext()) {
        const Profile::Property property = iter.next();
        profile->setProperty(property, propertyMap[property]);
    }

    // never save a profile with empty name into disk!
    persistent = persistent && !profile->name().isEmpty();

    // when changing a group, iterate through the profiles
    // in the group and call changeProfile() on each of them
    //
    // this is so that each profile in the group, the profile is
    // applied, a change notification is emitted and the profile
    // is saved to disk
    ProfileGroup::Ptr group = profile->asGroup();
    if (group) {
        foreach(const Profile::Ptr & profile, group->profiles()) {
            changeProfile(profile, propertyMap, persistent);
        }
        return;
    }

    // notify the world about the change
    emit profileChanged(profile);

    // save changes to disk, unless the profile is hidden, in which case
    // it has no file on disk
    if (persistent && !profile->isHidden()) {
        profile->setProperty(Profile::Path, saveProfile(profile));
    }
}

void ProfileManager::addProfile(Profile::Ptr profile)
{
    if (_profiles.isEmpty())
        _defaultProfile = profile;

    _profiles.insert(profile);

    emit profileAdded(profile);
}

bool ProfileManager::deleteProfile(Profile::Ptr profile)
{
    bool wasDefault = (profile == defaultProfile());

    if (profile) {
        // try to delete the config file
        if (profile->isPropertySet(Profile::Path) && QFile::exists(profile->path())) {
            if (!QFile::remove(profile->path())) {
                kWarning() << "Could not delete profile: " << profile->path()
                           << "The file is most likely in a directory which is read-only.";

                return false;
            }
        }

        // remove from favorites, profile list, shortcut list etc.
        setFavorite(profile, false);
        setShortcut(profile, QKeySequence());
        _profiles.remove(profile);

        // mark the profile as hidden so that it does not show up in the
        // Manage Profiles dialog and is not saved to disk
        profile->setHidden(true);
    }

    // if we just deleted the default profile,
    // replace it with a random profile from the list
    if (wasDefault) {
        setDefaultProfile(_profiles.toList().first());
    }

    emit profileRemoved(profile);

    return true;
}

void ProfileManager::setDefaultProfile(Profile::Ptr profile)
{
    Q_ASSERT(_profiles.contains(profile));

    _defaultProfile = profile;
}

void ProfileManager::saveDefaultProfile()
{
    QString path = _defaultProfile->path();

    if (path.isEmpty())
        path = KDE4ProfileWriter().getPath(_defaultProfile);

    QFileInfo fileInfo(path);

    KSharedConfigPtr appConfig = KGlobal::config();
    KConfigGroup group = appConfig->group("Desktop Entry");
    group.writeEntry("DefaultProfile", fileInfo.fileName());
}

QSet<Profile::Ptr> ProfileManager::findFavorites()
{
    loadFavorites();

    return _favorites;
}
void ProfileManager::setFavorite(Profile::Ptr profile , bool favorite)
{
    if (!_profiles.contains(profile))
        addProfile(profile);

    if (favorite && !_favorites.contains(profile)) {
        _favorites.insert(profile);
        emit favoriteStatusChanged(profile, favorite);
    } else if (!favorite && _favorites.contains(profile)) {
        _favorites.remove(profile);
        emit favoriteStatusChanged(profile, favorite);
    }
}
void ProfileManager::loadShortcuts()
{
    KSharedConfigPtr appConfig = KGlobal::config();
    KConfigGroup shortcutGroup = appConfig->group("Profile Shortcuts");

    QMap<QString, QString> entries = shortcutGroup.entryMap();

    QMapIterator<QString, QString> iter(entries);
    while (iter.hasNext()) {
        iter.next();

        QKeySequence shortcut = QKeySequence::fromString(iter.key());
        QString profilePath = iter.value();

        ShortcutData data;

        // if the file is not an absolute path, look it up
        QFileInfo fileInfo(profilePath);
        if (!fileInfo.isAbsolute()) {
            profilePath = KStandardDirs::locate("data", "konsole/" + profilePath);
        }

        data.profilePath = profilePath;
        _shortcuts.insert(shortcut, data);
    }
}
void ProfileManager::saveShortcuts()
{
    KSharedConfigPtr appConfig = KGlobal::config();
    KConfigGroup shortcutGroup = appConfig->group("Profile Shortcuts");
    shortcutGroup.deleteGroup();

    QMapIterator<QKeySequence, ShortcutData> iter(_shortcuts);
    while (iter.hasNext()) {
        iter.next();

        QString shortcutString = iter.key().toString();

        // if the profile path in "Profile Shortcuts" is an absolute path,
        // take the profile name
        QFileInfo fileInfo(iter.value().profilePath);
        QString profileName;
        if (fileInfo.isAbsolute()) {
            // Check to see if file is under KDE's data locations.  If not,
            // store full path.
            QString location = KGlobal::dirs()->locate("data",
                               "konsole/" + fileInfo.fileName());
            if (location.isEmpty()) {
                profileName = iter.value().profilePath;
            } else  {
                profileName = fileInfo.fileName();
            }
        } else {
            profileName = iter.value().profilePath;
        }

        shortcutGroup.writeEntry(shortcutString, profileName);
    }
}
void ProfileManager::setShortcut(Profile::Ptr profile ,
                                 const QKeySequence& keySequence)
{
    QKeySequence existingShortcut = shortcut(profile);
    _shortcuts.remove(existingShortcut);

    if (keySequence.isEmpty())
        return;

    ShortcutData data;
    data.profileKey = profile;
    data.profilePath = profile->path();
    // TODO - This won't work if the profile doesn't
    // have a path yet
    _shortcuts.insert(keySequence, data);

    emit shortcutChanged(profile, keySequence);
}
void ProfileManager::loadFavorites()
{
    if (_loadedFavorites)
        return;

    KSharedConfigPtr appConfig = KGlobal::config();
    KConfigGroup favoriteGroup = appConfig->group("Favorite Profiles");

    QSet<QString> favoriteSet;

    if (favoriteGroup.hasKey("Favorites")) {
        QStringList list = favoriteGroup.readEntry("Favorites", QStringList());
        favoriteSet = QSet<QString>::fromList(list);
    } else {
        // if there is no favorites key at all, mark the
        // supplied 'Shell.profile' as the only favorite
        favoriteSet << "Shell.profile";
    }

    // look for favorites among those already loaded
    foreach(const Profile::Ptr& profile, _profiles) {
        const QString& path = profile->path();
        if (favoriteSet.contains(path)) {
            _favorites.insert(profile);
            favoriteSet.remove(path);
        }
    }
    // load any remaining favorites
    foreach(const QString& favorite, favoriteSet) {
        Profile::Ptr profile = loadProfile(favorite);
        if (profile)
            _favorites.insert(profile);
    }

    _loadedFavorites = true;
}
void ProfileManager::saveFavorites()
{
    KSharedConfigPtr appConfig = KGlobal::config();
    KConfigGroup favoriteGroup = appConfig->group("Favorite Profiles");

    QStringList paths;
    foreach(const Profile::Ptr& profile, _favorites) {
        Q_ASSERT(_profiles.contains(profile) && profile);

        QFileInfo fileInfo(profile->path());
        QString profileName;

        if (fileInfo.isAbsolute()) {
            // Check to see if file is under KDE's data locations.  If not,
            // store full path.
            QString location = KGlobal::dirs()->locate("data",
                               "konsole/" + fileInfo.fileName());
            if (location.isEmpty()) {
                profileName = profile->path();
            } else {
                profileName = fileInfo.fileName();
            }
        } else {
            profileName = profile->path();
        }

        paths << profileName;
    }

    favoriteGroup.writeEntry("Favorites", paths);
}

QList<QKeySequence> ProfileManager::shortcuts()
{
    return _shortcuts.keys();
}

Profile::Ptr ProfileManager::findByShortcut(const QKeySequence& shortcut)
{
    Q_ASSERT(_shortcuts.contains(shortcut));

    if (!_shortcuts[shortcut].profileKey) {
        Profile::Ptr key = loadProfile(_shortcuts[shortcut].profilePath);
        if (!key) {
            _shortcuts.remove(shortcut);
            return Profile::Ptr();
        }
        _shortcuts[shortcut].profileKey = key;
    }

    return _shortcuts[shortcut].profileKey;
}


QKeySequence ProfileManager::shortcut(Profile::Ptr profile) const
{
    QMapIterator<QKeySequence, ShortcutData> iter(_shortcuts);
    while (iter.hasNext()) {
        iter.next();
        if (iter.value().profileKey == profile
                || iter.value().profilePath == profile->path())
            return iter.key();
    }

    return QKeySequence();
}


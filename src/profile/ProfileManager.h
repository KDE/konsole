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

#ifndef PROFILEMANAGER_H
#define PROFILEMANAGER_H

// Qt
#include <QKeySequence>
#include <QHash>
#include <QList>
#include <QSet>
#include <QStringList>
#include <QVariant>
#include <QStack>

// Konsole
#include "Profile.h"

namespace Konsole {
/**
 * Manages profiles which specify various settings for terminal sessions
 * and their displays.
 */
class KONSOLEPROFILE_EXPORT ProfileManager : public QObject
{
    Q_OBJECT

public:
    /**
     * Constructs a new profile manager and loads information about the available
     * profiles.
     */
    ProfileManager();

    /**
     * Destroys the ProfileManager.
     */
    ~ProfileManager() override;

    /**
     * Returns the profile manager instance.
     */
    static ProfileManager *instance();

    /**
     * Returns a list of all available profiles
     *
     * Initially only the profile currently set as the default is loaded.
    *
     * When this method is called, it calls loadAllProfiles() internally to
     * ensure all available profiles are loaded and usable.
     */
    QList<Profile::Ptr> allProfiles();

    /**
     * Returns a list of already loaded profiles
     */
    QList<Profile::Ptr> loadedProfiles() const;

    /**
     * Loads all available profiles.  This involves reading each
     * profile configuration file from disk and parsing it.
     * Therefore it should only be done when necessary.
     */
    void loadAllProfiles();

    /**
     * Loads a profile from the specified path and registers
     * it with the ProfileManager.
     *
     * @p path may be relative or absolute.  The path may just be the
     * base name of the profile to load (eg. if the profile's full path
     * is "<konsole data dir>/My Profile.profile" then both
     * "konsole/My Profile.profile" , "My Profile.profile" and
     * "My Profile" will be accepted)
     *
     * @return Pointer to a profile which can be passed to
     * SessionManager::createSession() to create a new session using
     * this profile.
     */
    Profile::Ptr loadProfile(const QString &shortPath);

    /**
     * This creates a profile based on the fallback profile, it's shown
     * as "Default". This is a special profile as it's not saved on disk
     * but rather created from code in Profile class, based on the default
     * profile settings. When the user tries to save this profile, a name
     * (other than "Default") is set for it, e.g. "Profile 1" unless the
     * user has specified a name; then we init a fallback profile again as
     * a separate instance.
     */
    void initFallbackProfile();

    /**
     * Searches for available profiles on-disk and returns a list
     * of paths of profiles which can be loaded.
     */
    QStringList availableProfilePaths() const;

    /**
     * Returns a list of names of all available profiles
     */
    QStringList availableProfileNames() const;

    /**
     * Registers a new type of session.
     */
    void addProfile(const Profile::Ptr &profile);

    /**
     * Deletes the configuration file used to store a profile.
     * The profile will continue to exist while sessions are still using it.  The profile
     * will be marked as hidden (see Profile::setHidden() ) so that it does not show
     * up in profile lists and future changes to the profile are not stored to disk.
     *
     * Returns true if the profile was successfully deleted or false otherwise.
     */
    bool deleteProfile(Profile::Ptr profile);

    /**
     * Updates a @p profile with the changes specified in @p propertyMap.
     *
     * All sessions currently using the profile will be updated to reflect the new settings.
     *
     * After the profile is updated, the profileChanged() signal will be emitted.
     *
     * @param profile The profile to change
     * @param propertyMap A map between profile properties and values describing the changes
     * @param persistent If true, the changes are saved to the profile's configuration file,
     * set this to false if you want to preview possible changes to a profile but do not
     * wish to make them permanent.
     */
    void changeProfile(Profile::Ptr profile, QHash<Profile::Property, QVariant> propertyMap,
                       bool persistent = true);

    /**
     * Sets the @p profile as the default profile for creating new sessions
     */
    void setDefaultProfile(const Profile::Ptr &profile);

    /**
     * Returns a Profile object describing the default profile
     */
    Profile::Ptr defaultProfile() const;

    /**
     * Returns a Profile object with hard-coded settings which is always available.
     * This can be used as a parent for new profiles which provides suitable default settings
     * for all properties.
     */
    Profile::Ptr fallbackProfile() const;

    /**
     * Sorts the profile list by menuindex; those without an menuindex, sort by name.
     *  The menuindex list is first and then the non-menuindex list.
     *
     * @param list The profile list to sort
     */
    void sortProfiles(QList<Profile::Ptr> &list);

    /**
     * Associates a shortcut with a particular profile.
     */
    void setShortcut(Profile::Ptr profile, const QKeySequence &keySequence);

    /** Returns the shortcut associated with a particular profile. */
    QKeySequence shortcut(Profile::Ptr profile) const;

Q_SIGNALS:

    /** Emitted when a profile is added to the manager. */
    void profileAdded(const Profile::Ptr &ptr);
    /** Emitted when a profile is removed from the manager. */
    void profileRemoved(const Profile::Ptr &ptr);
    /** Emitted when a profile's properties are modified. */
    void profileChanged(const Profile::Ptr &ptr);

    /**
     * Emitted when the shortcut for a profile is changed.
     *
     * @param profile The profile whose status was changed
     * @param newShortcut The new shortcut key sequence for the profile
     */
    void shortcutChanged(const Profile::Ptr &profile, const QKeySequence &newShortcut);

public Q_SLOTS:
    /** Saves settings (shortcuts, default profile etc.) to disk. */
    void saveSettings();

protected Q_SLOTS:

private Q_SLOTS:

private:
    Q_DISABLE_COPY(ProfileManager)

    // loads the mappings between shortcut key sequences and
    // profile paths
    void loadShortcuts();
    // saves the mappings between shortcut key sequences and
    // profile paths
    void saveShortcuts();

    // records which profile is set as the default profile
    // Note: it does not save the profile itself into disk. That is
    // what saveProfile() does.
    void saveDefaultProfile();

    // saves a profile to a file
    // returns the path to which the profile was saved, which will
    // be the same as the path property of profile if valid or a newly generated path
    // otherwise
    QString saveProfile(const Profile::Ptr &profile);

    QSet<Profile::Ptr> _profiles;  // list of all loaded profiles

    Profile::Ptr _defaultProfile;
    Profile::Ptr _fallbackProfile;

    bool _loadedAllProfiles; // set to true after loadAllProfiles has been called

    struct ShortcutData {
        Profile::Ptr profileKey;
        QString profilePath;
    };
    QMap<QKeySequence, ShortcutData> _shortcuts; // shortcut keys -> profile path

    // finds out if it's a internal profile or an external one,
    // fixing the path to point to the correct location for the profile.
    QString normalizePath(const QString& path) const;
};

}

#endif //PROFILEMANAGER_H

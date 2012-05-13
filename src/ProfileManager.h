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
#include <QtGui/QKeySequence>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtCore/QStack>

// Konsole
#include "Profile.h"

namespace Konsole
{
/**
 * Manages profiles which specify various settings for terminal sessions
 * and their displays.
 *
 * Profiles in the manager have a concept of favorite status, which can be used
 * by widgets and dialogs in the application decide which profiles to list and
 * how to display them.  The favorite status of a profile can be altered using
 * setFavorite() and retrieved using isFavorite()
 */
class KONSOLEPRIVATE_EXPORT ProfileManager : public QObject
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
    virtual ~ProfileManager();

    /**
     * Returns the profile manager instance.
     */
    static ProfileManager* instance();

    /**
     * Returns a list of all available profiles
     *
     * Initially only the profile currently set as the default is loaded.
     *
     * Favorite profiles are loaded automatically when findFavorites() is called.
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
    Profile::Ptr loadProfile(const QString& path);

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
     * The favorite status of the session ( as returned by isFavorite() ) is set to false by default.
     */
    void addProfile(Profile::Ptr type);

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
    void changeProfile(Profile::Ptr profile , QHash<Profile::Property, QVariant> propertyMap,
                       bool persistent = true);

    /**
     * Sets the @p profile as the default profile for creating new sessions
     */
    void setDefaultProfile(Profile::Ptr profile);

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
     * Specifies whether a profile should be included in the user's
     * list of favorite profiles.
     */
    void setFavorite(Profile::Ptr profile , bool favorite);

    /**
     * Returns the set of the user's favorite profiles.
     */
    QSet<Profile::Ptr> findFavorites();

    QList<Profile::Ptr> sortedFavorites();

    /*
     * Sorts the profile list by menuindex; those without an menuindex, sort by name.
     *  The menuindex list is first and then the non-menuindex list.
     *
     * @param list The profile list to sort
     */
    void sortProfiles(QList<Profile::Ptr>& list);

    /**
     * Associates a shortcut with a particular profile.
     */
    void setShortcut(Profile::Ptr profile , const QKeySequence& shortcut);

    /** Returns the shortcut associated with a particular profile. */
    QKeySequence shortcut(Profile::Ptr profile) const;

    /**
     * Returns the list of shortcut key sequences which
     * can be used to create new sessions based on
     * existing profiles
     *
     * When one of the shortcuts is activated,
     * use findByShortcut() to load the profile associated
     * with the shortcut.
     */
    QList<QKeySequence> shortcuts();

    /**
     * Finds and loads the profile associated with
     * the specified @p shortcut key sequence and returns a pointer to it.
     */
    Profile::Ptr findByShortcut(const QKeySequence& shortcut);

signals:

    /** Emitted when a profile is added to the manager. */
    void profileAdded(Profile::Ptr ptr);
    /** Emitted when a profile is removed from the manager. */
    void profileRemoved(Profile::Ptr ptr);
    /** Emitted when a profile's properties are modified. */
    void profileChanged(Profile::Ptr ptr);

    /**
     * Emitted when the favorite status of a profile changes.
     *
     * @param profile The profile to change
     * @param favorite Specifies whether the profile is a favorite or not
     */
    void favoriteStatusChanged(Profile::Ptr profile , bool favorite);

    /**
     * Emitted when the shortcut for a profile is changed.
     *
     * @param profile The profile whose status was changed
     * @param newShortcut The new shortcut key sequence for the profile
     */
    void shortcutChanged(Profile::Ptr profile , const QKeySequence& newShortcut);

public slots:
    /** Saves settings (favorites, shortcuts, default profile etc.) to disk. */
    void saveSettings();

protected slots:

private slots:

private:
    // loads the mappings between shortcut key sequences and
    // profile paths
    void loadShortcuts();
    // saves the mappings between shortcut key sequences and
    // profile paths
    void saveShortcuts();

    //loads the set of favorite profiles
    void loadFavorites();
    //saves the set of favorite profiles
    void saveFavorites();

    // records which profile is set as the default profile
    // Note: it does not save the profile itself into disk. That is
    // what saveProfile() does.
    void saveDefaultProfile();

    // saves a profile to a file
    // returns the path to which the profile was saved, which will
    // be the same as the path property of profile if valid or a newly generated path
    // otherwise
    QString saveProfile(Profile::Ptr profile);

    QSet<Profile::Ptr> _profiles;  // list of all loaded profiles
    QSet<Profile::Ptr> _favorites; // list of favorite profiles

    Profile::Ptr _defaultProfile;
    Profile::Ptr _fallbackProfile;

    bool _loadedAllProfiles; // set to true after loadAllProfiles has been called
    bool _loadedFavorites; // set to true after loadFavorites has been called

    struct ShortcutData {
        Profile::Ptr profileKey;
        QString profilePath;
    };
    QMap<QKeySequence, ShortcutData> _shortcuts; // shortcut keys -> profile path
};

/**
 * PopStackOnExit is a utility to remove all values from a QStack which are added during
 * the lifetime of a PopStackOnExit instance.
 *
 * When a PopStackOnExit instance is destroyed, elements are removed from the stack
 * until the stack count is reduced the value when the PopStackOnExit instance was created.
 */
template <class T>
class PopStackOnExit
{
public:
    explicit PopStackOnExit(QStack<T>& stack) : _stack(stack) , _count(stack.count()) {}
    ~PopStackOnExit() {
        while (_stack.count() > _count)
            _stack.pop();
    }
private:
    QStack<T>& _stack;
    int _count;
};
}
#endif //PROFILEMANAGER_H

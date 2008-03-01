/*
    This source file is part of Konsole, a terminal emulator.

    Copyright (C) 2006-7 by Robert Knight <robertknight@gmail.com>

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

#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

// Qt
#include <QtGui/QFont>
#include <QtGui/QKeySequence>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <QtCore/QPair>
#include <QtCore/QPointer>
#include <QtCore/QVariant>

// Konsole
#include "Profile.h"

class QSignalMapper;


namespace Konsole
{

class Session;

/**
 * Manages running terminal sessions and the profiles which specify various
 * settings for terminal sessions and their displays.
 *
 * Profiles in the manager have a concept of favorite status, which can be used
 * by widgets and dialogs in the application decide which sessions to list and
 * how to display them.  The favorite status of a profile can be altered using
 * setFavorite() and retrieved using isFavorite() 
 */
class SessionManager : public QObject
{
Q_OBJECT

public:
    /** 
     * Constructs a new session manager and loads information about the available 
     * profiles.
     */
    SessionManager();
    virtual ~SessionManager();

    /**
     * Returns a list of keys for profiles which have been loaded.
     * Initially only the profile currently set as the default is loaded.
     *
     * Favorite profiles are loaded automatically when findFavorites() is called.
     *
     * All other profiles can be loaded by calling loadAllProfiles().  This may
     * involves opening, reading and parsing all profiles from disk, and
     * should only be done when necessary.
     */
    QList<QString> loadedProfiles() const;

    /**
     * Searches for available profiles on-disk and returns a list
     * of paths of profiles which can be loaded.
     */
    QList<QString> availableProfilePaths() const;

    /**
     * Returns the session information object for the profile with the specified
     * key or 0 if no profile with the specified key exists.
     *
     * If @p key is empty, a pointer to the default profile is returned.
     */
    Profile* profile(const QString& key) const;

    /**
     * Registers a new type of session and returns the key
     * which can be passed to createSession() to create new instances of the session.
     *
     * The favorite status of the session ( as returned by isFavorite() ) is set
     * to false by default.
     */
    QString addProfile(Profile* type);

    /**
     * Loads a profile from the specified path and registers 
     * it with the SessionManager.
     *
     * @p path may be relative or absolute.  The path may just be the 
     * base name of the profile to load (eg. if the profile's full path
     * is "<konsole data dir>/My Profile.profile" then both
     * "konsole/My Profile.profile" , "My Profile.profile" and 
     * "My Profile" will be accepted)
     *
     * @return The profile key which can be used to create new sessions
     * from this profile using createSession()
     */
    QString loadProfile(const QString& path);

    /**
     * Updates the profile associated with the specified @p key with
     * the changes specified in @p propertyMap.
     *
     * All sessions using the profile will be updated to reflect the new settings.
     *
     * After the profile is updated, the profileChanged() signal will be emitted.
     *
     * @param key The profile's key
     * @param propertyMap A map between profile properties and values describing the changes
     * @param persistant If true, the changes are saved to the profile's configuration file,
     * set this to false if you want to preview possible changes to a profile but do not
     * wish to make them permanent.
     */
    void changeProfile(const QString& key , QHash<Profile::Property,QVariant> propertyMap, 
            bool persistant = true);

    /**
     * Returns a Profile object describing the default type of session, which is used
     * if createSession() is called with an empty configPath argument.
     */
    Profile* defaultProfile() const;

    /**
     * Returns the key for the default profile.
     */
    QString defaultProfileKey() const;

    /**
     * Creates a new session using the settings specified by the specified
     * profile key.
     *
     * The new session has no views associated with it.  A new TerminalDisplay view
     * must be created in order to display the output from the terminal session and
     * send keyboard or mouse input to it.
     *
     * @param type Specifies the type of session to create.  Passing an empty
     *             string will create a session using the default configuration.
     */
    Session* createSession(const QString& key = QString());

    /**
     * Updates a session's properties to match its current profile.
     */
    void updateSession(Session* session);

    /**
     * Returns a list of active sessions.
     */
    const QList<Session*> sessions();

    /**
     * Deletes the profile with the specified key.
     * The configuration file associated with the profile is
     * deleted if possible.
     *
     * Returns true if the profile was successfully deleted or false otherwise.
     */
    bool deleteProfile(const QString& key);

    /**
     * Sets the profile with the specified key
     * as the default type.
     */
    void setDefaultProfile(const QString& key);

    /**
     * Returns the set of keys for the user's favorite profiles.
     */
    QSet<QString> findFavorites();

    /**
     * Returns the list of shortcut key sequences which
     * can be used to create new sessions based on
     * existing profiles
     *
     * When one of the shortcuts is activated, 
     * use findByShortcut() to load the profile associated
     * with the shortcut and obtain its key.
     */
    QList<QKeySequence> shortcuts();

    /**
     * Finds and loads the profile associated with 
     * the specified @p shortcut key sequence and returns
     * its string key.
     */
    QString findByShortcut(const QKeySequence& shortcut);

    /**
     * Associates a shortcut with a particular profile.
     */
    void setShortcut(const QString& key , const QKeySequence& shortcut);

    /** Returns the shortcut associated with a particular profile. */
    QKeySequence shortcut(const QString& profileKey) const;

    /**
     * Specifies whether a profile should be included in the user's
     * list of favorite sessions.
     */
    void setFavorite(const QString& key , bool favorite);

    /** 
     * Loads all available profiles.  This involves reading each
     * profile configuration file from disk and parsing it.  
     * Therefore it should only be done when necessary.
     */
    void loadAllProfiles();

    /**
     * Sets the global session manager instance.
     */
    static void setInstance(SessionManager* instance);
    /**
     * Returns the session manager instance.
     */
    static SessionManager* instance();

signals:
    /** Emitted when a profile is added to the manager. */
    void profileAdded(const QString& key);
    /** Emitted when a profile is removed from the manager. */
    void profileRemoved(const QString& key);
    /** Emitted when a profile's properties are modified. */
    void profileChanged(const QString& key);

    /** 
     * Emitted when a session's settings are updated to match 
     * its current profile. 
     */
    void sessionUpdated(Session* session);

    /** 
     * Emitted when the favorite status of a profile changes. 
     * 
     * @param key The key for the profile
     * @param favorite Specifies whether the session is a favorite or not 
     */
    void favoriteStatusChanged(const QString& key , bool favorite);

    /** 
     * Emitted when the shortcut for a profile is changed. 
     *
     * @param key The key for the profile 
     * @param newShortcut The new shortcut key sequence for the profile
     */
    void shortcutChanged(const QString& key , const QKeySequence& newShortcut);

protected Q_SLOTS:

    /**
     * Called to inform the manager that a session has finished executing.
     *
     * @param session The Session which has finished executing.
     */
    void sessionTerminated( QObject* session );

private slots:
    void sessionProfileChanged();
    void sessionProfileCommandReceived(const QString& text);

private:
    // loads the mappings between shortcut key sequences and 
    // profile paths
    void loadShortcuts();
    // saves the mappings between shortcut key sequences and
    // profile paths
    void saveShortcuts();

    //loads the set of favorite sessions
    void loadFavorites();
    //saves the set of favorite sessions
    void saveFavorites();
    // saves a profile to a file
    // returns the path to which the profile was saved, which will
    // be the same as the path property of profile if valid or a newly generated path
    // otherwise
    QString saveProfile(Profile* profile);

    // applies updates to the profile associated with @p key
    // to all sessions currently using that profile
    // if modifiedPropertiesOnly is true, only properties which
    // are set in the profile @p key are updated
    void applyProfile(const QString& key , bool modifiedPropertiesOnly);
    // apples updates to the profile @p info to the session @p session
    // if modifiedPropertiesOnly is true, only properties which
    // are set in @p info are update ( ie. properties for which info->isPropertySet(<property>) 
    // returns true )
    void applyProfile(Session* session , const Profile* info , bool modifiedPropertiesOnly); 

    QHash<QString,Profile*> _types;             // profile key -> Profile instance
    
    struct ShortcutData
    {
        QString profileKey;
        QString profilePath;
    };
    QMap<QKeySequence,ShortcutData> _shortcuts; // shortcut keys -> profile path

    QList<Session*> _sessions; // list of running sessions

    QString _defaultProfile; // profile key of default profile
    
    QSet<QString> _favorites; // list of profile keys for favorite profiles

    bool _loadedAllProfiles; // set to true after loadAllProfiles has been called

    QSignalMapper* _sessionMapper;
};

class ShouldApplyProperty 
{
public:
	ShouldApplyProperty(const Profile* profile , bool modifiedOnly) : 
	_profile(profile) , _modifiedPropertiesOnly(modifiedOnly) {}

	bool shouldApply(Profile::Property property) const
	{
		return !_modifiedPropertiesOnly || _profile->isPropertySet(property); 
	}
private:
	const Profile* _profile;
	bool _modifiedPropertiesOnly;
};

}
#endif //SESSIONMANAGER_H

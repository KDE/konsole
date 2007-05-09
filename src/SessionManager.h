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
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <QtCore/QPair>
#include <QtCore/QPointer>
#include <QtCore/QVariant>

class KConfigGroup;
class KDesktopFile;
class KConfig;

namespace Konsole
{

class Session;

class Profile : public QObject 
{
public:
    enum Property
    {
        // Path to profile's config file
        Path,   // QString

        // General profile options
        Name,   // QString
        Title,  // QString
        Icon,   // QString
        Command,        // QString
        Arguments,      // QStringList
        Environment,    // QStringList
        Directory,      // QString

        // Tab Title Formats
        LocalTabTitleFormat,    // QString
        RemoteTabTitleFormat,   // QString

        // Window & Tab Bar 
        ShowMenuBar,    // bool
        TabBarMode,     // TabBarMode

        // Appearence
        Font,           // QFont
        ColorScheme,    // QString

        // Keyboard
        KeyBindings,    // QString

        // Scrolling
        HistoryMode,        // HistoryMode
        HistorySize,        // int
        ScrollBarPosition,  // ScrollBarPosition

        // Terminal Features
        SelectWordCharacters,       // QString   
        BlinkingTextEnabled,        // bool
        FlowControlEnabled,         // bool
        AllowProgramsToResizeWindow,// bool
        BlinkingCursorEnabled,      // bool

        // Cursor Options
        UseCustomCursorColor,       // bool 
        CursorShape,                // CursorShapeEnum
        CustomCursorColor,          // QColor

        // Interaction options
        WordCharacters  // QString
    };

    enum TabBarModeEnum
    {
        AlwaysHideTabBar,
        ShowTabBarAsNeeded,
        AlwaysShowTabBar
    };

    enum HistoryModeEnum
    {
        DisableHistory,
        FixedSizeHistory,
        UnlimitedHistory
    };

    enum ScrollBarPositionEnum
    {
        ScrollBarLeft,
        ScrollBarRight,
        ScrollBarHidden
    };

    enum CursorShapeEnum
    {
        BlockCursor,
        IBeamCursor,
        UnderlineCursor
    };

    /**
     * Constructs a new profile
     */
    Profile(Profile* parent = 0);
    virtual ~Profile() {}

    /** 
     * Changes the parent profile.  When calling the property() method,
     * if the specified property has not been set for this profile,
     * the parent's value for the property will be returned instead.
     */
    void setParent(Profile* parent);

    /** Returns the parent profile. */
    const Profile* parent() const;

    /** 
     * Returns the current value of the specified @p property. 
     *
     * If the specified @p property has not been set in this profile,
     * and a non-null parent was specified in the Profile's constructor,
     * the parent's value for @p property will be returned.
     */
    virtual QVariant property(Property property) const;
    /** Sets the value of the specified @p property to @p value. */
    virtual void setProperty(Property property,const QVariant& value);
    /** Returns true if the specified property has been set in this Profile instance. */
    virtual bool isPropertySet(Property property) const;

    /** Returns a map of the properties set in this Profile instance. */
    virtual QHash<Property,QVariant> setProperties() const;

    /** Returns true if no properties have been set in this Profile instance. */
    bool isEmpty() const;

    /** 
     * Returns true if this is a 'hidden' profile which should not be displayed
     * in menus.
     */
    bool isHidden() const;

    /** Specifies whether this is a hidden profile.  See isHidden() */
    void setHidden(bool hidden);

    //
    // Convenience methods for property() and setProperty() go here
    //

    /** Convenience method for property(Profile::Path) */
    QString path() const { return property(Profile::Path).value<QString>(); }

    /** Convenience method for property(Profile::Name) */
    QString name() const { return property(Profile::Name).value<QString>(); }
    
    /** Convenience method for property(Profile::Directory) */
    QString defaultWorkingDirectory() const 
            { return property(Profile::Directory).value<QString>(); }

    /** Convenience method for property(Profile::Icon) */
    QString icon() const { return property(Profile::Icon).value<QString>(); }

    /** Convenience method for property(Profile::Command) */
    QString command() const { return property(Profile::Command).value<QString>(); }

    /** Convenience method for property(Profile::Arguments) */
    QStringList arguments() const { return property(Profile::Arguments).value<QStringList>(); }

    /** Convenience method for property(Profile::Font) */
    QFont font() const { return property(Profile::Font).value<QFont>(); }

    /** Convenience method for property(Profile::ColorScheme) */
    QString colorScheme() const { return property(Profile::ColorScheme).value<QString>(); }

    /** Convenience method for property(Profile::Environment) */
    QStringList environment() const { return property(Profile::Environment).value<QStringList>(); }

    /** 
     * Convenience method.
     * Returns the value of the "TERM" value in the environment list.
     */
    QString terminal() const { return "xterm"; }

    /**
     * Returns true if @p name has been associated with an element
     * from the Property enum or false otherwise.
     */
    static bool isNameRegistered(const QString& name);

    /** 
     * Returns the element from the Property enum associated with the 
     * specified @p name.
     */
    static Property lookupByName(const QString& name);
    /**
     * Returns the string names associated with the specified @p property from
     * the Property enum, in the order the associations were created using
     * registerName()
     */
    static QList<QString> namesForProperty(Property property); 
    /**
     * Adds an association between a string @p name and a @p property.
     * Subsequent calls to lookupByName() with @p name as the argument
     * will return @p property.
     */
    static void registerName(Property property , const QString& name); 

private:
    QHash<Property,QVariant> _propertyValues;
    QPointer<Profile> _parent;

    bool _hidden;

    static QHash<QString,Property> _propertyNames;
};

class FallbackProfile : public Profile
{
public:
    FallbackProfile();
};

/** Interface for all classes which can load profile settings from a file. */
class ProfileReader
{
public:
    virtual ~ProfileReader() {}
    virtual QStringList findProfiles() { return QStringList(); }
    virtual bool readProfile(const QString& path , Profile* profile) = 0;
};
/** Reads a KDE 3 profile .desktop file. */
class KDE3ProfileReader : public ProfileReader
{
public:
    virtual QStringList findProfiles();
    virtual bool readProfile(const QString& path , Profile* profile);
};
/** Reads a KDE 4 .profile file. */
class KDE4ProfileReader : public ProfileReader
{
public:
    virtual QStringList findProfiles();
    virtual bool readProfile(const QString& path , Profile* profile);
private:
    template <typename T>
    void readStandardElement(const KConfigGroup& group , 
                             char* name , 
                             Profile* info , 
                             Profile::Property property);
};
/** Interface for all classes which can write profile settings to a file. */
class ProfileWriter
{
public:
    virtual ~ProfileWriter() {}
    virtual QString getPath(const Profile* profile) = 0;
    virtual bool writeProfile(const QString& path , const Profile* profile) = 0;
};
/** Writes a KDE 4 .profile file. */
class KDE4ProfileWriter : public ProfileWriter
{
public:
    virtual QString getPath(const Profile* profile);
    virtual bool writeProfile(const QString& path , const Profile* profile);

private:
    void writeStandardElement(KConfigGroup& group,
                              char* name,
                              const Profile* profile,
                              Profile::Property property);
};

#if 0
/** 
 * Provides information about a type of 
 * session, including the title of the session
 * type, whether or not the session will run
 * as root and whether or not the binary
 * for the session is available.
 *
 * The availability of the profile is not determined until the
 * isAvailable() method is called.
 *
 */
class Profile
{
public:

    enum Property
    {
        // General session options
        Name,
        Title,
        Icon,
        Command,
        Arguments,
        Environment,
        Directory,

        // Appearence
        Font,
        ColorScheme,

        // Keyboard
        KeyBindings,

        // Terminal Features
    };

    /**
     * Construct a new Profile
     * to provide information on a profile.
     *
     * @p path Path to the configuration file
     * for this type of session
     */
    Profile(const QString& path);
    
    virtual ~Profile();

    /** Sets the parent profile. */
    void setParent( Profile* parent );
    /** Returns the parent profile. */
    Profile* parent() const;
    /** Sets the value of a property. */
    virtual void setProperty( Property property , const QVariant& value );
    /** Retrieves the value of a property. */
    virtual QVariant property( Property property ) const;

    /**
     * Returns the path to the session's
     * config file
     */
    QString path() const;

    /** Returns the title of the profile */
    QString name() const;
    /**
     * Returns the path of an icon associated
     * with this profile
     */
    QString icon() const;
    /**
     * Returns the command that will be executed
     * when the session is run
     *
     * @param stripSu For commands of the form
     * "su -flags 'commandname'", specifies whether
     * to return the whole command string or just
     * the 'commandname' part
     *
     * eg.  If the command string is
     * "su -c 'screen'", command(true) will
     * just return "screen"
     *
     * @param stripArguments Specifies whether the arguments should be
     * removed from the returned string.  Anything after the first space
     * character in the command string is considered an argument
     */
    QString command(bool stripSu , bool stripArguments = true) const;

    /**
     * Extracts the arguments from the command string for this session.  The first
     * argument is always the command name
     */
    QStringList arguments() const;

    /**
     * Returns true if the session will run as
     * root
     */
    bool isRootSession() const;
    /**
     * Searches the user's PATH for the binary
     * specified in the command string.
     *
     * TODO:  isAvailable() assumes and does not yet verify the
     * existence of additional binaries(usually 'su' or 'sudo') required
     * to run the command as root.
     */
    bool isAvailable() const;

    /**
     * Returns the terminal-type string which is made available to
     * programs running in sessions of this type via the $TERM environment variable.
     *
     * This defaults to "xterm"
     */
    QString terminal() const;

    /** Returns the path of the default keyboard setup file for sessions of this type */
    QString keyboardSetup() const;

    /** Returns the path of the default colour scheme for sessions of this type */
    QString colorScheme() const;

    /**
     * Returns the default font for sessions of this type.
     * If no font is specified in the session's configuration file, @p font will be returned.
     */
    QFont defaultFont() const;

    /** Returns the default working directory for sessions of this type */
    QString defaultWorkingDirectory() const;

    /**
     * Returns the text that should be displayed in menus or in other UI widgets
     * which are used to create new instances of this type of session
     */
    QString newSessionText() const;

private:
    Profile* _parent;
    KDesktopFile* _desktopFile;
    KConfigGroup* _config;
    QString  _path;

    QHash<Property,QVariant> _properties;
};
#endif

/**
 * Creates new terminal sessions using information in configuration files.
 * Information about the available session kinds can be obtained using
 * availableprofiles().  Call createSession() to create a new session.
 * The session will automatically notify the SessionManager when it finishes running.
 *
 * Session types in the manager have a concept of favorite status, which can be used
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
     * Returns a list of keys for registered profiles. 
     */
    QList<QString> availableProfiles() const;
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
     * Updates the profile associated with the specified @p key with
     * the changes specified in @p propertyMap.
     *
     * @param key The profile's key
     * @param propertyMap A map between profile properties and values describing the changes
     * @param persistant If true, the changes are saved to the profile's configuration file
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
     * Creates a new session from the specified profile, using the settings specified
     * using addSetting() and from profile associated with the specified key.
     * The profile must have been previously registered using addprofile()
     * or upon construction of the SessionManager. 
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
     * Returns a list of active sessions.
     */
    const QList<Session*> sessions();

    /**
     * Deletes the profile with the specified key.
     * The configuration file associated with the profile is
     * deleted if possible.
     */
    void deleteProfile(const QString& key);

    /**
     * Sets the profile with the specified key
     * as the default type.
     */
    void setDefaultProfile(const QString& key);

    /**
     * Returns the set of keys for the user's favorite profiles.
     */
    QSet<QString> findFavorites() ;

    /**
     * Specifies whether a profile should be included in the user's
     * list of favorite sessions.
     */
    void setFavorite(const QString& key , bool favorite);

    /** Loads all available profiles. */
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
     * Emitted when the favorite status of a profile changes. 
     * 
     * @param key The key for the profile
     * @param favorite Specifies whether the session is a favorite or not 
     */
    void favoriteStatusChanged(const QString& key , bool favorite);

protected Q_SLOTS:

    /**
     * Called to inform the manager that a session has finished executing
     */
    void sessionTerminated( Session* session );

private:
    //loads the set of favorite sessions
    void loadFavorites();
    //saves the set of favorite sessions
    void saveFavorites();
    //loads a profile from a file
    QString loadProfile(const QString& path);
        // saves a profile to a file
    void saveProfile(const QString& path , Profile* profile);

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

    QHash<QString,Profile*> _types;
    QList<Session*> _sessions;

    QString _defaultProfile;
    
    QSet<QString> _favorites;

    bool _loadedAllProfiles;

    static SessionManager* _instance;
};

}
#endif //SESSIONMANAGER_H

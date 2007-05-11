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

/**
 * Represents a terminal set-up which can be used to 
 * set the initial state of new terminal sessions or applied
 * to existing sessions.
 *
 * Profiles can be loaded from disk using ProfileReader instances
 * and saved to disk using ProfileWriter instances.
 */
class Profile : public QObject 
{
public:
    /**
     * This enum describes the available properties
     * which a Profile may consist of.
     *
     * Properties can be set using setProperty() and read
     * using property()
     */
    enum Property
    {
        /** (QString) Path to the profile's configuration file on-disk. */
        Path,   

        /** (QString) The descriptive name of this profile. */
        Name,   
        /** (QString) TODO: Document me. */
        Title, 
        /** (QString) The name of the icon associated with this profile.  This 
         * is used in menus and tabs to represent the profile. 
         */
        Icon, 
        /** 
         * (QString) The command to execute ( excluding arguments ) when creating a new terminal
         * session using this profile.
         */
        Command,   
        /**
         * (QStringList) The arguments which are passed to the program specified by the Command property
         * when creating a new terminal session using this profile.
         */ 
        Arguments,
        /** 
         * (QStringList) Additional environment variables ( in the form of NAME=VALUE pairs )
         * which are passed to the program specified by the Command property
         * when creating a new terminal session using this profile. 
         */ 
        Environment,
        /** (QString) The initial working directory for sessions created using this profile. */ 
        Directory,      // QString

        /** (QString) The format used for tab titles when running normal commands. */
        LocalTabTitleFormat,   
        /** (QString) The format used for tab titles when the session is running a remote command (eg. SSH) */ 
        RemoteTabTitleFormat,   

        /** (bool) Specifies whether the menu bar should be shown in the main application window. */
        ShowMenuBar,    
        /** (TabBarModeEnum) Specifies when the tab bar should be shown in the main application window. */ 
        TabBarMode,    

        /** (QFont) The font to use in terminal displays using this profile. */
        Font,           
        /** (QString) 
         * The name of the color scheme to use in terminal displays using this profile. 
         * Color schemes are managed by the ColorSchemeManager class. 
         */
        ColorScheme,   

        /** (QString) The name of the key bindings. 
         * Key bindings are managed by the KeyboardTranslatorManager class. 
         */
        KeyBindings, 

        /** (HistoryModeEnum) Specifies the storage type used for keeping the output produced
         * by terminal sessions using this profile.
         */
        HistoryMode,
        /** (int) Specifies the number of lines of output to remember in terminal sessions
         * using this profile.  Once the limit is reached, the oldest lines are lost.
         * Only applicable if the HistoryMode property is FixedSizeHistory
         */
        HistorySize,
        /**
         * (ScrollBarPositionEnum) Specifies the position of the scroll bar in 
         * terminal displays using this profile.
         */
        ScrollBarPosition,  

        /** TODO Document me*/
        SelectWordCharacters,
        /** (bool) Specifies whether text in terminal displays is allowed to blink. */
        BlinkingTextEnabled,       
        /** (bool) Specifies whether the flow control keys ( typically Ctrl+S , Ctrl+Q )
         * have any effect.  Also known as Xon/Xoff
         */ 
        FlowControlEnabled,
        /** (bool) Specifies whether programs running in the terminal are allowed to 
         * resize the terminal display. 
         */
        AllowProgramsToResizeWindow,
        /** (bool) Specifies whether the cursor blinks ( in a manner similar 
         * to text editing applications )
         */
        BlinkingCursorEnabled,      // bool

        /** (bool) If true, terminal displays use a fixed color to draw the cursor,
         * specified by the CustomCursorColor property.  Otherwise the cursor changes
         * color to match the character underneath it.
         */
        UseCustomCursorColor,
        /** (CursorShapeEnum) The shape used by terminal displays to represent the cursor. */ 
        CursorShape,           
        /** (QColor) The color used by terminal displays to draw the cursor.  Only applicable
         * if the UseCustomCursorColor property is true. */ 
        CustomCursorColor,        

        /** TODO Document me */
        // FIXME - Is this a duplicate of SelectWordCharacters?
        WordCharacters  // QString
    };

    /** This enum describes the available modes for showing or hiding the tab bar. */
    enum TabBarModeEnum
    {
        /** The tab bar is never shown. */
        AlwaysHideTabBar,
        /** The tab bar is shown if there are multiple tabs open or hidden otherwise. */
        ShowTabBarAsNeeded,
        /** The tab bar is always shown. */
        AlwaysShowTabBar
    };

    /** 
     * This enum describes the modes available to remember lines of output produced 
     * by the terminal. 
     */
    enum HistoryModeEnum
    {
        /** No output is remembered.  As soon as lines of text are scrolled off-screen they are lost. */
        DisableHistory,
        /** A fixed number of lines of output are remembered.  Once the limit is reached, the oldest
         * lines are lost. */
        FixedSizeHistory,
        /** All output is remembered for the duration of the session.  
         * Typically this means that lines are recorded to
         * a file as they are scrolled off-screen.
         */
        UnlimitedHistory
    };

    /**
     * This enum describes the positions where the terminal display's scroll bar may be placed.
     */
    enum ScrollBarPositionEnum
    {
        /** Show the scroll-bar on the left of the terminal display. */
        ScrollBarLeft,
        /** Show the scroll-bar on the right of the terminal display. */
        ScrollBarRight,
        /** Do not show the scroll-bar. */
        ScrollBarHidden
    };

    /** This enum describes the shapes used to draw the cursor in terminal displays. */
    enum CursorShapeEnum
    {
        /** Use a solid rectangular block to draw the cursor. */
        BlockCursor,
        /** Use an 'I' shape, similar to that used in text editing applications, to draw the cursor. */
        IBeamCursor,
        /** Draw a line underneath the cursor's position. */
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

/** 
 * A profile which contains a number of default settings for various properties.
 * This can be used as a parent for other profiles or a fallback in case
 * a profile cannot be loaded from disk.
 */
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
    /** Returns a list of paths to profiles which this reader can read. */
    virtual QStringList findProfiles() { return QStringList(); }
    /** 
     * Attempts to read a profile from @p path and 
     * save the property values described into @p profile.
     *
     * Returns true if the profile was successfully read or false otherwise.
     */
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
    /** 
     * Returns a suitable path-name for writing 
     * @p profile to. The path-name should be accepted by
     * the corresponding ProfileReader class.
     */
    virtual QString getPath(const Profile* profile) = 0;
    /**
     * Writes the properties and values from @p profile to the file specified by
     * @p path.  This profile should be readable by the corresponding ProfileReader class.
     */
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

/**
 * Manages running terminal sessions and the profiles which specify various
 * settings for terminal sessions and their displays.
 *
 *
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

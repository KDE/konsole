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
#include <QFont>
#include <QHash>
#include <QList>
#include <QSet>
#include <QStringList>
#include <QPair>
#include <QVariant>

class KConfigGroup;
class KDesktopFile;
class KConfig;

namespace Konsole
{

class Session;

/** Replacement for Profile */
class SessionSettings 
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
        SelectWordCharacters
        
    };

    virtual ~SessionSettings() {}

    /** Returns the current value of the specified @p property. */
    virtual QVariant property(Property property) const;
    /** Sets the value of the specified @p property to @p value. */
    virtual void setProperty(Property property,const QVariant& value);


    //
    // Convenience methods for property() and setProperty() go here
    //

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
    
    static QHash<QString,Property> _propertyNames;
};

/** 
 * Provides information about a type of 
 * session, including the title of the session
 * type, whether or not the session will run
 * as root and whether or not the binary
 * for the session is available.
 *
 * The availability of the session type is not determined until the
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
     * to provide information on a session type.
     *
     * @p path Path to the configuration file
     * for this type of session
     */
    Profile(const QString& path);
    
    virtual ~Profile();

    /** Sets the parent session type. */
    void setParent( Profile* parent );
    /** Returns the parent session type. */
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

    /** Returns the title of the session type */
    QString name() const;
    /**
     * Returns the path of an icon associated
     * with this session type
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

/**
 * Creates new terminal sessions using information in configuration files.
 * Information about the available session kinds can be obtained using
 * availableSessionTypes().  Call createSession() to create a new session.
 * The session will automatically notify the SessionManager when it finishes running.
 *
 * Session types in the manager have a concept of favorite status, which can be used
 * by widgets and dialogs in the application decide which sessions to list and
 * how to display them.  The favorite status of a session type can be altered using
 * setFavorite() and retrieved using isFavorite() 
 */
class SessionManager : public QObject
{
Q_OBJECT

public:
    /** 
     * Constructs a new session manager and loads information about the available 
     * session types.
     */
    SessionManager();
    virtual ~SessionManager();

    /** document me */
    enum Setting
    {
        Font                     = 0,
        InitialWorkingDirectory  = 1,
        ColorScheme              = 2,
        HistoryEnabled           = 3,
        HistorySize              = 4  // set to 0 for unlimited history ( stored in a file )
    };

    /** document me */

    //The values of these settings are significant, higher priority sources
    //have higher values
    enum Source
    {
        ApplicationDefault  = 0,
        GlobalConfig        = 1,
        SessionConfig       = 2,
        Commandline         = 3,
        Action              = 4,
        SingleShot          = 5
    };

    /**
     * Returns a list of keys for registered session types.      
     */
    QList<QString> availableSessionTypes() const;
    /**
     * Returns the session information object for the session type with the specified
     * key or 0 if no session type with the specified key exists.
     *
     * If @p key is empty, a pointer to the default session type is returned.
     */
    Profile* sessionType(const QString& key) const;

    /**
     * Registers a new type of session and returns the key
     * which can be passed to createSession() to create new instances of the session.
     *
     * The favorite status of the session ( as returned by isFavorite() ) is set
     * to false by default.
     */
    QString addSessionType(Profile* type);

    /**
     * Returns a Profile object describing the default type of session, which is used
     * if createSession() is called with an empty configPath argument.
     */
    Profile* defaultSessionType() const;

    /**
     * Returns the key for the default session type.
     */
    QString defaultSessionKey() const;

    /**
     * Adds a setting which will be considered when creating new sessions.
     * Each setting ( such as terminal font , initial working directory etc. )
     * can be specified by multiple different sources.  The
     *
     * For example, the working directory in which a new session starts is specified
     * in the configuration file for that session type, but can be overridden
     * by creating a new session from a bookmark or specifying what to use on
     * the command line.
     *
     * The active value for a setting (ie. the one which will actually be used when
     * creating the session) can be found using activeSetting()
     *
     * @p setting The setting to change
     * @p source Specifies where the setting came from.
     * @p value The new value for this setting,source pair
     */
    void addSetting( Setting setting , Source source , const QVariant& value );


    /**
     * Returns the value for a particular setting which will be used
     * when a new session is created.
     *
     * Values for settings come from different places, such as the command-line,
     * config files and menu options.
     *
     * The active setting is the value for the setting which comes from the source
     * with the highest priority.  For example, a setting specified on the command-line
     * when Konsole is launched will take priority over a setting specified in a
     * configuration file.
     */
    QVariant activeSetting( Setting setting ) const;

    /**
     * Creates a new session of the specified type, using the settings specified
     * using addSetting() and from session type associated with the specified key.
     * The session type must have been previously registered using addSessionType()
     * or upon construction of the SessionManager. 
     *
     * The new session has no views associated with it.  A new TerminalDisplay view
     * must be created in order to display the output from the terminal session and
     * send keyboard or mouse input to it.
     *
     * @param type Specifies the type of session to create.  Passing an empty
     *             string will create a session using the default configuration.
     */
    Session* createSession(QString key = QString());

    /**
     * Returns a list of active sessions.
     */
    const QList<Session*> sessions();

    /**
     * Deletes the session type with the specified key.
     * The configuration file associated with the session type is
     * deleted if possible.
     */
    void deleteSessionType(const QString& key);

    /**
     * Sets the session type with the specified key
     * as the default type.
     */
    void setDefaultSessionType(const QString& key);

    /**
     * Returns the set of keys for the user's favorite session types.
     */
    QSet<QString> favorites() const;

    /**
     * Specifies whether a session type should be included in the user's
     * list of favorite sessions.
     */
    void setFavorite(const QString& key , bool favorite);

    /**
     * Sets the global session manager instance.
     */
    static void setInstance(SessionManager* instance);
    /**
     * Returns the session manager instance.
     */
    static SessionManager* instance();

signals:
    /** Emitted when a session type is added to the manager. */
    void sessionTypeAdded(const QString& key);
    /** Emitted when a session type is removed from the manager. */
    void sessionTypeRemoved(const QString& key);
    /** 
     * Emitted when the favorite status of a session type changes. 
     * 
     * @param key The key for the session type
     * @param favorite Specifies whether the session is a favorite or not 
     */
    void favoriteStatusChanged(const QString& key , bool favorite);

protected Q_SLOTS:

    /**
     * Called to inform the manager that a session has finished executing
     */
    void sessionTerminated( Session* session );

private:
    //fills the settings store with the settings from the session config file
    void pushSessionSettings( const Profile*  info );
    //loads the set of favorite sessions 
    void loadFavorites();
    //saves the set of favorite sessions
    void saveFavorites();

    QHash<QString,Profile*> _types;
    QList<Session*> _sessions;

    QString _defaultSessionType;
    
    typedef QPair<Source,QVariant> SourceVariant;

    QHash< Setting , QList< SourceVariant > >  _settings;

    QSet<QString> _favorites;

    static SessionManager* _instance;
};

};
#endif //SESSIONMANAGER_H

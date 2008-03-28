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

#ifndef PROFILE_H
#define PROFILE_H

// Qt
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

#include <QtGui/QFont>

// KDE
#include <KSharedPtr>

class KConfig;
class KConfigGroup;

namespace Konsole
{

/**
 * Represents a terminal set-up which can be used to 
 * set the initial state of new terminal sessions or applied
 * to existing sessions.  Profiles consist of a number of named
 * properties, which can be retrieved using property() and
 * set using setProperty().  isPropertySet() can be used to check
 * whether a particular property has been set in a profile.
 *
 * Profiles support a simple form of inheritance.  When a new Profile
 * is constructed, a pointer to a parent profile can be passed to
 * the constructor.  When querying a particular property of a profile
 * using property(), the profile will return its own value for that 
 * property if one has been set or otherwise it will return the
 * parent's value for that property.  
 *
 * Profiles can be loaded from disk using ProfileReader instances
 * and saved to disk using ProfileWriter instances.
 *
 * TODO: Profile inherits from QObject so that it can store a guarded 
 * pointer to the parent profile using QPointer<Profile>.  Try to find
 * a more light-weight solution to this.  
 */
class Profile : public QSharedData 
{

friend class KDE4ProfileReader;
friend class KDE4ProfileWriter;

public:
	typedef KSharedPtr<Profile> Ptr;

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
        /** (QString) Title of this profile that will be displayed. */
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
        Directory,

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
        BlinkingCursorEnabled,

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

        /** (QString) A string consisting of the characters used to delimit words when
         * selecting text in the terminal display.
         */
        WordCharacters,

        /** (TabBarPositionEnum) Position of the tab-bar relative to the terminal displays. */
        TabBarPosition,

        /** (String) Default text codec */
        DefaultEncoding,

        /** (bool) Whether fonts should be aliased or not */
        AntiAliasFonts,

		/** (bool) Whether new sessions should be started in the same directory as the 
		 * currently active session. */
		StartInCurrentSessionDir
    };

    /** 
     * This enum describes the available modes for showing or hiding the tab bar. 
     */
    enum TabBarModeEnum
    {
        /** The tab bar is never shown. */
        AlwaysHideTabBar   = 0,
        /** The tab bar is shown if there are multiple tabs open or hidden otherwise. */
        ShowTabBarAsNeeded = 1,
        /** The tab bar is always shown. */
        AlwaysShowTabBar   = 2
    };

    /** 
     * This enum describes the available tab bar positions. 
     */
    enum TabBarPositionEnum
    {
        /** Show tab bar below displays. */
        TabBarBottom = 0,
        /** Show tab bar above displays. */
        TabBarTop    = 1
    };

    /** 
     * This enum describes the modes available to remember lines of output produced 
     * by the terminal. 
     */
    enum HistoryModeEnum
    {
        /** No output is remembered.  As soon as lines of text are scrolled off-screen they are lost. */
        DisableHistory   = 0,
        /** A fixed number of lines of output are remembered.  Once the limit is reached, the oldest
         * lines are lost. */
        FixedSizeHistory = 1,
        /** All output is remembered for the duration of the session.  
         * Typically this means that lines are recorded to
         * a file as they are scrolled off-screen.
         */
        UnlimitedHistory = 2
    };

    /**
     * This enum describes the positions where the terminal display's scroll bar may be placed.
     */
    enum ScrollBarPositionEnum
    {
        /** Show the scroll-bar on the left of the terminal display. */
        ScrollBarLeft   = 0,
        /** Show the scroll-bar on the right of the terminal display. */
        ScrollBarRight  = 1,
        /** Do not show the scroll-bar. */
        ScrollBarHidden = 2
    };

    /** This enum describes the shapes used to draw the cursor in terminal displays. */
    enum CursorShapeEnum
    {
        /** Use a solid rectangular block to draw the cursor. */
        BlockCursor     = 0,
        /** Use an 'I' shape, similar to that used in text editing applications, to draw the cursor. */
        IBeamCursor     = 1,
        /** Draw a line underneath the cursor's position. */
        UnderlineCursor = 2
    };

    /**
     * Constructs a new profile
     */
    explicit Profile(Ptr parent = Ptr());
    virtual ~Profile(); 

    /** 
     * Changes the parent profile.  When calling the property() method,
     * if the specified property has not been set for this profile,
     * the parent's value for the property will be returned instead.
     */
    void setParent(Ptr parent);

    /** Returns the parent profile. */
    const Ptr parent() const;

    /** 
     * Returns the current value of the specified @p property, cast to type T.
	 * Internally properties are stored using the QVariant type and cast to T
	 * using QVariant::value<T>();
     *
     * If the specified @p property has not been set in this profile,
     * and a non-null parent was specified in the Profile's constructor,
     * the parent's value for @p property will be returned.
     */
    template <class T>
	T property(Property property) const;
	
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
     * in menus or saved to disk.
     *
     * This is used for the fallback profile, in case there are no profiles on 
     * disk which can be loaded, or for overlay profiles created to handle
     * command-line arguments which change profile properties.
     */
    bool isHidden() const;

    /** Specifies whether this is a hidden profile.  See isHidden() */
    void setHidden(bool hidden);

    //
    // Convenience methods for property() and setProperty() go here
    //

    /** Convenience method for property(Profile::Path) */
    QString path() const { return property<QString>(Profile::Path); }

    /** Convenience method for property(Profile::Name) */
    QString name() const { return property<QString>(Profile::Name); }
    
    /** Convenience method for property(Profile::Directory) */
    QString defaultWorkingDirectory() const 
            { return property<QString>(Profile::Directory); }

    /** Convenience method for property(Profile::Icon) */
    QString icon() const { return property<QString>(Profile::Icon); }

    /** Convenience method for property(Profile::Command) */
    QString command() const { return property<QString>(Profile::Command); }

    /** Convenience method for property(Profile::Arguments) */
    QStringList arguments() const { return property<QStringList>(Profile::Arguments); }

    /** Convenience method for property(Profile::Font) */
    QFont font() const { return property<QFont>(Profile::Font); }

    /** Convenience method for property(Profile::ColorScheme) */
    QString colorScheme() const { return property<QString>(Profile::ColorScheme); }

    /** Convenience method for property(Profile::Environment) */
    QStringList environment() const { return property<QStringList>(Profile::Environment); }

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
     *
     * @param The name of the property to look for, this is case insensitive.
     */
    static Property lookupByName(const QString& name);
    /**
     * Returns the string names associated with the specified @p property from
     * the Property enum, in the order the associations were created using
     * registerName()
     */
    static QList<QString> namesForProperty(Property property);

    /** 
     * Returns the primary name for the specified @p property.
     * TODO More documentation
     */
    static QString primaryNameForProperty(Property property);

private:
    struct PropertyInfo;
	/**
     * Adds an association between a string @p name and a @p property.
     * Subsequent calls to lookupByName() with @p name as the argument
     * will return @p property.
     */
    static void registerProperty(const PropertyInfo& info); 

	// fills the table with default names for profile properties
    // the first time it is called.
    // subsequent calls return immediately
    static void fillTableWithDefaultNames();

    QHash<Property,QVariant> _propertyValues;
    Ptr _parent;

    bool _hidden;

    static QHash<QString,PropertyInfo> _propertyInfoByName;
    static QHash<Property,PropertyInfo> _infoByProperty;
    
	struct PropertyInfo
    {
        Property property;
        const char* name;
		const char* group;
		QVariant::Type type;
    };
    static const PropertyInfo DefaultPropertyNames[];
};

template <class T>
inline T Profile::property(Property theProperty) const
{
	return property<QVariant>(theProperty).value<T>();
}
template <>
inline QVariant Profile::property(Property property) const
{
	bool canInheritProperty = property != Path;

    if ( _propertyValues.contains(property) )
        return _propertyValues[property];
    else if ( _parent && canInheritProperty )
        return _parent->property<QVariant>(property);
    else
        return QVariant();
}

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
     *
     * @param path Path to the profile to read
     * @param profile Pointer to the Profile the settings will be read into
     * @param parentProfile Receives the name of the parent profile specified in
     */
    virtual bool readProfile(const QString& path , Profile::Ptr profile , QString& parentProfile) = 0;
};
/** Reads a KDE 3 profile .desktop file. */
class KDE3ProfileReader : public ProfileReader
{
public:
    virtual QStringList findProfiles();
    virtual bool readProfile(const QString& path , Profile::Ptr profile, QString& parentProfile);
};
/** Reads a KDE 4 .profile file. */
class KDE4ProfileReader : public ProfileReader
{
public:
    virtual QStringList findProfiles();
    virtual bool readProfile(const QString& path , Profile::Ptr profile, QString& parentProfile);
private:
	void readProperties(const KConfig& config, Profile::Ptr profile, 
						const Profile::PropertyInfo* properties);
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
    virtual QString getPath(const Profile::Ptr profile) = 0;
    /**
     * Writes the properties and values from @p profile to the file specified by
     * @p path.  This profile should be readable by the corresponding ProfileReader class.
     */
    virtual bool writeProfile(const QString& path , const Profile::Ptr profile) = 0;
};
/** Writes a KDE 4 .profile file. */
class KDE4ProfileWriter : public ProfileWriter
{
public:
    virtual QString getPath(const Profile::Ptr profile);
    virtual bool writeProfile(const QString& path , const Profile::Ptr profile);

private:
	void writeProperties(KConfig& config, const Profile::Ptr profile, 
						 const Profile::PropertyInfo* properties);
};

/** 
 * Parses an input string consisting of property names
 * and assigned values and returns a table of properties
 * and values.
 *
 * The input string will typically look like this:
 *
 * @code
 *   PropertyName=Value;PropertyName=Value ...
 * @endcode 
 *
 * For example:
 *
 * @code
 *   Icon=konsole;Directory=/home/bob
 * @endcode
 */
class ProfileCommandParser
{
public:
    /**
     * Parses an input string consisting of property names
     * and assigned values and returns a table of 
     * properties and values.
     */
    QHash<Profile::Property,QVariant> parse(const QString& input);

};

}
Q_DECLARE_METATYPE(Konsole::Profile::Ptr)

#endif // PROFILE_H

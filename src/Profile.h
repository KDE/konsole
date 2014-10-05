/*
    This source file is part of Konsole, a terminal emulator.

    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>

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
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtGui/QFont>
#include <QtGui/QColor>

// Konsole
#include "konsole_export.h"

namespace Konsole
{
class ProfileGroup;

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
 */
class KONSOLEPRIVATE_EXPORT Profile : public QSharedData
{
    friend class KDE4ProfileReader;
    friend class KDE4ProfileWriter;
    friend class ProfileGroup;

public:
    typedef QExplicitlySharedDataPointer<Profile> Ptr;
    typedef QExplicitlySharedDataPointer<ProfileGroup> GroupPtr;

    /**
     * This enum describes the available properties
     * which a Profile may consist of.
     *
     * Properties can be set using setProperty() and read
     * using property()
     */
    enum Property {
        /** (QString) Path to the profile's configuration file on-disk. */
        Path,
        /** (QString) The descriptive name of this profile. */
        Name,
        /** (QString) The untranslated name of this profile.
         * Warning: this is an internal property. Do not touch it.
         */
        UntranslatedName,
        /** (QString) The name of the icon associated with this profile.
         * This is used in menus and tabs to represent the profile.
         */
        Icon,
        /** (QString) The command to execute ( excluding arguments ) when
         * creating a new terminal session using this profile.
         */
        Command,
        /** (QStringList) The arguments which are passed to the program
         * specified by the Command property when creating a new terminal
         * session using this profile.
         */
        Arguments,
        /** (QStringList) Additional environment variables (in the form of
         * NAME=VALUE pairs) which are passed to the program specified by
         * the Command property when creating a new terminal session using
         * this profile.
         */
        Environment,
        /** (QString) The initial working directory for sessions created
         * using this profile.
         */
        Directory,
        /** (QString) The format used for tab titles when running normal
          * commands.
          */
        LocalTabTitleFormat,
        /** (QString) The format used for tab titles when the session is
         * running a remote command (eg. SSH)
         */
        RemoteTabTitleFormat,
        /** (bool) Specifies whether show hint for terminal size after
         * resizing the application window.
         */
        ShowTerminalSizeHint,
        /** (QFont) The font to use in terminal displays using this profile. */
        Font,
        /** (QString) The name of the color scheme to use in terminal
         * displays using this profile.
         * Color schemes are managed by the ColorSchemeManager class.
         */
        ColorScheme,
        /** (QString) The name of the key bindings.
         * Key bindings are managed by the KeyboardTranslatorManager class.
         */
        KeyBindings,
        /** (HistoryModeEnum) Specifies the storage type used for keeping
         * the output produced by terminal sessions using this profile.
         *
         * See Enum::HistoryModeEnum
         */
        HistoryMode,
        /** (int) Specifies the number of lines of output to remember in
         * terminal sessions using this profile.  Once the limit is reached,
         * the oldest lines are lost if the HistoryMode property is
         * FixedSizeHistory
         */
        HistorySize,
        /** (ScrollBarPositionEnum) Specifies the position of the scroll bar
         * in terminal displays using this profile.
         *
         * See Enum::ScrollBarPositionEnum
         */
        ScrollBarPosition,
	/** (bool) Specifies whether the PageUp/Down will scroll the full
	 * height or half height.
         */
        ScrollFullPage,
        /** (bool) Specifies whether the terminal will enable Bidirectional
         * text display
         */
        BidiRenderingEnabled,
        /** (bool) Specifies whether text in terminal displays is allowed
         * to blink.
         */
        BlinkingTextEnabled,
        /** (bool) Specifies whether the flow control keys (typically Ctrl+S,
         * Ctrl+Q) have any effect.  Also known as Xon/Xoff
         */
        FlowControlEnabled,
        /** (int) Specifies the pixels between the terminal lines.
         */
        LineSpacing,
        /** (bool) Specifies whether the cursor blinks ( in a manner similar
         * to text editing applications )
         */
        BlinkingCursorEnabled,
        /** (bool) If true, terminal displays use a fixed color to draw the
         * cursor, specified by the CustomCursorColor property.  Otherwise
         * the cursor changes color to match the character underneath it.
         */
        UseCustomCursorColor,
        /** (CursorShapeEnum) The shape used by terminal displays to
         * represent the cursor.
         *
         * See Enum::CursorShapeEnum
         */
        CursorShape,
        /** (QColor) The color used by terminal displays to draw the cursor.
         * Only applicable if the UseCustomCursorColor property is true.
         */
        CustomCursorColor,
        /** (QString) A string consisting of the characters used to delimit
         * words when selecting text in the terminal display.
         */
        WordCharacters,
        /** (TripleClickModeEnum) Specifies which part of current line should
         * be selected with triple click action.
         *
         * See Enum::TripleClickModeEnum
         */
        TripleClickMode,
        /** (bool) If true, text that matches a link or an email address is
         * underlined when hovered by the mouse pointer.
         */
        UnderlineLinksEnabled,
        /** (bool) If true, links can be opened by direct mouse click.*/
        OpenLinksByDirectClickEnabled,
        /** (bool) If true, control key must be pressed to click and drag selected text. */
        CtrlRequiredForDrag,
        /** (bool) If true, automatically copy selected text into the clipboard */
        AutoCopySelectedText,
        /** (bool) If true, trailing spaces are trimmed in selected text */
        TrimTrailingSpacesInSelectedText,
        /** (bool) If true, middle mouse button pastes from X Selection */
        PasteFromSelectionEnabled,
        /** (bool) If true, middle mouse button pastes from Clipboard */
        PasteFromClipboardEnabled,
        /** (MiddleClickPasteModeEnum) Specifies the source from which mouse
         * middle click pastes data.
         *
         * See Enum::MiddleClickPasteModeEnum
         */
        MiddleClickPasteMode,
        /** (String) Default text codec */
        DefaultEncoding,
        /** (bool) Whether fonts should be aliased or not */
        AntiAliasFonts,
        /** (bool) Whether character with intense colors should be rendered
         * in bold font or just in bright color. */
        BoldIntense,
        /** (bool) Whether new sessions should be started in the same
         * directory as the currently active session.
         */
        StartInCurrentSessionDir,
        /** (int) Specifies the threshold of detected silence in seconds. */
        SilenceSeconds,
        /** (BellModeEnum) Specifies the behavior of bell.
         *
         * See Enum::BellModeEnum
         */
        BellMode,
        /** (int) Specifies the preferred columns. */
        TerminalColumns,
        /** (int) Specifies the preferred rows. */
        TerminalRows,
        /** Index of profile in the File Menu
         * WARNING: this is currently an internal field, which is
         * expected to be zero on disk. Do not modify it manually.
         *
         * In future, the format might be #.#.# to account for levels
         */
        MenuIndex,
        /** (bool) If true, mouse wheel scroll with Ctrl key pressed
         * increases/decreases the terminal font size.
         */
        MouseWheelZoomEnabled
    };

    /**
     * Constructs a new profile
     *
     * @param parent The parent profile.  When querying the value of a
     * property using property(), if the property has not been set in this
     * profile then the parent's value for the property will be returned.
     */
    explicit Profile(Ptr parent = Ptr());
    virtual ~Profile();

    /**
     * Copies all properties except Name and Path from the specified @p
     * profile into this profile
     *
     * @param profile The profile to copy properties from
     * @param differentOnly If true, only properties in @p profile which have
     * a different value from this profile's current value (either set via
     * setProperty() or inherited from the parent profile) will be set.
     */
    void clone(Ptr profile, bool differentOnly = true);

    /**
     * Changes the parent profile.  When calling the property() method,
     * if the specified property has not been set for this profile,
     * the parent's value for the property will be returned instead.
     */
    void setParent(Ptr parent);

    /** Returns the parent profile. */
    const Ptr parent() const;

    /** Returns this profile as a group or null if this profile is not a
     * group.
     */
    const GroupPtr asGroup() const;
    GroupPtr asGroup();

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
    virtual void setProperty(Property property, const QVariant& value);
    /** Returns true if the specified property has been set in this Profile
     * instance.
     */
    virtual bool isPropertySet(Property property) const;

    /** Returns a map of the properties set in this Profile instance. */
    virtual QHash<Property, QVariant> setProperties() const;

    /** Returns true if no properties have been set in this Profile instance. */
    bool isEmpty() const;

    /**
     * Returns true if this is a 'hidden' profile which should not be
     * displayed in menus or saved to disk.
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

    /** Convenience method for property<QString>(Profile::Path) */
    QString path() const {
        return property<QString>(Profile::Path);
    }

    /** Convenience method for property<QString>(Profile::Name) */
    QString name() const {
        return property<QString>(Profile::Name);
    }

    /** Convenience method for property<QString>(Profile::UntranslatedName) */
    QString untranslatedName() const {
        return property<QString>(Profile::UntranslatedName);
    }

    /** Convenience method for property<QString>(Profile::Directory) */
    QString defaultWorkingDirectory() const {
        return property<QString>(Profile::Directory);
    }

    /** Convenience method for property<QString>(Profile::Icon) */
    QString icon() const {
        return property<QString>(Profile::Icon);
    }

    /** Convenience method for property<QString>(Profile::Command) */
    QString command() const {
        return property<QString>(Profile::Command);
    }

    /** Convenience method for property<QStringList>(Profile::Arguments) */
    QStringList arguments() const {
        return property<QStringList>(Profile::Arguments);
    }

    /** Convenience method for property<QString>(Profile::LocalTabTitleFormat) */
    QString localTabTitleFormat() const {
        return property<QString>(Profile::LocalTabTitleFormat);
    }

    /** Convenience method for property<QString>(Profile::RemoteTabTitleFormat) */
    QString remoteTabTitleFormat() const {
        return property<QString>(Profile::RemoteTabTitleFormat);
    }

    /** Convenience method for property<bool>(Profile::ShowTerminalSizeHint) */
    bool showTerminalSizeHint() const {
        return property<bool>(Profile::ShowTerminalSizeHint);
    }

    /** Convenience method for property<QFont>(Profile::Font) */
    QFont font() const {
        return property<QFont>(Profile::Font);
    }

    /** Convenience method for property<QString>(Profile::ColorScheme) */
    QString colorScheme() const {
        return property<QString>(Profile::ColorScheme);
    }

    /** Convenience method for property<QStringList>(Profile::Environment) */
    QStringList environment() const {
        return property<QStringList>(Profile::Environment);
    }

    /** Convenience method for property<QString>(Profile::KeyBindings) */
    QString keyBindings() const {
        return property<QString>(Profile::KeyBindings);
    }

    /** Convenience method for property<QString>(Profile::HistorySize) */
    int historySize() const {
        return property<int>(Profile::HistorySize);
    }

    /** Convenience method for property<bool>(Profile::BidiRenderingEnabled) */
    bool bidiRenderingEnabled() const {
        return property<bool>(Profile::BidiRenderingEnabled);
    }

    /** Convenience method for property<bool>(Profile::LineSpacing) */
    int lineSpacing() const {
        return property<int>(Profile::LineSpacing);
    }


    /** Convenience method for property<bool>(Profile::BlinkingTextEnabled) */
    bool blinkingTextEnabled() const {
        return property<bool>(Profile::BlinkingTextEnabled);
    }

    /** Convenience method for property<bool>(Profile::MouseWheelZoomEnabled) */
    bool mouseWheelZoomEnabled() const {
        return property<bool>(Profile::MouseWheelZoomEnabled);
    }

    /** Convenience method for property<bool>(Profile::BlinkingCursorEnabled) */
    bool blinkingCursorEnabled() const {
        return property<bool>(Profile::BlinkingCursorEnabled);
    }

    /** Convenience method for property<bool>(Profile::FlowControlEnabled) */
    bool flowControlEnabled() const {
        return property<bool>(Profile::FlowControlEnabled);
    }

    /** Convenience method for property<bool>(Profile::UseCustomCursorColor) */
    bool useCustomCursorColor() const {
        return property<bool>(Profile::UseCustomCursorColor);
    }

    /** Convenience method for property<bool>(Profile::CustomCursorColor) */
    QColor customCursorColor() const {
        return property<QColor>(Profile::CustomCursorColor);
    }

    /** Convenience method for property<QString>(Profile::WordCharacters) */
    QString wordCharacters() const {
        return property<QString>(Profile::WordCharacters);
    }

    /** Convenience method for property<bool>(Profile::UnderlineLinksEnabled) */
    bool underlineLinksEnabled() const {
        return property<bool>(Profile::UnderlineLinksEnabled);
    }

    bool autoCopySelectedText() const {
        return property<bool>(Profile::AutoCopySelectedText);
    }

    /** Convenience method for property<QString>(Profile::DefaultEncoding) */
    QString defaultEncoding() const {
        return property<QString>(Profile::DefaultEncoding);
    }

    /** Convenience method for property<bool>(Profile::AntiAliasFonts) */
    bool antiAliasFonts() const {
        return property<bool>(Profile::AntiAliasFonts);
    }

    /** Convenience method for property<bool>(Profile::BoldIntense) */
    bool boldIntense() const {
        return property<bool>(Profile::BoldIntense);
    }

    /** Convenience method for property<bool>(Profile::StartInCurrentSessionDir) */
    bool startInCurrentSessionDir() const {
        return property<bool>(Profile::StartInCurrentSessionDir);
    }

    /** Convenience method for property<QString>(Profile::SilenceSeconds) */
    int silenceSeconds() const {
        return property<int>(Profile::SilenceSeconds);
    }

    /** Convenience method for property<QString>(Profile::TerminalColumns) */
    int terminalColumns() const {
        return property<int>(Profile::TerminalColumns);
    }

    /** Convenience method for property<QString>(Profile::TerminalRows) */
    int terminalRows() const {
        return property<int>(Profile::TerminalRows);
    }

    /** Convenience method for property<QString>(Profile::MenuIndex) */
    QString menuIndex() const {
        return property<QString>(Profile::MenuIndex);
    }

    int menuIndexAsInt() const;

    /** Return a list of all properties names and their type
     *  (for use with -p option).
     */
    const QStringList propertiesInfoList() const;

    /**
     * Returns the element from the Property enum associated with the
     * specified @p name.
     *
     * @param name The name of the property to look for, this is case
     * insensitive.
     */
    static Property lookupByName(const QString& name);

private:
    struct PropertyInfo;
    // Defines a new property, this property is then available
    // to all Profile instances.
    static void registerProperty(const PropertyInfo& info);

    // fills the table with default names for profile properties
    // the first time it is called.
    // subsequent calls return immediately
    static void fillTableWithDefaultNames();

    // returns true if the property can be inherited
    static bool canInheritProperty(Property property);

    QHash<Property, QVariant> _propertyValues;
    Ptr _parent;

    bool _hidden;

    static QHash<QString, PropertyInfo> PropertyInfoByName;
    static QHash<Property, PropertyInfo> PropertyInfoByProperty;

    // Describes a property.  Each property has a name and group
    // which is used when saving/loading the profile.
    struct PropertyInfo {
        Property property;
        const char* name;
        const char* group;
        QVariant::Type type;
    };
    static const PropertyInfo DefaultPropertyNames[];
};

inline bool Profile::canInheritProperty(Property aProperty)
{
    return aProperty != Name && aProperty != Path;
}

template <class T>
inline T Profile::property(Property aProperty) const
{
    return property<QVariant>(aProperty).value<T>();
}
template <>
inline QVariant Profile::property(Property aProperty) const
{
    if (_propertyValues.contains(aProperty)) {
        return _propertyValues[aProperty];
    } else if (_parent && canInheritProperty(aProperty)) {
        return _parent->property<QVariant>(aProperty);
    } else {
        return QVariant();
    }
}

/**
 * A profile which contains a number of default settings for various
 * properties.  This can be used as a parent for other profiles or a
 * fallback in case a profile cannot be loaded from disk.
 */
class FallbackProfile : public Profile
{
public:
    FallbackProfile();
};

/**
 * A composite profile which allows a group of profiles to be treated as one.
 * When setting a property, the new value is applied to all profiles in the
 * group.  When reading a property, if all profiles in the group have the same
 * value then that value is returned, otherwise the result is null.
 *
 * Profiles can be added to the group using addProfile().  When all profiles
 * have been added updateValues() must be called
 * to sync the group's property values with those of the group's profiles.
 *
 * The Profile::Name and Profile::Path properties are unique to individual
 * profiles, setting these properties on a ProfileGroup has no effect.
 */
class KONSOLEPRIVATE_EXPORT ProfileGroup : public Profile
{
public:
    typedef QExplicitlySharedDataPointer<ProfileGroup> Ptr;

    /** Construct a new profile group, which is hidden by default. */
    explicit ProfileGroup(Profile::Ptr parent = Profile::Ptr());

    /** Add a profile to the group.  Calling setProperty() will update this
     * profile.  When creating a group, add the profiles to the group then
     * call updateValues() to make the group's property values reflect the
     * profiles currently in the group.
     */
    void addProfile(Profile::Ptr profile) {
        _profiles.append(profile);
    }

    /** Remove a profile from the group.  Calling setProperty() will no longer
     * affect this profile. */
    void removeProfile(Profile::Ptr profile) {
        _profiles.removeAll(profile);
    }

    /** Returns the profiles in this group .*/
    QList<Profile::Ptr> profiles() const {
        return _profiles;
    }

    /**
     * Updates the property values in this ProfileGroup to match those from
     * the group's profiles()
     *
     * For each available property, if each profile in the group has the same
     * value then the ProfileGroup will use that value for the property.
     * Otherwise the value for the property will be set to a null QVariant
     *
     * Some properties such as the name and the path of the profile
     * will always be set to null if the group has more than one profile.
     */
    void updateValues();

    /** Sets the value of @p property in each of the group's profiles to
     * @p value.
     */
    void setProperty(Property property, const QVariant& value);

private:
    QList<Profile::Ptr> _profiles;
};
inline ProfileGroup::ProfileGroup(Profile::Ptr profileParent)
    : Profile(profileParent)
{
    setHidden(true);
}
inline const Profile::GroupPtr Profile::asGroup() const
{
    const Profile::GroupPtr ptr(dynamic_cast<ProfileGroup*>(
                                    const_cast<Profile*>(this)));
    return ptr;
}
inline Profile::GroupPtr Profile::asGroup()
{
    return Profile::GroupPtr(dynamic_cast<ProfileGroup*>(this));
}

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
class KONSOLEPRIVATE_EXPORT ProfileCommandParser
{
public:
    /**
     * Parses an input string consisting of property names
     * and assigned values and returns a table of
     * properties and values.
     */
    QHash<Profile::Property, QVariant> parse(const QString& input);
};
}
Q_DECLARE_METATYPE(Konsole::Profile::Ptr)

#endif // PROFILE_H

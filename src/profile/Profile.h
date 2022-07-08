/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PROFILE_H
#define PROFILE_H

// Qt
#include <QColor>
#include <QFont>
#include <QHash>
#include <QKeySequence>
#include <QStringList>
#include <QVariant>

#include <vector>

// Konsole
#include "konsoleprofile_export.h"

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
class KONSOLEPROFILE_EXPORT Profile : public QSharedData
{
    Q_GADGET

    friend class ProfileReader;
    friend class ProfileWriter;
    friend class ProfileGroup;

public:
    using Ptr = QExplicitlySharedDataPointer<Profile>;
    using GroupPtr = QExplicitlySharedDataPointer<ProfileGroup>;

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
        /** (bool) If the background color should change to indicate if the window is active
         */
        DimWhenInactive,
        /**
         * (bool) Whether to always invert the colors for text selection.
         */
        InvertSelectionColors,
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
        /** (bool) Specifies whether the lines that are scrolled into view
         * should be highlighted.
         */
        HighlightScrolledLines,
        /** (bool) Specifies whether the terminal will enable Reflow of
         * lines when terminal resizes.
         */
        ReflowLines,
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
        /** (QColor) The color used by terminal displays to draw the character
         * underneath the cursor. Only applicable if the UseCustomCursorColor
         * property is true and CursorShape property is Enum::BlockCursor.
         */
        CustomCursorTextColor,
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
        /** (bool) If true, text that matches a file is
         * underlined when hovered by the mouse pointer.
         */
        UnderlineFilesEnabled,

        /**
         * (Enum::TextEditorCmd) Text editor command used to open local
         * text file URLs at a given line/column. There is a default list
         * of editors that the user can select from, and a CustomTextEditor
         * option if the user wants to use a different editor.
         *
         * See Enum::TextEditorCmd
         */
        TextEditorCmd,
        /**
         * (QString) This is the command string corresponding to Enum::CustomTextEditor.
         *
         * See TextEditorCmd and Enum::TextEditorCmd
         */
        TextEditorCmdCustom,

        /** (bool) If true, links can be opened by direct mouse click.*/
        OpenLinksByDirectClickEnabled,
        /** (bool) If true, control key must be pressed to click and drag selected text. */
        CtrlRequiredForDrag,
        /** (bool) If true, automatically copy selected text into the clipboard */
        AutoCopySelectedText,
        /** (bool) The QMimeData object used when copying text always
         * has the plain/text MIME set and if this is @c true then the
         * text/html MIME is set too in that object i.e. the copied
         * text will include formatting, font faces, colors... etc; users
         * can paste the text as HTML (default) or as plain/text by using
         * e.g. the "Paste Special" functionality in LibreOffice.
         */
        CopyTextAsHTML,
        /** (bool) If true, leading spaces are trimmed in selected text */
        TrimLeadingSpacesInSelectedText,
        /** (bool) If true, trailing spaces are trimmed in selected text */
        TrimTrailingSpacesInSelectedText,
        /** (bool) If true, then dropped URLs will be pasted as text without asking */
        DropUrlsAsText,
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
        /** (bool) Whether to use font's line characters instead of the
         * builtin code. */
        UseFontLineCharacters,
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
        /** (int) Margin width in pixels */
        TerminalMargin,
        /** (bool) Center terminal when there is a margin */
        TerminalCenter,
        /** (bool) If true, mouse wheel scroll with Ctrl key pressed
         * increases/decreases the terminal font size.
         */
        MouseWheelZoomEnabled,
        /** (bool) Specifies whether emulated up/down key press events are
         * sent, for mouse scroll wheel events, to programs using the
         * Alternate Screen buffer; this is mainly for the benefit of
         * programs that don't natively support mouse scroll events, e.g.
         * less.
         *
         * This also works for scrolling in applications that support Mouse
         * Tracking events but don't indicate they're interested in those
         * events; for example, when vim doesn't indicate it's interested
         * in Mouse Tracking events (i.e. when the mouse is in Normal
         * (not Visual) mode): https://vimhelp.org/intro.txt.html#vim-modes-intro
         * mouse wheel scroll events will send up/down key press events.
         *
         * Default value is true.
         * See also, MODE_Mouse1007 in the Emulation header, which toggles
         * Alternate Scrolling with escape sequences.
         */
        AlternateScrolling,
        /** (int) Keyboard modifiers to show URL hints */
        UrlHintsModifiers,
        /** (bool) Reverse the order of URL hints */
        ReverseUrlHints,
        /** (QColor) used in tab color */
        TabColor,
        /** (int) Value of the Dimm Effect */
        DimValue,
        /** (bool) Allow Escape sequence for Links.
         * this allows applications to use links in the form of
         * printf '\e]8;;https://www.example.org\e\\example text\e]8;;\e\\\n'
         */
        AllowEscapedLinks,
        /** (String) Escape Sequence for Links schema,
         * This tell us what kind of links are accepted, for security reason we can't accept
         * some weird ones like git:// and ssh:// but if the user wants he can enable.
         */
        EscapedLinksSchema,
        /** Use Vertical Line At */
        VerticalLine,
        /** Vertical Line Pixel at */
        VerticalLineAtChar,

        /** Shortcut for peeking primary screen */
        PeekPrimaryKeySequence,

        /** (bool) If true, text that matches a color in hex format
         *  when hovered by the mouse pointer.
         */
        ColorFilterEnabled,

        /** (bool) Allows mouse tracking */
        AllowMouseTracking,

        /** (int) Show semantic hints (lines between commands, lighter input):
         * 0 for Never, 1 when showing URL hints, 2 for always
         */
        SemanticHints,
        /** (bool) If true, convert Up/Down arrows when in input mode to Left/Right
         * key presses that emulate the same cursor movement
         */
        SemanticUpDown,
        /** (bool) If true, move cursor with Left/Right keys when mouse clicks in input area
         */
        SemanticInputClick,
    };

    Q_ENUM(Property)

    /**
     * Constructs a new profile
     *
     * @param parent The parent profile.  When querying the value of a
     * property using property(), if the property has not been set in this
     * profile then the parent's value for the property will be returned.
     */
    explicit Profile(const Ptr &parent = Ptr());
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
     * A profile which contains a number of default settings for various
     * properties.  This can be used as a parent for other profiles or as a
     * fallback in case a profile cannot be loaded from disk.
     */
    void useBuiltin();

    /**
     * Changes the parent profile.  When calling the property() method,
     * if the specified property has not been set for this profile,
     * the parent's value for the property will be returned instead.
     */
    void setParent(const Ptr &parent);

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
    template<class T>
    T property(Property p) const;

    /** Sets the value of the specified @p property to @p value. */
    virtual void setProperty(Property p, const QVariant &value);
    /** Returns true if the specified property has been set in this Profile
     * instance.
     */
    virtual bool isPropertySet(Property p) const;

    /** Returns a map of the properties set in this Profile instance. */
    virtual QHash<Property, QVariant> setProperties() const;

    /** Returns true if no properties have been set in this Profile instance. */
    bool isEmpty() const;

    /**
     * Returns true if this profile is the built-in profile.
     */
    bool isBuiltin() const;

    /**
     * Returns true if this is a 'hidden' profile which should not be
     * displayed in menus or saved to disk.
     *
     * This is true for the built-in profile, in case there are no profiles on
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
    QString path() const
    {
        return property<QString>(Profile::Path);
    }

    /** Convenience method for property<QString>(Profile::Name) */
    QString name() const
    {
        return property<QString>(Profile::Name);
    }

    /** Convenience method for property<QString>(Profile::UntranslatedName) */
    QString untranslatedName() const
    {
        return property<QString>(Profile::UntranslatedName);
    }

    /** Convenience method for property<QString>(Profile::Directory) */
    QString defaultWorkingDirectory() const
    {
        return property<QString>(Profile::Directory);
    }

    /** Convenience method for property<QString>(Profile::Icon) */
    QString icon() const
    {
        return property<QString>(Profile::Icon);
    }

    /** Convenience method for property<QString>(Profile::Command) */
    QString command() const
    {
        return property<QString>(Profile::Command);
    }

    /** Convenience method for property<QStringList>(Profile::Arguments) */
    QStringList arguments() const
    {
        return property<QStringList>(Profile::Arguments);
    }

    /** Convenience method for property<QString>(Profile::LocalTabTitleFormat) */
    QString localTabTitleFormat() const
    {
        return property<QString>(Profile::LocalTabTitleFormat);
    }

    /** Convenience method for property<QString>(Profile::RemoteTabTitleFormat) */
    QString remoteTabTitleFormat() const
    {
        return property<QString>(Profile::RemoteTabTitleFormat);
    }

    /** Convenience method for property<QColor>(Profile::TabColor) */
    QColor tabColor() const
    {
        return property<QColor>(Profile::TabColor);
    }

    bool allowEscapedLinks() const
    {
        return property<bool>(Profile::AllowEscapedLinks);
    }

    QStringList escapedLinksSchema() const
    {
        return property<QString>(Profile::EscapedLinksSchema).split(QLatin1Char(';'));
    }

    /** Convenience method for property<bool>(Profile::ShowTerminalSizeHint) */
    bool showTerminalSizeHint() const
    {
        return property<bool>(Profile::ShowTerminalSizeHint);
    }

    /** Convenience method for property<bool>(Profile::DimWhenInactive) */
    bool dimWhenInactive() const
    {
        return property<bool>(Profile::DimWhenInactive);
    }

    int dimValue() const
    {
        return property<int>(Profile::DimValue);
    }

    /** Convenience method for property<QFont>(Profile::Font) */
    QFont font() const
    {
        return property<QFont>(Profile::Font);
    }

    /** Convenience method for property<QString>(Profile::ColorScheme) */
    QString colorScheme() const
    {
        return property<QString>(Profile::ColorScheme);
    }

    /** Convenience method for property<QStringList>(Profile::Environment) */
    QStringList environment() const
    {
        return property<QStringList>(Profile::Environment);
    }

    /** Convenience method for property<QString>(Profile::KeyBindings) */
    QString keyBindings() const
    {
        return property<QString>(Profile::KeyBindings);
    }

    /** Convenience method for property<QString>(Profile::HistorySize) */
    int historySize() const
    {
        return property<int>(Profile::HistorySize);
    }

    /** Convenience method for property<bool>(Profile::BidiRenderingEnabled) */
    bool bidiRenderingEnabled() const
    {
        return property<bool>(Profile::BidiRenderingEnabled);
    }

    /** Convenience method for property<bool>(Profile::LineSpacing) */
    int lineSpacing() const
    {
        return property<int>(Profile::LineSpacing);
    }

    /** Convenience method for property<bool>(Profile::BlinkingTextEnabled) */
    bool blinkingTextEnabled() const
    {
        return property<bool>(Profile::BlinkingTextEnabled);
    }

    /** Convenience method for property<bool>(Profile::MouseWheelZoomEnabled) */
    bool mouseWheelZoomEnabled() const
    {
        return property<bool>(Profile::MouseWheelZoomEnabled);
    }

    /** Convenience method for property<bool>(Profile::BlinkingCursorEnabled) */
    bool blinkingCursorEnabled() const
    {
        return property<bool>(Profile::BlinkingCursorEnabled);
    }

    /** Convenience method for property<bool>(Profile::FlowControlEnabled) */
    bool flowControlEnabled() const
    {
        return property<bool>(Profile::FlowControlEnabled);
    }

    /** Convenience method for property<bool>(Profile::UseCustomCursorColor) */
    bool useCustomCursorColor() const
    {
        return property<bool>(Profile::UseCustomCursorColor);
    }

    /** Convenience method for property<QColor>(Profile::CustomCursorColor) */
    QColor customCursorColor() const
    {
        return property<QColor>(Profile::CustomCursorColor);
    }

    /** Convenience method for property<QColor>(Profile::CustomCursorTextColor) */
    QColor customCursorTextColor() const
    {
        return property<QColor>(Profile::CustomCursorTextColor);
    }

    /** Convenience method for property<QString>(Profile::WordCharacters) */
    QString wordCharacters() const
    {
        return property<QString>(Profile::WordCharacters);
    }

    /** Convenience method for property<bool>(Profile::UnderlineLinksEnabled) */
    bool underlineLinksEnabled() const
    {
        return property<bool>(Profile::UnderlineLinksEnabled);
    }

    /** Convenience method for property<bool>(Profile::UnderlineFilesEnabled) */
    bool underlineFilesEnabled() const
    {
        return property<bool>(Profile::UnderlineFilesEnabled);
    }

    /**
     * Returns the command line that will be used with the current text
     * editor setting.
     */
    QString textEditorCmd() const;
    /**
     * Convenience method to get the text editor command corresponding to
     * Enum::TextEditorCmdCustom.
     */
    QString customTextEditorCmd() const
    {
        return property<QString>(Profile::TextEditorCmdCustom);
    }

    bool autoCopySelectedText() const
    {
        return property<bool>(Profile::AutoCopySelectedText);
    }

    /** Convenience method for property<QString>(Profile::DefaultEncoding) */
    QString defaultEncoding() const
    {
        return property<QString>(Profile::DefaultEncoding);
    }

    /** Convenience method for property<bool>(Profile::AntiAliasFonts) */
    bool antiAliasFonts() const
    {
        return property<bool>(Profile::AntiAliasFonts);
    }

    /** Convenience method for property<bool>(Profile::BoldIntense) */
    bool boldIntense() const
    {
        return property<bool>(Profile::BoldIntense);
    }

    /** Convenience method for property<bool>(Profile::UseFontLineCharacters)*/
    bool useFontLineCharacters() const
    {
        return property<bool>(Profile::UseFontLineCharacters);
    }

    /** Convenience method for property<bool>(Profile::StartInCurrentSessionDir) */
    bool startInCurrentSessionDir() const
    {
        return property<bool>(Profile::StartInCurrentSessionDir);
    }

    /** Convenience method for property<QString>(Profile::SilenceSeconds) */
    int silenceSeconds() const
    {
        return property<int>(Profile::SilenceSeconds);
    }

    /** Convenience method for property<QString>(Profile::TerminalColumns) */
    int terminalColumns() const
    {
        return property<int>(Profile::TerminalColumns);
    }

    /** Convenience method for property<QString>(Profile::TerminalRows) */
    int terminalRows() const
    {
        return property<int>(Profile::TerminalRows);
    }

    /** Convenience method for property<int>(Profile::TerminalMargin) */
    int terminalMargin() const
    {
        return property<int>(Profile::TerminalMargin);
    }

    /** Convenience method for property<bool>(Profile::TerminalCenter) */
    bool terminalCenter() const
    {
        return property<bool>(Profile::TerminalCenter);
    }

    /** Convenience method for property<QString>(Profile::MenuIndex) */
    QString menuIndex() const
    {
        return property<QString>(Profile::MenuIndex);
    }

    bool verticalLine() const
    {
        return property<bool>(Profile::VerticalLine);
    }

    int verticalLineAtChar() const
    {
        return property<int>(Profile::VerticalLineAtChar);
    }

    /** Convenience method for property<bool>(Profile::colorFilterEnabled) */
    bool colorFilterEnabled() const
    {
        return property<bool>(Profile::ColorFilterEnabled);
    }

    QKeySequence peekPrimaryKeySequence() const
    {
        return QKeySequence::fromString(property<QString>(Profile::PeekPrimaryKeySequence));
    }

    int semanticHints() const
    {
        return property<int>(Profile::SemanticHints);
    }

    bool semanticUpDown() const
    {
        return property<bool>(Profile::SemanticUpDown);
    }

    bool semanticInputClick() const
    {
        return property<bool>(Profile::SemanticInputClick);
    }

    /** Return a list of all properties names and their type
     *  (for use with -p option).
     */
    static const std::vector<std::string> &propertiesInfoList();

    /**
     * Returns the element from the Property enum associated with the
     * specified @p name.
     *
     * @param name The name of the property to look for, this is case
     * insensitive.
     */
    static Property lookupByName(const QString &name);

private:
    struct PropertyInfo;
    // Defines a new property, this property is then available
    // to all Profile instances.
    static void registerProperty(const PropertyInfo &info);

    // fills the table with default names for profile properties
    // the first time it is called.
    // subsequent calls return immediately
    static void fillTableWithDefaultNames();

    // returns true if the property can be inherited
    static bool canInheritProperty(Property p);

    QHash<Property, QVariant> _propertyValues;
    Ptr _parent;

    bool _hidden;

    static QHash<QString, PropertyInfo> PropertyInfoByName;

    // Describes a property.  Each property has a name and group
    // which is used when saving/loading the profile.
    struct PropertyInfo {
        Property property;
        const char *name;
        const char *group;
        QVariant defaultValue;
    };
    static const std::vector<PropertyInfo> DefaultProperties;
};

inline bool Profile::canInheritProperty(Property p)
{
    return p != Name && p != Path;
}

template<class T>
inline T Profile::property(Property p) const
{
    return property<QVariant>(p).value<T>();
}

template<>
inline QVariant Profile::property(Property p) const
{
    if (_propertyValues.contains(p)) {
        return _propertyValues[p];
    } else if (_parent && canInheritProperty(p)) {
        return _parent->property<QVariant>(p);
    } else {
        return QVariant();
    }
}

}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
Q_DECLARE_METATYPE(Konsole::Profile::Ptr)
#endif

#endif // PROFILE_H

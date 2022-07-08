/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "Profile.h"

// Qt
#include <QRegularExpression>
#include <QTextCodec>

// KDE
#include <KLocalizedString>
#include <QFontDatabase>
#include <kcoreaddons_version.h>

// Konsole
#include "Enumeration.h"
#include "ProfileGroup.h"
#include "config-konsole.h"

#ifdef HAVE_GETPWUID
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if KCOREADDONS_VERSION >= QT_VERSION_CHECK(5, 97, 0)
#include <KSandbox>
#endif

using namespace Konsole;

// mappings between property enum values and names
//
// multiple names are defined for some property values,
// in these cases, the "proper" string name comes first,
// as that is used when reading/writing profiles from/to disk
//
// the other names are usually shorter versions for convenience
// when parsing konsoleprofile commands
static const char GENERAL_GROUP[] = "General";
static const char KEYBOARD_GROUP[] = "Keyboard";
static const char APPEARANCE_GROUP[] = "Appearance";
static const char SCROLLING_GROUP[] = "Scrolling";
static const char TERMINAL_GROUP[] = "Terminal Features";
static const char CURSOR_GROUP[] = "Cursor Options";
static const char INTERACTION_GROUP[] = "Interaction Options";
static const char ENCODING_GROUP[] = "Encoding Options";

const std::vector<Profile::PropertyInfo> Profile::DefaultProperties = {
    // General
    {Path, "Path", nullptr, QString()},
    {Name, "Name", GENERAL_GROUP, QString()},
    {UntranslatedName, "UntranslatedName", nullptr, QString()},
    {Icon, "Icon", GENERAL_GROUP, QLatin1String("utilities-terminal")},
    {Command, "Command", nullptr, QString()},
    {Arguments, "Arguments", nullptr, QStringList()},
    {MenuIndex, "MenuIndex", nullptr, QLatin1String("0")},
    {Environment, "Environment", GENERAL_GROUP, QStringList{QLatin1String("TERM=xterm-256color"), QLatin1String("COLORTERM=truecolor")}},
    {Directory, "Directory", GENERAL_GROUP, QString()},
    {LocalTabTitleFormat, "LocalTabTitleFormat", GENERAL_GROUP, QLatin1String("%d : %n")},
    {LocalTabTitleFormat, "tabtitle", nullptr, QLatin1String("%d : %n")},
    {RemoteTabTitleFormat, "RemoteTabTitleFormat", GENERAL_GROUP, QLatin1String("(%u) %H")},
    {SemanticHints, "SemanticHints", GENERAL_GROUP, 1},
    {SemanticUpDown, "SemanticUpDown", GENERAL_GROUP, false},
    {SemanticInputClick, "SemanticInputClick", GENERAL_GROUP, false},
    {ShowTerminalSizeHint, "ShowTerminalSizeHint", GENERAL_GROUP, true},
    {StartInCurrentSessionDir, "StartInCurrentSessionDir", GENERAL_GROUP, true},
    {SilenceSeconds, "SilenceSeconds", GENERAL_GROUP, 10},
    {TerminalColumns, "TerminalColumns", GENERAL_GROUP, 110},
    {TerminalRows, "TerminalRows", GENERAL_GROUP, 28},
    {TerminalMargin, "TerminalMargin", GENERAL_GROUP, 1},
    {TerminalCenter, "TerminalCenter", GENERAL_GROUP, false},

    // Appearance
    {Font, "Font", APPEARANCE_GROUP, QFont()},
    {ColorScheme, "ColorScheme", APPEARANCE_GROUP, QLatin1String("Breeze")},
    {ColorScheme, "colors", nullptr, QLatin1String("Breeze")},
    {AntiAliasFonts, "AntiAliasFonts", APPEARANCE_GROUP, true},
    {BoldIntense, "BoldIntense", APPEARANCE_GROUP, true},
    {UseFontLineCharacters, "UseFontLineChararacters", APPEARANCE_GROUP, false},
    {LineSpacing, "LineSpacing", APPEARANCE_GROUP, 0},
    {TabColor, "TabColor", APPEARANCE_GROUP, QColor(QColor::Invalid)},
    {DimValue, "DimmValue", APPEARANCE_GROUP, 128},
    {DimWhenInactive, "DimWhenInactive", GENERAL_GROUP, false},
    {InvertSelectionColors, "InvertSelectionColors", GENERAL_GROUP, false},

// Keyboard
#ifdef Q_OS_MACOS
    {KeyBindings, "KeyBindings", KEYBOARD_GROUP, QLatin1String("macos")},
#else
    {KeyBindings, "KeyBindings", KEYBOARD_GROUP, QLatin1String("default")},
#endif

    // Scrolling
    {HistoryMode, "HistoryMode", SCROLLING_GROUP, Enum::FixedSizeHistory},
    {HistorySize, "HistorySize", SCROLLING_GROUP, 1000},
    {ScrollBarPosition, "ScrollBarPosition", SCROLLING_GROUP, Enum::ScrollBarRight},
    {ScrollFullPage, "ScrollFullPage", SCROLLING_GROUP, false},
    {HighlightScrolledLines, "HighlightScrolledLines", SCROLLING_GROUP, true},
    {ReflowLines, "ReflowLines", SCROLLING_GROUP, true},

    // Terminal Features
    {UrlHintsModifiers, "UrlHintsModifiers", TERMINAL_GROUP, 0},
    {ReverseUrlHints, "ReverseUrlHints", TERMINAL_GROUP, false},
    {BlinkingTextEnabled, "BlinkingTextEnabled", TERMINAL_GROUP, true},
    {FlowControlEnabled, "FlowControlEnabled", TERMINAL_GROUP, true},
    {BidiRenderingEnabled, "BidiRenderingEnabled", TERMINAL_GROUP, true},
    {BlinkingCursorEnabled, "BlinkingCursorEnabled", TERMINAL_GROUP, false},
    {BellMode, "BellMode", TERMINAL_GROUP, Enum::NotifyBell},
    {VerticalLine, "VerticalLine", TERMINAL_GROUP, false},
    {VerticalLineAtChar, "VerticalLineAtChar", TERMINAL_GROUP, 80},
    {PeekPrimaryKeySequence, "PeekPrimaryKeySequence", TERMINAL_GROUP, QString()},

    // Cursor
    {UseCustomCursorColor, "UseCustomCursorColor", CURSOR_GROUP, false},
    {CursorShape, "CursorShape", CURSOR_GROUP, Enum::BlockCursor},
    {CustomCursorColor, "CustomCursorColor", CURSOR_GROUP, QColor(Qt::white)},
    {CustomCursorTextColor, "CustomCursorTextColor", CURSOR_GROUP, QColor(Qt::black)},

    // Interaction
    {WordCharacters, "WordCharacters", INTERACTION_GROUP, QLatin1String(":@-./_~?&=%+#")},
    {TripleClickMode, "TripleClickMode", INTERACTION_GROUP, Enum::SelectWholeLine},
    {UnderlineLinksEnabled, "UnderlineLinksEnabled", INTERACTION_GROUP, true},
    {UnderlineFilesEnabled, "UnderlineFilesEnabled", INTERACTION_GROUP, false},
    {OpenLinksByDirectClickEnabled, "OpenLinksByDirectClickEnabled", INTERACTION_GROUP, false},
    {TextEditorCmd, "TextEditorCmd", INTERACTION_GROUP, Enum::Kate},
    {TextEditorCmdCustom, "TextEditorCmdCustom", INTERACTION_GROUP, QLatin1String("kate PATH:LINE:COLUMN")},
    {CtrlRequiredForDrag, "CtrlRequiredForDrag", INTERACTION_GROUP, true},
    {DropUrlsAsText, "DropUrlsAsText", INTERACTION_GROUP, true},
    {AutoCopySelectedText, "AutoCopySelectedText", INTERACTION_GROUP, false},
    {CopyTextAsHTML, "CopyTextAsHTML", INTERACTION_GROUP, true},
    {TrimLeadingSpacesInSelectedText, "TrimLeadingSpacesInSelectedText", INTERACTION_GROUP, false},
    {TrimTrailingSpacesInSelectedText, "TrimTrailingSpacesInSelectedText", INTERACTION_GROUP, false},
    {PasteFromSelectionEnabled, "PasteFromSelectionEnabled", INTERACTION_GROUP, true},
    {PasteFromClipboardEnabled, "PasteFromClipboardEnabled", INTERACTION_GROUP, false},
    {MiddleClickPasteMode, "MiddleClickPasteMode", INTERACTION_GROUP, Enum::PasteFromX11Selection},
    {MouseWheelZoomEnabled, "MouseWheelZoomEnabled", INTERACTION_GROUP, true},
    {AllowMouseTracking, "AllowMouseTracking", INTERACTION_GROUP, true},
    {AlternateScrolling, "AlternateScrolling", INTERACTION_GROUP, true},
    {AllowEscapedLinks, "AllowEscapedLinks", INTERACTION_GROUP, false},
    {EscapedLinksSchema, "EscapedLinksSchema", INTERACTION_GROUP, QLatin1String("http://;https://;file://")},
    {ColorFilterEnabled, "ColorFilterEnabled", INTERACTION_GROUP, true},

    // Encoding
    {DefaultEncoding, "DefaultEncoding", ENCODING_GROUP, QString()},
};

QHash<QString, Profile::PropertyInfo> Profile::PropertyInfoByName;

// Magic path for the built-in profile which is not a valid file name,
// thus it can not interfere with regular profiles.
// For backward compatibility with existing profiles, it should never change.
static const QString BUILTIN_MAGIC_PATH = QStringLiteral("FALLBACK/");

// UntranslatedName property of the built-in profile, as a char array.
//
// Note: regular profiles created in earlier versions of Konsole may have this name too.
static const char BUILTIN_UNTRANSLATED_NAME_CHAR[] = "Built-in";

static QString defaultShell()
{
#if KCOREADDONS_VERSION >= QT_VERSION_CHECK(5, 97, 0)
    if (!KSandbox::isFlatpak()) {
        return QString::fromUtf8(qgetenv("SHELL"));
    }

#ifdef HAVE_GETPWUID
    auto pw = getpwuid(getuid());
    // pw: Do not pass the returned pointer to free.
    if (pw != nullptr) {
        QProcess proc;
        proc.setProgram(QStringLiteral("getent"));
        proc.setArguments({QStringLiteral("passwd"), QString::number(pw->pw_uid)});
        KSandbox::startHostProcess(proc);
        proc.waitForFinished();
        return QString::fromUtf8(proc.readAllStandardOutput().simplified().split(':').at(6));
    }
    return {};
#endif
#else
    return QString::fromUtf8(qgetenv("SHELL"));
#endif
}

void Profile::fillTableWithDefaultNames()
{
    static bool filledDefaults = false;

    if (filledDefaults) {
        return;
    }

    std::for_each(DefaultProperties.cbegin(), DefaultProperties.cend(), Profile::registerProperty);

    filledDefaults = true;
}

void Profile::useBuiltin()
{
    for (const PropertyInfo &propInfo : DefaultProperties) {
        setProperty(propInfo.property, propInfo.defaultValue);
    }
    setProperty(Name, i18nc("Name of the built-in profile", BUILTIN_UNTRANSLATED_NAME_CHAR));
    setProperty(UntranslatedName, QString::fromLatin1(BUILTIN_UNTRANSLATED_NAME_CHAR));
    setProperty(Path, BUILTIN_MAGIC_PATH);
    setProperty(Command, defaultShell());
    // See Pty.cpp on why Arguments is populated
    setProperty(Arguments, QStringList{defaultShell()});
    setProperty(Font, QFontDatabase::systemFont(QFontDatabase::FixedFont));
    setProperty(DefaultEncoding, QLatin1String(QTextCodec::codecForLocale()->name()));
    // Built-in profile should not be shown in menus
    setHidden(true);
}

Profile::Profile(const Profile::Ptr &parent)
    : _propertyValues(QHash<Property, QVariant>())
    , _parent(parent)
    , _hidden(false)
{
}

void Profile::clone(Profile::Ptr profile, bool differentOnly)
{
    for (const PropertyInfo &info : DefaultProperties) {
        Property current = info.property;
        QVariant otherValue = profile->property<QVariant>(current);
        if (current == Name || current == Path) { // These are unique per Profile
            continue;
        }
        if (!differentOnly || property<QVariant>(current) != otherValue) {
            setProperty(current, otherValue);
        }
    }
}

Profile::~Profile() = default;

bool Profile::isBuiltin() const
{
    return path() == BUILTIN_MAGIC_PATH;
}

bool Profile::isHidden() const
{
    return _hidden;
}
void Profile::setHidden(bool hidden)
{
    _hidden = hidden;
}

void Profile::setParent(const Profile::Ptr &parent)
{
    _parent = parent;
}
const Profile::Ptr Profile::parent() const
{
    return _parent;
}

bool Profile::isEmpty() const
{
    return _propertyValues.isEmpty();
}

QHash<Profile::Property, QVariant> Profile::setProperties() const
{
    return _propertyValues;
}

void Profile::setProperty(Property p, const QVariant &value)
{
    _propertyValues.insert(p, value);
}

bool Profile::isPropertySet(Property p) const
{
    return _propertyValues.contains(p);
}

Profile::Property Profile::lookupByName(const QString &name)
{
    // insert default names into table the first time this is called
    fillTableWithDefaultNames();

    return PropertyInfoByName[name.toLower()].property;
}

void Profile::registerProperty(const PropertyInfo &info)
{
    QString name = QLatin1String(info.name);
    PropertyInfoByName.insert(name.toLower(), info);
}

// static
const std::vector<std::string> &Profile::propertiesInfoList()
{
    static std::vector<std::string> list;
    if (!list.empty()) {
        return list;
    }

    list.reserve(DefaultProperties.size());
    for (const PropertyInfo &info : DefaultProperties) {
        list.push_back(std::string(info.name) + " : " + info.defaultValue.typeName());
    }
    return list;
}

const Profile::GroupPtr Profile::asGroup() const
{
    const Profile::GroupPtr ptr(dynamic_cast<ProfileGroup *>(const_cast<Profile *>(this)));
    return ptr;
}

Profile::GroupPtr Profile::asGroup()
{
    return Profile::GroupPtr(dynamic_cast<ProfileGroup *>(this));
}

QString Profile::textEditorCmd() const
{
    auto current = property<int>(Profile::TextEditorCmd);

    QString editorCmd;
    switch (current) {
    case Enum::Kate:
        editorCmd = QStringLiteral("kate PATH:LINE:COLUMN");
        break;
    case Enum::KWrite:
        editorCmd = QStringLiteral("kwrite PATH:LINE:COLUMN");
        break;
    case Enum::KDevelop:
        editorCmd = QStringLiteral("kdevelop PATH:LINE:COLUMN");
        break;
    case Enum::QtCreator:
        editorCmd = QStringLiteral("qtcreator PATH:LINE:COLUMN");
        break;
    case Enum::Gedit:
        editorCmd = QStringLiteral("gedit +LINE:COLUMN PATH");
        break;
    case Enum::gVim:
        editorCmd = QStringLiteral("gvim +LINE PATH");
        break;
    case Enum::CustomTextEditor:
        editorCmd = customTextEditorCmd();
        break;
    default:
        break;
    }

    return editorCmd;
}

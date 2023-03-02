/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "Profile.h"

// Qt
#include <QDir>
#include <QFileInfo>
#include <QTextCodec>

// KDE
#include <KLocalizedString>
#include <QFontDatabase>
#include <kcoreaddons_version.h>

// Konsole
#include "Enumeration.h"
#include "ProfileGroup.h"
#include "config-konsole.h"

#ifndef Q_OS_WIN
#ifdef HAVE_GETPWUID
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif // HAVE_GETPWUID
#endif // Q_OS_WIN

#if KCOREADDONS_VERSION >= QT_VERSION_CHECK(5, 97, 0)
#include <KSandbox>
#endif

using namespace Konsole;

#if defined(Q_OS_WIN)
#define DEFAULT_ENCODING QStringLiteral("utf8")
#else
#define DEFAULT_ENCODING QString()
#endif

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
    {ErrorBars, "ErrorBars", GENERAL_GROUP, 2},
    {ErrorBackground, "ErrorBackground", GENERAL_GROUP, 1},
    {AlternatingBars, "AlternatingBars", GENERAL_GROUP, 2},
    {AlternatingBackground, "AlternatingBackground", GENERAL_GROUP, 1},

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
    {EmojiFont, "EmojiFont", APPEARANCE_GROUP, QFont()},
    {WordMode, "WordMode", APPEARANCE_GROUP, true},
    {WordModeAttr, "WordModeAttr", APPEARANCE_GROUP, false},
    {WordModeAscii, "WordModeAscii", APPEARANCE_GROUP, true},
    {WordModeBrahmic, "WordModeBrahmic", APPEARANCE_GROUP, false},
    {IgnoreWcWidth, "IgnoreWcWidth", APPEARANCE_GROUP, false},

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
    {BidiLineLTR, "BidiLineLTR", TERMINAL_GROUP, true},
    {BidiTableDirOverride, "BidiTableDirOverride", TERMINAL_GROUP, true},
    {BlinkingCursorEnabled, "BlinkingCursorEnabled", TERMINAL_GROUP, false},
    {BellMode, "BellMode", TERMINAL_GROUP, Enum::NotifyBell},
    {VerticalLine, "VerticalLine", TERMINAL_GROUP, false},
    {VerticalLineAtChar, "VerticalLineAtChar", TERMINAL_GROUP, 80},
    {PeekPrimaryKeySequence, "PeekPrimaryKeySequence", TERMINAL_GROUP, QString()},
    {LineNumbers, "LineNumbers", TERMINAL_GROUP, 0},

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
    {DefaultEncoding, "DefaultEncoding", ENCODING_GROUP, DEFAULT_ENCODING},
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

#ifdef Q_OS_WIN
static QString checkFile(const QStringList &dirList, const QString &filePath)
{
    for (const QString &root : dirList) {
        QFileInfo info(root, filePath);
        if (info.exists()) {
            return QDir::toNativeSeparators(info.filePath());
        }
    }
    return QString();
}

static QString GetWindowPowerShell()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QStringList dirList;
    dirList << env.value(QStringLiteral("windir"), QStringLiteral("C:\\Windows"));
    return checkFile(dirList, QStringLiteral("System32\\WindowsPowerShell\\v1.0\\powershell.exe"));
}

static QString GetWindowsShell()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString windir = env.value(QStringLiteral("windir"), QStringLiteral("C:\\Windows"));
    QFileInfo info(windir, QStringLiteral("System32\\cmd.exe"));
    return QDir::toNativeSeparators(info.filePath());
}
#endif

static QString defaultShell()
{
#ifndef Q_OS_WIN
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
        const auto output = proc.readAllStandardOutput().simplified();
        if (auto parts = QString::fromUtf8(output).split(QLatin1Char(':')); output.size() >= 7) {
            return parts.at(6);
        }
    }
    return {};
#endif
#else
    return QString::fromUtf8(qgetenv("SHELL"));
#endif

#else // Q_OS_WIN
    auto shell = GetWindowPowerShell();
    if (shell.isEmpty()) {
        shell = GetWindowsShell();
    }
    return shell;
#endif // Q_OS_WIN
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
#if defined(Q_OS_WIN)
    setProperty(DefaultEncoding, QLatin1String(QTextCodec::codecForName("utf8")->name()));
#else
    setProperty(DefaultEncoding, QLatin1String(QTextCodec::codecForLocale()->name()));
#endif
    // Built-in profile should not be shown in menus
    setHidden(true);
}

Profile::Profile(const Profile::Ptr &parent)
    : _propertyValues(PropertyMap())
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

bool Profile::isEditable() const
{
    // Read-only profiles (i.e. with non-user-writable .profile location)
    // aren't editable. This includes the built-in profile, which is hardcoded.
    return !isBuiltin() && QFileInfo(path()).isWritable();
}

bool Profile::isDeletable() const
{
    // To delete a file, parent dir must be writable
    const QFileInfo fileInfo(path());
    return !isBuiltin() && fileInfo.exists() && QFileInfo(fileInfo.path()).isWritable();
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
    return _propertyValues.empty();
}

const Profile::PropertyMap &Profile::properties() const
{
    return _propertyValues;
}

void Profile::setProperty(Property p, const QVariant &value)
{
    _propertyValues.insert_or_assign(p, value);
}

void Profile::setProperty(Property p, QVariant &&value)
{
    _propertyValues.insert_or_assign(p, value);
}

void Profile::assignProperties(const PropertyMap &map)
{
    for (const auto &[p, value] : map) {
        setProperty(p, value);
    }
}

void Profile::assignProperties(PropertyMap &&map)
{
    // If a key exists in both maps, we want to use the associated
    // value from 'map'
    map.merge(_propertyValues);
    _propertyValues.swap(map);
}

bool Profile::isPropertySet(Property p) const
{
    return _propertyValues.find(p) != _propertyValues.cend();
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

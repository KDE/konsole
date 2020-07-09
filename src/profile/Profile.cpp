/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

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

// Konsole
#include "Enumeration.h"
#include "ProfileGroup.h"

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

const Profile::PropertyInfo Profile::DefaultPropertyNames[] = {
    // General
    {Path, "Path", nullptr, QVariant::String},
    {Name, "Name", GENERAL_GROUP, QVariant::String},
    {UntranslatedName, "UntranslatedName", nullptr, QVariant::String},
    {Icon, "Icon", GENERAL_GROUP, QVariant::String},
    {Command, "Command", nullptr, QVariant::String},
    {Arguments, "Arguments", nullptr, QVariant::StringList},
    {MenuIndex, "MenuIndex", nullptr, QVariant::String},
    {Environment, "Environment", GENERAL_GROUP, QVariant::StringList},
    {Directory, "Directory", GENERAL_GROUP, QVariant::String},
    {LocalTabTitleFormat, "LocalTabTitleFormat", GENERAL_GROUP, QVariant::String},
    {LocalTabTitleFormat, "tabtitle", nullptr, QVariant::String},
    {RemoteTabTitleFormat, "RemoteTabTitleFormat", GENERAL_GROUP, QVariant::String},
    {ShowTerminalSizeHint, "ShowTerminalSizeHint", GENERAL_GROUP, QVariant::Bool},
    {StartInCurrentSessionDir, "StartInCurrentSessionDir", GENERAL_GROUP, QVariant::Bool},
    {SilenceSeconds, "SilenceSeconds", GENERAL_GROUP, QVariant::Int},
    {TerminalColumns, "TerminalColumns", GENERAL_GROUP, QVariant::Int},
    {TerminalRows, "TerminalRows", GENERAL_GROUP, QVariant::Int},
    {TerminalMargin, "TerminalMargin", GENERAL_GROUP, QVariant::Int},
    {TerminalCenter, "TerminalCenter", GENERAL_GROUP, QVariant::Bool}

    // Appearance
    ,
    {Font, "Font", APPEARANCE_GROUP, QVariant::Font},
    {ColorScheme, "ColorScheme", APPEARANCE_GROUP, QVariant::String},
    {ColorScheme, "colors", nullptr, QVariant::String},
    {AntiAliasFonts, "AntiAliasFonts", APPEARANCE_GROUP, QVariant::Bool},
    {BoldIntense, "BoldIntense", APPEARANCE_GROUP, QVariant::Bool},
    {UseFontLineCharacters, "UseFontLineChararacters", APPEARANCE_GROUP, QVariant::Bool},
    {LineSpacing, "LineSpacing", APPEARANCE_GROUP, QVariant::Int},
    {TabColor, "TabColor", APPEARANCE_GROUP, QVariant::Color},
    {DimValue, "DimmValue", APPEARANCE_GROUP, QVariant::Int},
    {DimWhenInactive, "DimWhenInactive", GENERAL_GROUP, QVariant::Bool},
    {InvertSelectionColors, "InvertSelectionColors", GENERAL_GROUP, QVariant::Bool}

    // Keyboard
    ,
    {KeyBindings, "KeyBindings", KEYBOARD_GROUP, QVariant::String}

    // Scrolling
    ,
    {HistoryMode, "HistoryMode", SCROLLING_GROUP, QVariant::Int},
    {HistorySize, "HistorySize", SCROLLING_GROUP, QVariant::Int},
    {ScrollBarPosition, "ScrollBarPosition", SCROLLING_GROUP, QVariant::Int},
    {ScrollFullPage, "ScrollFullPage", SCROLLING_GROUP, QVariant::Bool},
    {HighlightScrolledLines, "HighlightScrolledLines", SCROLLING_GROUP, QVariant::Bool},
    {ReflowLines, "ReflowLines", SCROLLING_GROUP, QVariant::Bool}

    // Terminal Features
    ,
    {UrlHintsModifiers, "UrlHintsModifiers", TERMINAL_GROUP, QVariant::Int},
    {ReverseUrlHints, "ReverseUrlHints", TERMINAL_GROUP, QVariant::Bool},
    {BlinkingTextEnabled, "BlinkingTextEnabled", TERMINAL_GROUP, QVariant::Bool},
    {FlowControlEnabled, "FlowControlEnabled", TERMINAL_GROUP, QVariant::Bool},
    {BidiRenderingEnabled, "BidiRenderingEnabled", TERMINAL_GROUP, QVariant::Bool},
    {BlinkingCursorEnabled, "BlinkingCursorEnabled", TERMINAL_GROUP, QVariant::Bool},
    {BellMode, "BellMode", TERMINAL_GROUP, QVariant::Int},
    {VerticalLine, "VerticalLine", TERMINAL_GROUP, QVariant::Bool},
    {VerticalLineAtChar, "VerticalLineAtChar", TERMINAL_GROUP, QVariant::Int},
    {PeekPrimaryKeySequence, "PeekPrimaryKeySequence", TERMINAL_GROUP, QVariant::String}
    // Cursor
    ,
    {UseCustomCursorColor, "UseCustomCursorColor", CURSOR_GROUP, QVariant::Bool},
    {CursorShape, "CursorShape", CURSOR_GROUP, QVariant::Int},
    {CustomCursorColor, "CustomCursorColor", CURSOR_GROUP, QVariant::Color},
    {CustomCursorTextColor, "CustomCursorTextColor", CURSOR_GROUP, QVariant::Color}

    // Interaction
    ,
    {WordCharacters, "WordCharacters", INTERACTION_GROUP, QVariant::String},
    {TripleClickMode, "TripleClickMode", INTERACTION_GROUP, QVariant::Int},
    {UnderlineLinksEnabled, "UnderlineLinksEnabled", INTERACTION_GROUP, QVariant::Bool},
    {UnderlineFilesEnabled, "UnderlineFilesEnabled", INTERACTION_GROUP, QVariant::Bool},
    {OpenLinksByDirectClickEnabled, "OpenLinksByDirectClickEnabled", INTERACTION_GROUP, QVariant::Bool},
    {TextEditorCmd, "TextEditorCmd", INTERACTION_GROUP, QVariant::Int},
    {TextEditorCmdCustom, "TextEditorCmdCustom", INTERACTION_GROUP, QVariant::String},
    {CtrlRequiredForDrag, "CtrlRequiredForDrag", INTERACTION_GROUP, QVariant::Bool},
    {DropUrlsAsText, "DropUrlsAsText", INTERACTION_GROUP, QVariant::Bool},
    {AutoCopySelectedText, "AutoCopySelectedText", INTERACTION_GROUP, QVariant::Bool},
    {CopyTextAsHTML, "CopyTextAsHTML", INTERACTION_GROUP, QVariant::Bool},
    {TrimLeadingSpacesInSelectedText, "TrimLeadingSpacesInSelectedText", INTERACTION_GROUP, QVariant::Bool},
    {TrimTrailingSpacesInSelectedText, "TrimTrailingSpacesInSelectedText", INTERACTION_GROUP, QVariant::Bool},
    {PasteFromSelectionEnabled, "PasteFromSelectionEnabled", INTERACTION_GROUP, QVariant::Bool},
    {PasteFromClipboardEnabled, "PasteFromClipboardEnabled", INTERACTION_GROUP, QVariant::Bool},
    {MiddleClickPasteMode, "MiddleClickPasteMode", INTERACTION_GROUP, QVariant::Int},
    {MouseWheelZoomEnabled, "MouseWheelZoomEnabled", INTERACTION_GROUP, QVariant::Bool},
    {AlternateScrolling, "AlternateScrolling", INTERACTION_GROUP, QVariant::Bool},
    {AllowEscapedLinks, "AllowEscapedLinks", INTERACTION_GROUP, QVariant::Bool},
    {EscapedLinksSchema, "EscapedLinksSchema", INTERACTION_GROUP, QVariant::String},
    {ColorFilterEnabled, "ColorFilterEnabled", INTERACTION_GROUP, QVariant::Bool},
    {AllowMouseTracking, "AllowMouseTracking", INTERACTION_GROUP, QVariant::Bool}

    // Encoding
    ,
    {DefaultEncoding, "DefaultEncoding", ENCODING_GROUP, QVariant::String}

    ,
    {static_cast<Profile::Property>(0), nullptr, nullptr, QVariant::Invalid}};

QHash<QString, Profile::PropertyInfo> Profile::PropertyInfoByName;
QHash<Profile::Property, Profile::PropertyInfo> Profile::PropertyInfoByProperty;

void Profile::fillTableWithDefaultNames()
{
    static bool filledDefaults = false;

    if (filledDefaults) {
        return;
    }

    const PropertyInfo *iter = DefaultPropertyNames;
    while (iter->name != nullptr) {
        registerProperty(*iter);
        iter++;
    }

    filledDefaults = true;
}

void Profile::useFallback()
{
    // Fallback settings
    setProperty(Name, i18nc("Name of the default/builtin profile", "Default"));
    setProperty(UntranslatedName, QStringLiteral("Default"));
    // magic path for the fallback profile which is not a valid
    // non-directory file name
    setProperty(Path, QStringLiteral("FALLBACK/"));
    setProperty(Command, QString::fromUtf8(qgetenv("SHELL")));
    // See Pty.cpp on why Arguments is populated
    setProperty(Arguments, QStringList() << QString::fromUtf8(qgetenv("SHELL")));
    setProperty(Icon, QStringLiteral("utilities-terminal"));
    setProperty(Environment, QStringList() << QStringLiteral("TERM=xterm-256color") << QStringLiteral("COLORTERM=truecolor"));
    setProperty(LocalTabTitleFormat, QStringLiteral("%d : %n"));
    setProperty(RemoteTabTitleFormat, QStringLiteral("(%u) %H"));
    setProperty(ShowTerminalSizeHint, true);
    setProperty(DimWhenInactive, false);
    setProperty(DimValue, 128);
    setProperty(InvertSelectionColors, false);
    setProperty(StartInCurrentSessionDir, true);
    setProperty(MenuIndex, QStringLiteral("0"));
    setProperty(SilenceSeconds, 10);
    setProperty(TerminalColumns, 110);
    setProperty(TerminalRows, 28);
    setProperty(TerminalMargin, 1);
    setProperty(TerminalCenter, false);
    setProperty(MouseWheelZoomEnabled, true);
    setProperty(AlternateScrolling, true);
    setProperty(AllowMouseTracking, true);

#ifdef Q_OS_MACOS
    setProperty(KeyBindings, QStringLiteral("macos"));
#else
    setProperty(KeyBindings, QStringLiteral("default"));
#endif
    setProperty(ColorScheme, QStringLiteral("Breeze"));
    setProperty(Font, QFontDatabase::systemFont(QFontDatabase::FixedFont));

    setProperty(HistoryMode, Enum::FixedSizeHistory);
    setProperty(HistorySize, 1000);
    setProperty(ScrollBarPosition, Enum::ScrollBarRight);
    setProperty(ScrollFullPage, false);
    setProperty(HighlightScrolledLines, true);
    setProperty(ReflowLines, true);

    setProperty(FlowControlEnabled, true);
    setProperty(UrlHintsModifiers, 0);
    setProperty(ReverseUrlHints, false);
    setProperty(BlinkingTextEnabled, true);
    setProperty(UnderlineLinksEnabled, true);
    setProperty(UnderlineFilesEnabled, false);
    setProperty(OpenLinksByDirectClickEnabled, false);

    setProperty(TextEditorCmd, Enum::Kate);
    setProperty(TextEditorCmdCustom, QStringLiteral("kate PATH:LINE:COLUMN"));

    setProperty(CtrlRequiredForDrag, true);
    setProperty(AutoCopySelectedText, false);
    setProperty(CopyTextAsHTML, true);
    setProperty(TrimLeadingSpacesInSelectedText, false);
    setProperty(TrimTrailingSpacesInSelectedText, false);
    setProperty(DropUrlsAsText, true);
    setProperty(PasteFromSelectionEnabled, true);
    setProperty(PasteFromClipboardEnabled, false);
    setProperty(MiddleClickPasteMode, Enum::PasteFromX11Selection);
    setProperty(TripleClickMode, Enum::SelectWholeLine);
    setProperty(ColorFilterEnabled, true);

    setProperty(BlinkingCursorEnabled, false);
    setProperty(BidiRenderingEnabled, true);
    setProperty(LineSpacing, 0);
    setProperty(CursorShape, Enum::BlockCursor);
    setProperty(UseCustomCursorColor, false);
    setProperty(CustomCursorColor, QColor(Qt::white));
    setProperty(CustomCursorTextColor, QColor(Qt::black));
    setProperty(BellMode, Enum::NotifyBell);

    setProperty(DefaultEncoding, QLatin1String(QTextCodec::codecForLocale()->name()));
    setProperty(AntiAliasFonts, true);
    setProperty(BoldIntense, true);
    setProperty(UseFontLineCharacters, false);

    setProperty(WordCharacters, QStringLiteral(":@-./_~?&=%+#"));

    setProperty(TabColor, QColor(QColor::Invalid));
    setProperty(AllowEscapedLinks, false);
    setProperty(EscapedLinksSchema, QStringLiteral("http://;https://;file://"));
    setProperty(VerticalLine, false);
    setProperty(VerticalLineAtChar, 80);
    setProperty(PeekPrimaryKeySequence, QString());
    // Fallback should not be shown in menus
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
    const PropertyInfo *properties = DefaultPropertyNames;
    while (properties->name != nullptr) {
        Property current = properties->property;
        QVariant otherValue = profile->property<QVariant>(current);
        switch (current) {
        case Name:
        case Path:
            break;
        default:
            if (!differentOnly || property<QVariant>(current) != otherValue) {
                setProperty(current, otherValue);
            }
        }
        properties++;
    }
}

Profile::~Profile() = default;

bool Profile::isFallback() const
{
    return path() == QLatin1String{"FALLBACK/"};
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

    // only allow one property -> name map
    // (multiple name -> property mappings are allowed though)
    if (!PropertyInfoByProperty.contains(info.property)) {
        PropertyInfoByProperty.insert(info.property, info);
    }
}

const QStringList Profile::propertiesInfoList() const
{
    QStringList info;
    const PropertyInfo *iter = DefaultPropertyNames;
    while (iter->name != nullptr) {
        info << QLatin1String(iter->name) + QStringLiteral(" : ") + QLatin1String(QVariant(iter->type).typeName());
        iter++;
    }

    return info;
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

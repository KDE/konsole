/*
    This source file is part of Konsole, a terminal emulator.

    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>

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

// Own
#include "Profile.h"

// Qt
#include <QTextCodec>
#include <QRegularExpression>

// KDE
#include <KLocalizedString>
#include <QFontDatabase>

// Konsole
#include "Enumeration.h"

using namespace Konsole;

// mappings between property enum values and names
//
// multiple names are defined for some property values,
// in these cases, the "proper" string name comes first,
// as that is used when reading/writing profiles from/to disk
//
// the other names are usually shorter versions for convenience
// when parsing konsoleprofile commands
static const char GENERAL_GROUP[]     = "General";
static const char KEYBOARD_GROUP[]    = "Keyboard";
static const char APPEARANCE_GROUP[]  = "Appearance";
static const char SCROLLING_GROUP[]   = "Scrolling";
static const char TERMINAL_GROUP[]    = "Terminal Features";
static const char CURSOR_GROUP[]      = "Cursor Options";
static const char INTERACTION_GROUP[] = "Interaction Options";
static const char ENCODING_GROUP[]    = "Encoding Options";

const Profile::PropertyInfo Profile::DefaultPropertyNames[] = {
    // General
    { Path , "Path" , nullptr , QVariant::String }
    , { Name , "Name" , GENERAL_GROUP , QVariant::String }
    , { UntranslatedName, "UntranslatedName" , nullptr , QVariant::String }
    , { Icon , "Icon" , GENERAL_GROUP , QVariant::String }
    , { Command , "Command" , nullptr , QVariant::String }
    , { Arguments , "Arguments" , nullptr , QVariant::StringList }
    , { MenuIndex, "MenuIndex" , nullptr, QVariant::String }
    , { Environment , "Environment" , GENERAL_GROUP , QVariant::StringList }
    , { Directory , "Directory" , GENERAL_GROUP , QVariant::String }
    , { LocalTabTitleFormat , "LocalTabTitleFormat" , GENERAL_GROUP , QVariant::String }
    , { LocalTabTitleFormat , "tabtitle" , nullptr , QVariant::String }
    , { RemoteTabTitleFormat , "RemoteTabTitleFormat" , GENERAL_GROUP , QVariant::String }
    , { ShowTerminalSizeHint , "ShowTerminalSizeHint" , GENERAL_GROUP , QVariant::Bool }
    , { DimWhenInactive , "DimWhenInactive" , GENERAL_GROUP , QVariant::Bool }
    , { StartInCurrentSessionDir , "StartInCurrentSessionDir" , GENERAL_GROUP , QVariant::Bool }
    , { SilenceSeconds, "SilenceSeconds" , GENERAL_GROUP , QVariant::Int }
    , { TerminalColumns, "TerminalColumns" , GENERAL_GROUP , QVariant::Int }
    , { TerminalRows, "TerminalRows" , GENERAL_GROUP , QVariant::Int }
    , { TerminalMargin, "TerminalMargin" , GENERAL_GROUP , QVariant::Int }
    , { TerminalCenter, "TerminalCenter" , GENERAL_GROUP , QVariant::Bool }

    // Appearance
    , { Font , "Font" , APPEARANCE_GROUP , QVariant::Font }
    , { ColorScheme , "ColorScheme" , APPEARANCE_GROUP , QVariant::String }
    , { ColorScheme , "colors" , nullptr , QVariant::String }
    , { AntiAliasFonts, "AntiAliasFonts" , APPEARANCE_GROUP , QVariant::Bool }
    , { BoldIntense, "BoldIntense", APPEARANCE_GROUP, QVariant::Bool }
    , { UseFontLineCharacters, "UseFontLineChararacters", APPEARANCE_GROUP, QVariant::Bool }
    , { LineSpacing , "LineSpacing" , APPEARANCE_GROUP , QVariant::Int }
    , { TabColor, "TabColor", APPEARANCE_GROUP, QVariant::Color }

    // Keyboard
    , { KeyBindings , "KeyBindings" , KEYBOARD_GROUP , QVariant::String }

    // Scrolling
    , { HistoryMode , "HistoryMode" , SCROLLING_GROUP , QVariant::Int }
    , { HistorySize , "HistorySize" , SCROLLING_GROUP , QVariant::Int }
    , { ScrollBarPosition , "ScrollBarPosition" , SCROLLING_GROUP , QVariant::Int }
    , { ScrollFullPage , "ScrollFullPage" , SCROLLING_GROUP , QVariant::Bool }
    , { HighlightScrolledLines , "HighlightScrolledLines" , SCROLLING_GROUP , QVariant::Bool }

    // Terminal Features
    , { UrlHintsModifiers , "UrlHintsModifiers" , TERMINAL_GROUP , QVariant::Int }
    , { ReverseUrlHints , "ReverseUrlHints" , TERMINAL_GROUP , QVariant::Bool }
    , { BlinkingTextEnabled , "BlinkingTextEnabled" , TERMINAL_GROUP , QVariant::Bool }
    , { FlowControlEnabled , "FlowControlEnabled" , TERMINAL_GROUP , QVariant::Bool }
    , { BidiRenderingEnabled , "BidiRenderingEnabled" , TERMINAL_GROUP , QVariant::Bool }
    , { BlinkingCursorEnabled , "BlinkingCursorEnabled" , TERMINAL_GROUP , QVariant::Bool }
    , { BellMode , "BellMode" , TERMINAL_GROUP , QVariant::Int }

    // Cursor
    , { UseCustomCursorColor , "UseCustomCursorColor" , CURSOR_GROUP , QVariant::Bool}
    , { CursorShape , "CursorShape" , CURSOR_GROUP , QVariant::Int}
    , { CustomCursorColor , "CustomCursorColor" , CURSOR_GROUP , QVariant::Color }
    , { CustomCursorTextColor , "CustomCursorTextColor" , CURSOR_GROUP , QVariant::Color }

    // Interaction
    , { WordCharacters , "WordCharacters" , INTERACTION_GROUP , QVariant::String }
    , { TripleClickMode , "TripleClickMode" , INTERACTION_GROUP , QVariant::Int }
    , { UnderlineLinksEnabled , "UnderlineLinksEnabled" , INTERACTION_GROUP , QVariant::Bool }
    , { UnderlineFilesEnabled , "UnderlineFilesEnabled" , INTERACTION_GROUP , QVariant::Bool }
    , { OpenLinksByDirectClickEnabled , "OpenLinksByDirectClickEnabled" , INTERACTION_GROUP , QVariant::Bool }
    , { CtrlRequiredForDrag, "CtrlRequiredForDrag" , INTERACTION_GROUP , QVariant::Bool }
    , { DropUrlsAsText , "DropUrlsAsText" , INTERACTION_GROUP , QVariant::Bool }
    , { AutoCopySelectedText , "AutoCopySelectedText" , INTERACTION_GROUP , QVariant::Bool }
    , { CopyTextAsHTML , "CopyTextAsHTML" , INTERACTION_GROUP , QVariant::Bool }
    , { TrimLeadingSpacesInSelectedText , "TrimLeadingSpacesInSelectedText" , INTERACTION_GROUP , QVariant::Bool }
    , { TrimTrailingSpacesInSelectedText , "TrimTrailingSpacesInSelectedText" , INTERACTION_GROUP , QVariant::Bool }
    , { PasteFromSelectionEnabled , "PasteFromSelectionEnabled" , INTERACTION_GROUP , QVariant::Bool }
    , { PasteFromClipboardEnabled , "PasteFromClipboardEnabled" , INTERACTION_GROUP , QVariant::Bool }
    , { MiddleClickPasteMode, "MiddleClickPasteMode" , INTERACTION_GROUP , QVariant::Int }
    , { MouseWheelZoomEnabled, "MouseWheelZoomEnabled", INTERACTION_GROUP, QVariant::Bool }
    , { AlternateScrolling, "AlternateScrolling", INTERACTION_GROUP, QVariant::Bool }

    // Encoding
    , { DefaultEncoding , "DefaultEncoding" , ENCODING_GROUP , QVariant::String }

    , { static_cast<Profile::Property>(0) , nullptr , nullptr, QVariant::Invalid }
};

QHash<QString, Profile::PropertyInfo> Profile::PropertyInfoByName;
QHash<Profile::Property, Profile::PropertyInfo> Profile::PropertyInfoByProperty;

void Profile::fillTableWithDefaultNames()
{
    static bool filledDefaults = false;

    if (filledDefaults) {
        return;
    }

    const PropertyInfo* iter = DefaultPropertyNames;
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
    setProperty(StartInCurrentSessionDir, true);
    setProperty(MenuIndex, QStringLiteral("0"));
    setProperty(SilenceSeconds, 10);
    setProperty(TerminalColumns, 80);
    setProperty(TerminalRows, 24);
    setProperty(TerminalMargin, 1);
    setProperty(TerminalCenter, false);
    setProperty(MouseWheelZoomEnabled, true);
    setProperty(AlternateScrolling, true);

    setProperty(KeyBindings, QStringLiteral("default"));
    setProperty(ColorScheme, QStringLiteral("Breeze"));
    setProperty(Font, QFontDatabase::systemFont(QFontDatabase::FixedFont));

    setProperty(HistoryMode, Enum::FixedSizeHistory);
    setProperty(HistorySize, 1000);
    setProperty(ScrollBarPosition, Enum::ScrollBarRight);
    setProperty(ScrollFullPage, false);
    setProperty(HighlightScrolledLines, true);

    setProperty(FlowControlEnabled, true);
    setProperty(UrlHintsModifiers, 0);
    setProperty(ReverseUrlHints, false);
    setProperty(BlinkingTextEnabled, true);
    setProperty(UnderlineLinksEnabled, true);
    setProperty(UnderlineFilesEnabled, false);
    setProperty(OpenLinksByDirectClickEnabled, false);
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
    const PropertyInfo* properties = DefaultPropertyNames;
    while (properties->name != nullptr) {
        Property current = properties->property;
        QVariant otherValue = profile->property<QVariant>(current);
        switch (current) {
        case Name:
        case Path:
            break;
        default:
            if (!differentOnly ||
                    property<QVariant>(current) != otherValue) {
                setProperty(current, otherValue);
            }
        }
        properties++;
    }
}

Profile::~Profile() = default;

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

void Profile::setProperty(Property p, const QVariant& value)
{
    _propertyValues.insert(p, value);
}

bool Profile::isPropertySet(Property p) const
{
    return _propertyValues.contains(p);
}

Profile::Property Profile::lookupByName(const QString& name)
{
    // insert default names into table the first time this is called
    fillTableWithDefaultNames();

    return PropertyInfoByName[name.toLower()].property;
}

void Profile::registerProperty(const PropertyInfo& info)
{
    QString name = QLatin1String(info.name);
    PropertyInfoByName.insert(name.toLower(), info);

    // only allow one property -> name map
    // (multiple name -> property mappings are allowed though)
    if (!PropertyInfoByProperty.contains(info.property)) {
        PropertyInfoByProperty.insert(info.property, info);
    }
}

int Profile::menuIndexAsInt() const
{
    bool ok;
    int index = menuIndex().toInt(&ok, 10);
    if (ok) {
        return index;
    }
    return 0;
}

const QStringList Profile::propertiesInfoList() const
{
    QStringList info;
    const PropertyInfo* iter = DefaultPropertyNames;
    while (iter->name != nullptr) {
        info << QLatin1String(iter->name) + QStringLiteral(" : ") + QLatin1String(QVariant(iter->type).typeName());
        iter++;
    }

    return info;
}

QHash<Profile::Property, QVariant> ProfileCommandParser::parse(const QString& input)
{
    QHash<Profile::Property, QVariant> changes;

    // regular expression to parse profile change requests.
    //
    // format: property=value;property=value ...
    //
    // where 'property' is a word consisting only of characters from A-Z
    // where 'value' is any sequence of characters other than a semi-colon
    //
    static const QRegularExpression regExp(QStringLiteral("([a-zA-Z]+)=([^;]+)"));

    QRegularExpressionMatchIterator iterator(regExp.globalMatch(input));
    while (iterator.hasNext()) {
        QRegularExpressionMatch match(iterator.next());
        Profile::Property property = Profile::lookupByName(match.captured(1));
        const QString value = match.captured(2);
        changes.insert(property, value);
    }

    return changes;
}

void ProfileGroup::updateValues()
{
    const PropertyInfo* properties = Profile::DefaultPropertyNames;
    while (properties->name != nullptr) {
        // the profile group does not store a value for some properties
        // (eg. name, path) if even they are equal between profiles -
        //
        // the exception is when the group has only one profile in which
        // case it behaves like a standard Profile
        if (_profiles.count() > 1 &&
                !canInheritProperty(properties->property)) {
            properties++;
            continue;
        }

        QVariant value;
        for (int i = 0; i < _profiles.count(); i++) {
            QVariant profileValue = _profiles[i]->property<QVariant>(properties->property);
            if (value.isNull()) {
                value = profileValue;
            } else if (value != profileValue) {
                value = QVariant();
                break;
            }
        }
        Profile::setProperty(properties->property, value);
        properties++;
    }
}

void ProfileGroup::setProperty(Property p, const QVariant& value)
{
    if (_profiles.count() > 1 && !canInheritProperty(p)) {
        return;
    }

    Profile::setProperty(p, value);
    for (const Profile::Ptr &profile : qAsConst(_profiles)) {
        profile->setProperty(p, value);
    }
}

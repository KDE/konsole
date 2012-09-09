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
#include <QtCore/QTextCodec>

// KDE
#include <KGlobalSettings>
#include <KLocalizedString>

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
    { Path , "Path" , 0 , QVariant::String }
    , { Name , "Name" , GENERAL_GROUP , QVariant::String }
    , { UntranslatedName, "UntranslatedName" , 0 , QVariant::String }
    , { Icon , "Icon" , GENERAL_GROUP , QVariant::String }
    , { Command , "Command" , 0 , QVariant::String }
    , { Arguments , "Arguments" , 0 , QVariant::StringList }
    , { MenuIndex, "MenuIndex" , 0, QVariant::String }
    , { Environment , "Environment" , GENERAL_GROUP , QVariant::StringList }
    , { Directory , "Directory" , GENERAL_GROUP , QVariant::String }
    , { LocalTabTitleFormat , "LocalTabTitleFormat" , GENERAL_GROUP , QVariant::String }
    , { LocalTabTitleFormat , "tabtitle" , 0 , QVariant::String }
    , { RemoteTabTitleFormat , "RemoteTabTitleFormat" , GENERAL_GROUP , QVariant::String }
    , { ShowTerminalSizeHint , "ShowTerminalSizeHint" , GENERAL_GROUP , QVariant::Bool }
    , { SaveGeometryOnExit , "SaveGeometryOnExit" , GENERAL_GROUP , QVariant::Bool }
    , { StartInCurrentSessionDir , "StartInCurrentSessionDir" , GENERAL_GROUP , QVariant::Bool }
    , { SilenceSeconds, "SilenceSeconds" , GENERAL_GROUP , QVariant::Int }
    , { TerminalColumns, "TerminalColumns" , GENERAL_GROUP , QVariant::Int }
    , { TerminalRows, "TerminalRows" , GENERAL_GROUP , QVariant::Int }

    // Appearance
    , { Font , "Font" , APPEARANCE_GROUP , QVariant::Font }
    , { ColorScheme , "ColorScheme" , APPEARANCE_GROUP , QVariant::String }
    , { ColorScheme , "colors" , 0 , QVariant::String }
    , { AntiAliasFonts, "AntiAliasFonts" , APPEARANCE_GROUP , QVariant::Bool }
    , { BoldIntense, "BoldIntense", APPEARANCE_GROUP, QVariant::Bool }
    , { LineSpacing , "LineSpacing" , APPEARANCE_GROUP , QVariant::Int }

    // Keyboard
    , { KeyBindings , "KeyBindings" , KEYBOARD_GROUP , QVariant::String }

    // Scrolling
    , { HistoryMode , "HistoryMode" , SCROLLING_GROUP , QVariant::Int }
    , { HistorySize , "HistorySize" , SCROLLING_GROUP , QVariant::Int }
    , { ScrollBarPosition , "ScrollBarPosition" , SCROLLING_GROUP , QVariant::Int }

    // Terminal Features
    , { BlinkingTextEnabled , "BlinkingTextEnabled" , TERMINAL_GROUP , QVariant::Bool }
    , { FlowControlEnabled , "FlowControlEnabled" , TERMINAL_GROUP , QVariant::Bool }
    , { BidiRenderingEnabled , "BidiRenderingEnabled" , TERMINAL_GROUP , QVariant::Bool }
    , { BlinkingCursorEnabled , "BlinkingCursorEnabled" , TERMINAL_GROUP , QVariant::Bool }
    , { BellMode , "BellMode" , TERMINAL_GROUP , QVariant::Int }

    // Cursor
    , { UseCustomCursorColor , "UseCustomCursorColor" , CURSOR_GROUP , QVariant::Bool}
    , { CursorShape , "CursorShape" , CURSOR_GROUP , QVariant::Int}
    , { CustomCursorColor , "CustomCursorColor" , CURSOR_GROUP , QVariant::Color }

    // Interaction
    , { WordCharacters , "WordCharacters" , INTERACTION_GROUP , QVariant::String }
    , { TripleClickMode , "TripleClickMode" , INTERACTION_GROUP , QVariant::Int }
    , { UnderlineLinksEnabled , "UnderlineLinksEnabled" , INTERACTION_GROUP , QVariant::Bool }
    , { OpenLinksByDirectClickEnabled , "OpenLinksByDirectClickEnabled" , INTERACTION_GROUP , QVariant::Bool }
    , { CtrlRequiredForDrag, "CtrlRequiredForDrag" , INTERACTION_GROUP , QVariant::Bool }
    , { AutoCopySelectedText , "AutoCopySelectedText" , INTERACTION_GROUP , QVariant::Bool }
    , { TrimTrailingSpacesInSelectedText , "TrimTrailingSpacesInSelectedText" , INTERACTION_GROUP , QVariant::Bool }
    , { PasteFromSelectionEnabled , "PasteFromSelectionEnabled" , INTERACTION_GROUP , QVariant::Bool }
    , { PasteFromClipboardEnabled , "PasteFromClipboardEnabled" , INTERACTION_GROUP , QVariant::Bool }
    , { MiddleClickPasteMode, "MiddleClickPasteMode" , INTERACTION_GROUP , QVariant::Int }

    // Encoding
    , { DefaultEncoding , "DefaultEncoding" , ENCODING_GROUP , QVariant::String }

    , { (Property)0 , 0 , 0, QVariant::Invalid }
};

QHash<QString, Profile::PropertyInfo> Profile::PropertyInfoByName;
QHash<Profile::Property, Profile::PropertyInfo> Profile::PropertyInfoByProperty;

void Profile::fillTableWithDefaultNames()
{
    static bool filledDefaults = false;

    if (filledDefaults)
        return;

    const PropertyInfo* iter = DefaultPropertyNames;
    while (iter->name != 0) {
        registerProperty(*iter);
        iter++;
    }

    filledDefaults = true;
}

FallbackProfile::FallbackProfile()
    : Profile()
{
    // Fallback settings
    setProperty(Name, i18n("Shell"));
    setProperty(UntranslatedName, "Shell");
    // magic path for the fallback profile which is not a valid
    // non-directory file name
    setProperty(Path, "FALLBACK/");
    setProperty(Command, qgetenv("SHELL"));
    setProperty(Arguments, QStringList() << qgetenv("SHELL"));
    setProperty(Icon, "utilities-terminal");
    setProperty(Environment, QStringList() << "TERM=xterm");
    setProperty(LocalTabTitleFormat, "%d : %n");
    setProperty(RemoteTabTitleFormat, "(%u) %H");
    setProperty(ShowTerminalSizeHint, true);
    setProperty(SaveGeometryOnExit, true);
    setProperty(StartInCurrentSessionDir, true);
    setProperty(MenuIndex, "0");
    setProperty(SilenceSeconds, 10);
    setProperty(TerminalColumns, 80);
    setProperty(TerminalRows, 40);

    setProperty(KeyBindings, "default");
    setProperty(ColorScheme, "Linux"); //use DarkPastels when is start support blue ncurses UI properly
    setProperty(Font, KGlobalSettings::fixedFont());

    setProperty(HistoryMode, Enum::FixedSizeHistory);
    setProperty(HistorySize, 1000);
    setProperty(ScrollBarPosition, Enum::ScrollBarRight);

    setProperty(FlowControlEnabled, true);
    setProperty(BlinkingTextEnabled, true);
    setProperty(UnderlineLinksEnabled, true);
    setProperty(OpenLinksByDirectClickEnabled, false);
    setProperty(CtrlRequiredForDrag, true);
    setProperty(AutoCopySelectedText, false);
    setProperty(TrimTrailingSpacesInSelectedText, false);
    setProperty(PasteFromSelectionEnabled, true);
    setProperty(PasteFromClipboardEnabled, false);
    setProperty(MiddleClickPasteMode, Enum::PasteFromX11Selection);
    setProperty(TripleClickMode, Enum::SelectWholeLine);

    setProperty(BlinkingCursorEnabled, false);
    setProperty(BidiRenderingEnabled, true);
    setProperty(LineSpacing, 0);
    setProperty(CursorShape, Enum::BlockCursor);
    setProperty(UseCustomCursorColor, false);
    setProperty(CustomCursorColor, Qt::black);
    setProperty(BellMode, Enum::NotifyBell);

    setProperty(DefaultEncoding, QString(QTextCodec::codecForLocale()->name()));
    setProperty(AntiAliasFonts, true);
    setProperty(BoldIntense, true);

    // default taken from KDE 3
    setProperty(WordCharacters, ":@-./_~?&=%+#");

    // Fallback should not be shown in menus
    setHidden(true);
}
Profile::Profile(Profile::Ptr parent)
    : _parent(parent)
    , _hidden(false)
{
}
void Profile::clone(Profile::Ptr profile, bool differentOnly)
{
    const PropertyInfo* properties = DefaultPropertyNames;
    while (properties->name != 0) {
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
Profile::~Profile()
{
}
bool Profile::isHidden() const
{
    return _hidden;
}
void Profile::setHidden(bool hidden)
{
    _hidden = hidden;
}

void Profile::setParent(Profile::Ptr parent)
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
void Profile::setProperty(Property property , const QVariant& value)
{
    _propertyValues.insert(property, value);
}
bool Profile::isPropertySet(Property property) const
{
    return _propertyValues.contains(property);
}

Profile::Property Profile::lookupByName(const QString& name)
{
    // insert default names into table the first time this is called
    fillTableWithDefaultNames();

    return PropertyInfoByName[name.toLower()].property;
}

void Profile::registerProperty(const PropertyInfo& info)
{
    PropertyInfoByName.insert(QString(info.name).toLower(), info);

    // only allow one property -> name map
    // (multiple name -> property mappings are allowed though)
    if (!PropertyInfoByProperty.contains(info.property))
        PropertyInfoByProperty.insert(info.property, info);
}

int Profile::menuIndexAsInt() const
{
    bool ok;
    int index = menuIndex().toInt(&ok, 10);
    if (ok)
        return index;
    else
        return 0;
}

const QStringList Profile::propertiesInfoList() const
{
    QStringList info;
    const PropertyInfo* iter = DefaultPropertyNames;
    while (iter->name != 0) {
        info << QString(iter->name) + " : " + QString(QVariant(iter->type).typeName());
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
    static QRegExp regExp("([a-zA-Z]+)=([^;]+)");

    int offset = 0;
    while (regExp.indexIn(input, offset) != -1) {
        if (regExp.capturedTexts().count() == 3) {
            Profile::Property property = Profile::lookupByName(
                                             regExp.capturedTexts()[1]);
            const QString value = regExp.capturedTexts()[2];
            changes.insert(property, value);
        }

        offset = input.indexOf(';', offset) + 1;
        if (offset == 0)
            break;
    }

    return changes;
}

void ProfileGroup::updateValues()
{
    const PropertyInfo* properties = Profile::DefaultPropertyNames;
    while (properties->name != 0) {
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
            if (value.isNull())
                value = profileValue;
            else if (value != profileValue) {
                value = QVariant();
                break;
            }
        }
        Profile::setProperty(properties->property, value);
        properties++;
    }
}
void ProfileGroup::setProperty(Property property, const QVariant& value)
{
    if (_profiles.count() > 1 && !canInheritProperty(property))
        return;

    Profile::setProperty(property, value);
    foreach(Profile::Ptr profile, _profiles) {
        profile->setProperty(property, value);
    }
}


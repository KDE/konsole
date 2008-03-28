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

// Own
#include "Profile.h"

// Qt
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTextCodec>

// KDE
#include <KConfigGroup>
#include <KDesktopFile>
#include <KGlobal>
#include <KGlobalSettings>
#include <KLocale>
#include <KDebug>
#include <KStandardDirs>

// System
#include <unistd.h>

// Konsole
#include "ShellCommand.h"

using namespace Konsole;

// mappings between property enum values and names
//
// multiple names are defined for some property values,
// in these cases, the "proper" string name comes first,
// as that is used when reading/writing profiles from/to disk
//
// the other names are usually shorter versions for convenience
// when parsing konsoleprofile commands
static const char* GENERAL_GROUP = "General";
static const char* KEYBOARD_GROUP = "Keyboard";
static const char* APPEARANCE_GROUP = "Appearance";
static const char* SCROLLING_GROUP = "Scrolling";
static const char* TERMINAL_GROUP = "Terminal Features";
static const char* CURSOR_GROUP = "Cursor Options";
static const char* INTERACTION_GROUP = "Interaction Options";
static const char* ENCODING_GROUP = "Encoding Options";

const Profile::PropertyInfo Profile::DefaultPropertyNames[] =
{
	// General
      { Path , "Path" , 0 , QVariant::String }
    , { Name , "Name" , GENERAL_GROUP , QVariant::String }
    , { Title , "Title" , 0 , QVariant::String }
    , { Icon , "Icon" , GENERAL_GROUP , QVariant::String }
    , { Command , "Command" , 0 , QVariant::String }
    , { Arguments , "Arguments" , 0 , QVariant::StringList }
    , { Environment , "Environment" , GENERAL_GROUP , QVariant::StringList }
    , { Directory , "Directory" , GENERAL_GROUP , QVariant::String }
    , { LocalTabTitleFormat , "LocalTabTitleFormat" , GENERAL_GROUP , QVariant::String }
    , { LocalTabTitleFormat , "tabtitle" , 0 , QVariant::String }
    , { RemoteTabTitleFormat , "RemoteTabTitleFormat" , GENERAL_GROUP , QVariant::String }
    , { ShowMenuBar , "ShowMenuBar" , GENERAL_GROUP , QVariant::Bool }
    , { TabBarMode , "TabBarMode" , GENERAL_GROUP , QVariant::Int }
    , { TabBarPosition , "TabBarPosition" , GENERAL_GROUP , QVariant::Int }
	, { StartInCurrentSessionDir , "StartInCurrentSessionDir" , GENERAL_GROUP , QVariant::Bool }
    
	// Appearance
	, { Font , "Font" , APPEARANCE_GROUP , QVariant::Font }
    , { ColorScheme , "ColorScheme" , APPEARANCE_GROUP , QVariant::String }
    , { ColorScheme , "colors" , 0 , QVariant::String }
    , { AntiAliasFonts, "AntiAliasFonts" , APPEARANCE_GROUP , QVariant::Bool }
    
	// Keyboard
    , { KeyBindings , "KeyBindings" , KEYBOARD_GROUP , QVariant::String }
    
	// Scrolling
	, { HistoryMode , "HistoryMode" , SCROLLING_GROUP , QVariant::Int }
    , { HistorySize , "HistorySize" , SCROLLING_GROUP , QVariant::Int } 
    , { ScrollBarPosition , "ScrollBarPosition" , SCROLLING_GROUP , QVariant::Int }
   
   	// Terminal Features
	, { BlinkingTextEnabled , "BlinkingTextEnabled" , TERMINAL_GROUP , QVariant::Bool }
    , { FlowControlEnabled , "FlowControlEnabled" , TERMINAL_GROUP , QVariant::Bool }
    , { AllowProgramsToResizeWindow , "AllowProgramsToResizeWindow" , TERMINAL_GROUP , QVariant::Bool }
    , { BlinkingCursorEnabled , "BlinkingCursorEnabled" , TERMINAL_GROUP , QVariant::Bool }
    
	// Cursor 
	, { UseCustomCursorColor , "UseCustomCursorColor" , CURSOR_GROUP , QVariant::Bool}
    , { CursorShape , "CursorShape" , CURSOR_GROUP , QVariant::Int}
    , { CustomCursorColor , "CustomCursorColor" , CURSOR_GROUP , QVariant::Color }

	// Interaction
    , { WordCharacters , "WordCharacters" , INTERACTION_GROUP , QVariant::String }

	// Encoding
    , { DefaultEncoding , "DefaultEncoding" , ENCODING_GROUP , QVariant::String }

    , { (Property)0 , 0 , 0, QVariant::Invalid }
};

QHash<QString,Profile::PropertyInfo> Profile::_propertyInfoByName;
QHash<Profile::Property,Profile::PropertyInfo> Profile::_infoByProperty;

void Profile::fillTableWithDefaultNames()
{
    static bool filledDefaults = false;

    if ( filledDefaults )
        return;

    const PropertyInfo* iter = DefaultPropertyNames;
    while ( iter->name != 0 )
    {
       registerProperty(*iter);
       iter++;
    }

   filledDefaults = true; 
}

FallbackProfile::FallbackProfile()
 : Profile()
{
    // Fallback settings
    setProperty(Name,i18n("Shell"));
    setProperty(Command,getenv("SHELL"));
    setProperty(Icon,"utilities-terminal");
    setProperty(Arguments,QStringList() << getenv("SHELL"));
    setProperty(Environment,QStringList() << "TERM=xterm");
    setProperty(LocalTabTitleFormat,"%d : %n");
    setProperty(RemoteTabTitleFormat,"%H (%u)");
    setProperty(TabBarMode,AlwaysShowTabBar);
    setProperty(TabBarPosition,TabBarBottom);
    setProperty(ShowMenuBar,true);
	setProperty(StartInCurrentSessionDir,true);

    setProperty(KeyBindings,"default");
    setProperty(ColorScheme,"Linux");
    setProperty(Font,KGlobalSettings::fixedFont());

    setProperty(HistoryMode,FixedSizeHistory);
    setProperty(HistorySize,1000);
    setProperty(ScrollBarPosition,ScrollBarRight);
    
    setProperty(FlowControlEnabled,true);
    setProperty(AllowProgramsToResizeWindow,true);
    setProperty(BlinkingTextEnabled,true);
    
    setProperty(BlinkingCursorEnabled,false);
    setProperty(CursorShape,BlockCursor);
    setProperty(UseCustomCursorColor,false);
    setProperty(CustomCursorColor,Qt::black);

    setProperty(DefaultEncoding,QString(QTextCodec::codecForLocale()->name()));
    setProperty(AntiAliasFonts,true);

    // default taken from KDE 3
    setProperty(WordCharacters,":@-./_~?&=%+#");

    // Fallback should not be shown in menus
    setHidden(true);
}

Profile::Profile(Profile::Ptr parent)
    : _parent(parent)
     ,_hidden(false)
{
}
Profile::~Profile()
{
	if (!name().isEmpty() && !isHidden())
		kDebug() << "Destroying profile " << name();
}
bool Profile::isHidden() const { return _hidden; }
void Profile::setHidden(bool hidden) { _hidden = hidden; }

void Profile::setParent(Profile::Ptr parent) { _parent = parent; }
const Profile::Ptr Profile::parent() const { return _parent; }

bool Profile::isEmpty() const
{
    return _propertyValues.isEmpty();
}
QHash<Profile::Property,QVariant> Profile::setProperties() const
{
    return _propertyValues;
}
void Profile::setProperty(Property property , const QVariant& value)
{
    _propertyValues.insert(property,value);
}
bool Profile::isPropertySet(Property property) const
{
    return _propertyValues.contains(property);
}

bool Profile::isNameRegistered(const QString& name) 
{
    // insert default names into table the first time this is called
    fillTableWithDefaultNames();

    return _propertyInfoByName.contains(name);
}

Profile::Property Profile::lookupByName(const QString& name)
{
    // insert default names into table the first time this is called
    fillTableWithDefaultNames();

    return _propertyInfoByName[name.toLower()].property;
}
QString Profile::primaryNameForProperty(Property property)
{
    // insert default names into table the first time this is called
    fillTableWithDefaultNames();

    return _infoByProperty[property].name;
}
QList<QString> Profile::namesForProperty(Property property)
{
    // insert default names into table the first time this is called
    fillTableWithDefaultNames();

    return QList<QString>() << primaryNameForProperty(property);
}
void Profile::registerProperty(const PropertyInfo& info) 
{
    _propertyInfoByName.insert(QString(info.name).toLower(),info);

    // only allow one property -> name map
    // (multiple name -> property mappings are allowed though)
    if ( !_infoByProperty.contains(info.property) )
        _infoByProperty.insert(info.property,info);
}

QString KDE4ProfileWriter::getPath(const Profile::Ptr info)
{
    QString newPath;

    if ( info->isPropertySet(Profile::Path) && 
         info->path().startsWith(KGlobal::dirs()->saveLocation("data", "konsole/")) )
    {
        newPath = info->path();
    }
    else
    {
        // use the profile name + ".profile" and save it in $KDEHOME
        newPath = KGlobal::dirs()->saveLocation("data","konsole/") + info->name() + ".profile";
    }

    kDebug(1211) << "Saving profile under name: " << newPath;

    return newPath;
}
void KDE4ProfileWriter::writeProperties(KConfig& config,
										const Profile::Ptr profile,
										const Profile::PropertyInfo* properties) 
{
	const char* groupName = 0;
	KConfigGroup group;
	
	while (properties->name != 0)	
	{
		if (properties->group != 0)
		{
			if (groupName == 0 || strcmp(groupName,properties->group) != 0)
			{
				group = config.group(properties->group);
				groupName = properties->group;
			}

			if ( profile->isPropertySet(properties->property) )
        		group.writeEntry(QString(properties->name),
							profile->property<QVariant>(properties->property));
		}

		properties++;
	}
}
bool KDE4ProfileWriter::writeProfile(const QString& path , const Profile::Ptr profile)
{
    KConfig config(path,KConfig::NoGlobals);

	KConfigGroup general = config.group(GENERAL_GROUP);

	// Parent profile if set, when loading the profile in future, the parent
    // must be loaded as well if it exists.
    if ( profile->parent() )
        general.writeEntry("Parent",profile->parent()->path());

	if (    profile->isPropertySet(Profile::Command) 
         || profile->isPropertySet(Profile::Arguments) )
        general.writeEntry("Command",
                ShellCommand(profile->command(),profile->arguments()).fullCommand());

	// Write remaining properties
	writeProperties(config,profile,Profile::DefaultPropertyNames);

	return true;
}

QStringList KDE4ProfileReader::findProfiles()
{
    return KGlobal::dirs()->findAllResources("data","konsole/*.profile",
            KStandardDirs::NoDuplicates);
}
void KDE4ProfileReader::readProperties(const KConfig& config, Profile::Ptr profile,
									   const Profile::PropertyInfo* properties)
{
	const char* groupName = 0;
	KConfigGroup group;

	while (properties->name != 0)
	{
		if (properties->group != 0)
		{
			if (groupName == 0 || strcmp(groupName,properties->group) != 0)
			{
				group = config.group(properties->group);
				groupName = properties->group;
			}
		
			QString name(properties->name);

			if (group.hasKey(name))
				profile->setProperty(properties->property,
								 	group.readEntry(name,QVariant(properties->type)));

		}
		
		properties++;
	}
}

bool KDE4ProfileReader::readProfile(const QString& path , Profile::Ptr profile , QString& parentProfile)
{
    KConfig config(path,KConfig::NoGlobals);

    KConfigGroup general = config.group(GENERAL_GROUP);
	if (general.hasKey("Parent"))
		parentProfile = general.readEntry("Parent");

	if ( general.hasKey("Command") )
    {
        ShellCommand shellCommand(general.readEntry("Command"));

        profile->setProperty(Profile::Command,shellCommand.command());
        profile->setProperty(Profile::Arguments,shellCommand.arguments());
    }

	// Read remaining properties
	readProperties(config,profile,Profile::DefaultPropertyNames);

	return true;
}
QStringList KDE3ProfileReader::findProfiles()
{
    return KGlobal::dirs()->findAllResources("data", "konsole/*.desktop", 
            KStandardDirs::NoDuplicates);
}
bool KDE3ProfileReader::readProfile(const QString& path , Profile::Ptr profile , QString& parentProfile)
{
    if (!QFile::exists(path))
        return false;

    // KDE 3 profiles do not have parents
    parentProfile = QString();

    KDesktopFile* desktopFile = new KDesktopFile(path);
    KConfigGroup* config = new KConfigGroup( desktopFile->desktopGroup() );

    if ( config->hasKey("Name") )
        profile->setProperty(Profile::Name,config->readEntry("Name"));

    kDebug() << "reading KDE 3 profile " << profile->name();

    if ( config->hasKey("Icon") )
        profile->setProperty(Profile::Icon,config->readEntry("Icon"));
    if ( config->hasKey("Exec") )
    {
        const QString& fullCommand = config->readEntry("Exec");
        ShellCommand shellCommand(fullCommand);
    
        profile->setProperty(Profile::Command,shellCommand.command());
        profile->setProperty(Profile::Arguments,shellCommand.arguments());
    }
    if ( config->hasKey("Schema") )
    {
        profile->setProperty(Profile::ColorScheme,config->readEntry("Schema").replace
                                            (".schema",QString()));        
    }
    if ( config->hasKey("defaultfont") )
    {
        profile->setProperty(Profile::Font,config->readEntry("defaultfont"));
    }
    if ( config->hasKey("KeyTab") )
    {
        profile->setProperty(Profile::KeyBindings,config->readEntry("KeyTab"));
    }
    if ( config->hasKey("Term") )
    {
        profile->setProperty(Profile::Environment,
                QStringList() << "TERM="+config->readEntry("Term"));
    }
    if ( config->hasKey("Cwd") )
    {
        profile->setProperty(Profile::Directory,config->readEntry("Cwd"));
    }

    delete desktopFile;
    delete config;

    return true;
}

QHash<Profile::Property,QVariant> ProfileCommandParser::parse(const QString& input)
{
    QHash<Profile::Property,QVariant> changes;

    // regular expression to parse profile change requests.
    //
    // format: property=value;property=value ...
    //
    // where 'property' is a word consisting only of characters from A-Z
    // where 'value' is any sequence of characters other than a semi-colon
    //
    static QRegExp regExp("([a-zA-Z]+)=([^;]+)");

    int offset = 0;
    while ( regExp.indexIn(input,offset) != -1 )
    {
        if ( regExp.capturedTexts().count() == 3 )
        {
            Profile::Property property = Profile::lookupByName(
                                                regExp.capturedTexts()[1]);
            const QString value = regExp.capturedTexts()[2];

            kDebug() << "property:" << property << "value:" << value;

            changes.insert(property,value);
        }

        offset = input.indexOf(';',offset) + 1;
        if ( offset == 0 )
            break;
    }

    return changes;
}


#include "Profile.moc"


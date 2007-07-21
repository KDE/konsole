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
#include <KStandardDirs>

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
const Profile::PropertyNamePair Profile::DefaultPropertyNames[] =
{
      { Path , "Path" }
    , { Name , "Name" }
    , { Title , "Title" }
    , { Icon , "Icon" }
    , { Command , "Command" }
    , { Arguments , "Arguments" }
    , { Environment , "Environment" }
    , { Directory , "Directory" }

    , { LocalTabTitleFormat , "LocalTabTitleFormat" }
    , { LocalTabTitleFormat , "tabtitle"}
    
    , { RemoteTabTitleFormat , "RemoteTabTitleFormat" }
    , { ShowMenuBar , "ShowMenuBar" }
    , { TabBarMode , "TabBarMode" }
    , { Font , "Font" }

    , { ColorScheme , "ColorScheme" }
    , { ColorScheme , "colors" }
    
    , { KeyBindings , "KeyBindings" }
    , { HistoryMode , "HistoryMode" }
    , { HistorySize , "HistorySize" } 
    , { ScrollBarPosition , "ScrollBarPosition" }
    , { SelectWordCharacters , "SelectWordCharacters" }
    , { BlinkingTextEnabled , "BlinkingTextEnabled" }
    , { FlowControlEnabled , "FlowControlEnabled" }
    , { AllowProgramsToResizeWindow , "AllowProgramsToResizeWindow" }
    , { BlinkingCursorEnabled , "BlinkingCursorEnabled" }
    , { UseCustomCursorColor , "UseCustomCursorColor" }
    , { CursorShape , "CursorShape" }
    , { CustomCursorColor , "CustomCursorColor" }
    , { WordCharacters , "WordCharacters" }
    , { TabBarPosition , "TabBarPosition" }
    , { DefaultEncoding , "DefaultEncoding" }
    , { (Property)0 , 0 }
};

QHash<QString,Profile::Property> Profile::_propertyByName;
QHash<Profile::Property,QString> Profile::_nameByProperty;

void Profile::fillTableWithDefaultNames()
{
    static bool filledDefaults = false;

    if ( filledDefaults )
        return;

    const PropertyNamePair* iter = DefaultPropertyNames;
    while ( iter->name != 0 )
    {
       registerName(iter->property,iter->name);
       iter++;
    }

   filledDefaults = true; 
}

FallbackProfile::FallbackProfile()
 : Profile(0)
{
    // Fallback settings
    setProperty(Name,i18n("Shell"));
    setProperty(Command,getenv("SHELL"));
    setProperty(Icon,"konsole");
    setProperty(Arguments,QStringList() << getenv("SHELL"));
    setProperty(Environment,QStringList() << "TERM=xterm");
    setProperty(LocalTabTitleFormat,"%d : %n");
    setProperty(RemoteTabTitleFormat,"%H (%u)");
    setProperty(TabBarMode,AlwaysShowTabBar);
    setProperty(TabBarPosition,TabBarBottom);
    setProperty(ShowMenuBar,true);


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

    // default taken from KDE 3
    setProperty(WordCharacters,":@-./_~?&=%+#");

    // Fallback should not be shown in menus
    setHidden(true);
}

Profile::Profile(Profile* parent)
    : _parent(parent)
     ,_hidden(false)
{
}

bool Profile::isHidden() const { return _hidden; }
void Profile::setHidden(bool hidden) { _hidden = hidden; }

void Profile::setParent(Profile* parent) { _parent = parent; }
const Profile* Profile::parent() const { return _parent; }

bool Profile::isEmpty() const
{
    return _propertyValues.isEmpty();
}
QHash<Profile::Property,QVariant> Profile::setProperties() const
{
    return _propertyValues;
}

QVariant Profile::property(Property property) const
{
    if ( _propertyValues.contains(property) )
        return _propertyValues[property];
    else if ( _parent )
        return _parent->property(property);
    else
        return QVariant();
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

    return _propertyByName.contains(name);
}

Profile::Property Profile::lookupByName(const QString& name)
{
    // insert default names into table the first time this is called
    fillTableWithDefaultNames();

    return _propertyByName[name.toLower()];
}
QString Profile::primaryNameForProperty(Property property)
{
    // insert default names into table the first time this is called
    fillTableWithDefaultNames();

    return _nameByProperty[property];
}
QList<QString> Profile::namesForProperty(Property property)
{
    // insert default names into table the first time this is called
    fillTableWithDefaultNames();

    return QList<QString>() << primaryNameForProperty(property);
}
void Profile::registerName(Property property , const QString& name)
{
    _propertyByName.insert(name.toLower(),property);

    // only allow one property -> name map
    // (multiple name -> property mappings are allowed though)
    if ( !_nameByProperty.contains(property) )
        _nameByProperty.insert(property,name);
}

QString KDE4ProfileWriter::getPath(const Profile* info)
{
    QString newPath;
    
    if ( info->isPropertySet(Profile::Path) )
        newPath=info->path();

    // if the path is not specified, use the profile name + ".profile"
    if ( newPath.isEmpty() )
        newPath = info->name() + ".profile";    

    QFileInfo fileInfo(newPath);
    if (!fileInfo.isAbsolute())
        newPath = KGlobal::dirs()->saveLocation("data","konsole/") + newPath;

    qDebug() << "Saving profile under name: " << newPath;

    return newPath;
}
void KDE4ProfileWriter::writeStandardElement(KConfigGroup& group ,  const Profile* profile ,
                                             Profile::Property attribute)
{
    QString name = Profile::primaryNameForProperty(attribute);

    if ( profile->isPropertySet(attribute) )
        group.writeEntry(name,profile->property(attribute));
}
bool KDE4ProfileWriter::writeProfile(const QString& path , const Profile* profile)
{
    KConfig config(path,KConfig::NoGlobals);

    // Basic Profile Settings
    KConfigGroup general = config.group("General");

    // Parent profile if set, when loading the profile in future, the parent
    // must be loaded as well if it exists.
    if ( profile->parent() != 0 )
        general.writeEntry("Parent",profile->parent()->path());

    if ( profile->isPropertySet(Profile::Name) )
        general.writeEntry("Name",profile->name());
    
    if (    profile->isPropertySet(Profile::Command) 
         || profile->isPropertySet(Profile::Arguments) )
        general.writeEntry("Command",
                ShellCommand(profile->command(),profile->arguments()).fullCommand());

    if ( profile->isPropertySet(Profile::Directory) )
        general.writeEntry("Directory",profile->defaultWorkingDirectory());

    writeStandardElement( general ,  profile , Profile::Environment );
    writeStandardElement( general ,  profile , Profile::Icon );
    
    // Tab Titles
    writeStandardElement( general ,  profile , Profile::LocalTabTitleFormat );
    writeStandardElement( general ,  profile , Profile::RemoteTabTitleFormat );

    // Menu and Tab Bar
    writeStandardElement( general ,  profile , Profile::TabBarMode );
    writeStandardElement( general ,  profile , Profile::TabBarPosition );
    writeStandardElement( general ,  profile , Profile::ShowMenuBar );

    // Keyboard
    KConfigGroup keyboard = config.group("Keyboard");
    writeStandardElement( keyboard , profile , Profile::KeyBindings );

    // Appearance
    KConfigGroup appearance = config.group("Appearance");

    writeStandardElement( appearance , profile , Profile::ColorScheme );
    writeStandardElement( appearance , profile , Profile::Font );
   
    // Scrolling
    KConfigGroup scrolling = config.group("Scrolling");

    writeStandardElement( scrolling ,  profile , Profile::HistoryMode );
    writeStandardElement( scrolling ,  profile , Profile::HistorySize );
    writeStandardElement( scrolling ,  profile , Profile::ScrollBarPosition ); 

    // Terminal Features
    KConfigGroup terminalFeatures = config.group("Terminal Features");

    writeStandardElement( terminalFeatures ,  profile , Profile::FlowControlEnabled );
    writeStandardElement( terminalFeatures ,  profile , Profile::BlinkingCursorEnabled );

    // Cursor 
    KConfigGroup cursorOptions = config.group("Cursor Options");

    writeStandardElement( cursorOptions ,  profile , Profile::UseCustomCursorColor );
    writeStandardElement( cursorOptions ,  profile , Profile::CustomCursorColor );
    writeStandardElement( cursorOptions ,  profile , Profile::CursorShape );

    // Interaction
    KConfigGroup interactionOptions = config.group("Interaction Options");

    writeStandardElement( interactionOptions ,  profile , Profile::WordCharacters );

    // Encoding
    KConfigGroup encodingOptions = config.group("Encoding Options");
    writeStandardElement( encodingOptions ,  profile , Profile::DefaultEncoding );

    return true;
}

QStringList KDE4ProfileReader::findProfiles()
{
    return KGlobal::dirs()->findAllResources("data","konsole/*.profile",
            KStandardDirs::NoDuplicates);
}
bool KDE4ProfileReader::readProfile(const QString& path , Profile* profile , QString& parentProfile)
{
    //qDebug() << "KDE 4 Profile Reader:" << path;

    KConfig config(path,KConfig::NoGlobals);

    // general
    KConfigGroup general = config.group("General");

    if ( general.hasKey("Parent") )
        parentProfile = general.readEntry("Parent");

    if ( general.hasKey("Name") )
        profile->setProperty(Profile::Name,general.readEntry("Name"));
    else
        return false;

    if ( general.hasKey("Command") )
    {
        ShellCommand shellCommand(general.readEntry("Command"));

        profile->setProperty(Profile::Command,shellCommand.command());
        profile->setProperty(Profile::Arguments,shellCommand.arguments());
    }

    readStandardElement<QString>(general,profile,Profile::Directory);
    readStandardElement<QStringList>(general,profile,Profile::Environment);
    readStandardElement<QString>(general,profile,Profile::Icon);
    readStandardElement<QString>(general,profile,Profile::LocalTabTitleFormat); 
    readStandardElement<QString>(general,profile,Profile::RemoteTabTitleFormat);
   
    readStandardElement<int>(general,profile,Profile::TabBarMode);
    readStandardElement<int>(general,profile,Profile::TabBarPosition);
    readStandardElement<bool>(general,profile,Profile::ShowMenuBar);

    // keyboard
    KConfigGroup keyboard = config.group("Keyboard");
    readStandardElement<QString>(keyboard,profile,Profile::KeyBindings);

    // appearance
    KConfigGroup appearance = config.group("Appearance");

    readStandardElement<QString>(appearance,profile,Profile::ColorScheme);
    readStandardElement<QFont>(appearance,profile,Profile::Font);

    // scrolling
    KConfigGroup scrolling = config.group("Scrolling");

    readStandardElement<int>(scrolling,profile,Profile::HistoryMode);
    readStandardElement<int>(scrolling,profile,Profile::HistorySize);
    readStandardElement<int>(scrolling,profile,Profile::ScrollBarPosition);

    // terminal features
    KConfigGroup terminalFeatures = config.group("Terminal Features");

    readStandardElement<bool>(terminalFeatures,profile,Profile::FlowControlEnabled);
    readStandardElement<bool>(terminalFeatures,profile,Profile::BlinkingCursorEnabled);

    // cursor settings
    KConfigGroup cursorOptions = config.group("Cursor Options");

    readStandardElement<bool>(cursorOptions,profile,Profile::UseCustomCursorColor);
    readStandardElement<QColor>(cursorOptions,profile,Profile::CustomCursorColor);
    readStandardElement<int>(cursorOptions,profile,Profile::CursorShape);

    // interaction options
    KConfigGroup interactionOptions = config.group("Interaction Options");
    
    readStandardElement<QString>(interactionOptions,profile,Profile::WordCharacters);

    // encoding
    KConfigGroup encodingOptions = config.group("Encoding Options");
    readStandardElement<QString>(encodingOptions,profile,Profile::DefaultEncoding);

    return true;
}
template <typename T>
void KDE4ProfileReader::readStandardElement(const KConfigGroup& group , 
                                            Profile* info , 
                                            Profile::Property property)
{
    QString name = Profile::primaryNameForProperty(property);

    if ( group.hasKey(name) )
        info->setProperty(property,group.readEntry(name,T()));
}
                                                    
QStringList KDE3ProfileReader::findProfiles()
{
    return KGlobal::dirs()->findAllResources("data", "konsole/*.desktop", 
            KStandardDirs::NoDuplicates);
}
bool KDE3ProfileReader::readProfile(const QString& path , Profile* profile , QString& parentProfile)
{
    if (!QFile::exists(path))
        return false;

    // KDE 3 profiles do not have parents
    parentProfile = QString();

    KDesktopFile* desktopFile = new KDesktopFile(path);
    KConfigGroup* config = new KConfigGroup( desktopFile->desktopGroup() );

    if ( config->hasKey("Name") )
        profile->setProperty(Profile::Name,config->readEntry("Name"));

    qDebug() << "reading KDE 3 profile " << profile->name();

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

            qDebug() << "property:" << property << "value:" << value;

            changes.insert(property,value);
        }

        offset = input.indexOf(';',offset) + 1;
        if ( offset == 0 )
            break;
    }

    return changes;
}


#include "Profile.moc"


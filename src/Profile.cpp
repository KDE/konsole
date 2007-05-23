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

// KDE
#include <KConfigGroup>
#include <KDesktopFile>
#include <KGlobal>
#include <KLocale>
#include <KStandardDirs>

// Konsole
#include "ShellCommand.h"

using namespace Konsole;

QHash<QString,Profile::Property> Profile::_propertyNames;

FallbackProfile::FallbackProfile()
 : Profile(0)
{
    // Fallback settings
    setProperty(Name,i18n("Shell"));
    setProperty(Command,getenv("SHELL"));
    setProperty(Icon,"konsole");
    setProperty(Arguments,QStringList() << getenv("SHELL"));
    setProperty(LocalTabTitleFormat,"%d : %n");
    setProperty(RemoteTabTitleFormat,"%H (%u)");
    setProperty(TabBarMode,AlwaysShowTabBar);
    setProperty(TabBarPosition,TabBarBottom);
    setProperty(ShowMenuBar,true);

    setProperty(KeyBindings,"default");

    setProperty(Font,QFont("Monospace"));

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
    return _propertyNames.contains(name);
}

Profile::Property Profile::lookupByName(const QString& name)
{
    return _propertyNames[name];
}

QList<QString> Profile::namesForProperty(Property property)
{
    return _propertyNames.keys(property);
}
void Profile::registerName(Property property , const QString& name)
{
    _propertyNames.insert(name,property);
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
void KDE4ProfileWriter::writeStandardElement(KConfigGroup& group , char* name , const Profile* profile ,
                                             Profile::Property attribute)
{
    if ( profile->isPropertySet(attribute) )
        group.writeEntry(name,profile->property(attribute));
}
bool KDE4ProfileWriter::writeProfile(const QString& path , const Profile* profile)
{
    KConfig config(path,KConfig::NoGlobals);

    // Basic Profile Settings
    KConfigGroup general = config.group("General");

    if ( profile->isPropertySet(Profile::Name) )
        general.writeEntry("Name",profile->name());
    
    if (    profile->isPropertySet(Profile::Command) 
         || profile->isPropertySet(Profile::Arguments) )
        general.writeEntry("Command",
                ShellCommand(profile->command(),profile->arguments()).fullCommand());

    if ( profile->isPropertySet(Profile::Directory) )
        general.writeEntry("Directory",profile->defaultWorkingDirectory());

    writeStandardElement( general , "Icon" , profile , Profile::Icon );
    
    // Tab Titles
    writeStandardElement( general , "LocalTabTitleFormat" , profile , Profile::LocalTabTitleFormat );
    writeStandardElement( general , "RemoteTabTitleFormat" , profile , Profile::RemoteTabTitleFormat );

    // Menu and Tab Bar
    writeStandardElement( general , "TabBarMode" , profile , Profile::TabBarMode );
    writeStandardElement( general , "TabBarPosition" , profile , Profile::TabBarPosition );
    writeStandardElement( general , "ShowMenuBar" , profile , Profile::ShowMenuBar );

    // Keyboard
    KConfigGroup keyboard = config.group("Keyboard");
    writeStandardElement( keyboard , "KeyBindings" , profile , Profile::KeyBindings );

    // Appearance
    KConfigGroup appearance = config.group("Appearance");

    writeStandardElement( appearance , "ColorScheme" , profile , Profile::ColorScheme );
    writeStandardElement( appearance , "Font" , profile , Profile::Font );
   
    // Scrolling
    KConfigGroup scrolling = config.group("Scrolling");

    writeStandardElement( scrolling , "HistoryMode" , profile , Profile::HistoryMode );
    writeStandardElement( scrolling , "HistorySize" , profile , Profile::HistorySize );
    writeStandardElement( scrolling , "ScrollBarPosition" , profile , Profile::ScrollBarPosition ); 

    // Terminal Features
    KConfigGroup terminalFeatures = config.group("Terminal Features");

    writeStandardElement( terminalFeatures , "FlowControl" , profile , Profile::FlowControlEnabled );
    writeStandardElement( terminalFeatures , "BlinkingCursor" , profile , Profile::BlinkingCursorEnabled );

    // Cursor 
    KConfigGroup cursorOptions = config.group("Cursor Options");

    writeStandardElement( cursorOptions , "UseCustomCursorColor"  , profile , Profile::UseCustomCursorColor );
    writeStandardElement( cursorOptions , "CustomCursorColor" , profile , Profile::CustomCursorColor );
    writeStandardElement( cursorOptions , "CursorShape" , profile , Profile::CursorShape );

    // Interaction
    KConfigGroup interactionOptions = config.group("Interaction Options");

    writeStandardElement( interactionOptions , "WordCharacters" , profile , Profile::WordCharacters );

    return true;
}

QStringList KDE4ProfileReader::findProfiles()
{
    return KGlobal::dirs()->findAllResources("data","konsole/*.profile",
            KStandardDirs::NoDuplicates);
}
bool KDE4ProfileReader::readProfile(const QString& path , Profile* profile)
{
    qDebug() << "KDE 4 Profile Reader:" << path;

    KConfig config(path,KConfig::NoGlobals);

    // general
    KConfigGroup general = config.group("General");

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

    readStandardElement<QString>(general,"Directory",profile,Profile::Directory);

    readStandardElement<QString>(general,"Icon",profile,Profile::Icon);
    readStandardElement<QString>(general,"LocalTabTitleFormat",profile,Profile::LocalTabTitleFormat); 
    readStandardElement<QString>(general,"RemoteTabTitleFormat",profile,Profile::RemoteTabTitleFormat);
   
    readStandardElement<int>(general,"TabBarMode",profile,Profile::TabBarMode);
    readStandardElement<int>(general,"TabBarPosition",profile,Profile::TabBarPosition);
    readStandardElement<bool>(general,"ShowMenuBar",profile,Profile::ShowMenuBar);

    // keyboard
    KConfigGroup keyboard = config.group("Keyboard");
    readStandardElement<QString>(keyboard,"KeyBindings",profile,Profile::KeyBindings);

    // appearance
    KConfigGroup appearance = config.group("Appearance");

    readStandardElement<QString>(appearance,"ColorScheme",profile,Profile::ColorScheme);
    readStandardElement<QFont>(appearance,"Font",profile,Profile::Font);

    // scrolling
    KConfigGroup scrolling = config.group("Scrolling");

    readStandardElement<int>(scrolling,"HistoryMode",profile,Profile::HistoryMode);
    readStandardElement<int>(scrolling,"HistorySize",profile,Profile::HistorySize);
    readStandardElement<int>(scrolling,"ScrollBarPosition",profile,Profile::ScrollBarPosition);

    // terminal features
    KConfigGroup terminalFeatures = config.group("Terminal Features");

    readStandardElement<bool>(terminalFeatures,"FlowControl",profile,Profile::FlowControlEnabled);
    readStandardElement<bool>(terminalFeatures,"BlinkingCursor",profile,Profile::BlinkingCursorEnabled);

    // cursor settings
    KConfigGroup cursorOptions = config.group("Cursor Options");

    readStandardElement<bool>(cursorOptions,"UseCustomCursorColor",profile,Profile::UseCustomCursorColor);
    readStandardElement<QColor>(cursorOptions,"CustomCursorColor",profile,Profile::CustomCursorColor);
    readStandardElement<int>(cursorOptions,"CursorShape",profile,Profile::CursorShape);

    // interaction options
    KConfigGroup interactionOptions = config.group("Interaction Options");
    
    readStandardElement<QString>(interactionOptions,"WordCharacters",profile,Profile::WordCharacters);

    return true;
}
template <typename T>
void KDE4ProfileReader::readStandardElement(const KConfigGroup& group , 
                                            char* name , 
                                            Profile* info , 
                                            Profile::Property property)
{
    if ( group.hasKey(name) )
        info->setProperty(property,group.readEntry(name,T()));
}
                                                    
QStringList KDE3ProfileReader::findProfiles()
{
    return KGlobal::dirs()->findAllResources("data", "konsole/*.desktop", 
            KStandardDirs::NoDuplicates);
}
bool KDE3ProfileReader::readProfile(const QString& path , Profile* profile)
{
    if (!QFile::exists(path))
        return false;

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

#include "Profile.moc"


/*
    This source file is part of Konsole, a terminal emulator.

    Copyright (C) 2006 by Robert Knight <robertknight@gmail.com>

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

// Qt
#include <QFileInfo>
#include <QList>
#include <QString>

// KDE
#include <klocale.h>
#include <krun.h>
#include <kshell.h>
#include <kconfig.h>
#include <kstandarddirs.h>
#include <kdesktopfile.h>

// Konsole
#include "ColorScheme.h"
#include "Session.h"
#include "History.h"
#include "SessionManager.h"

using namespace Konsole;

SessionManager* SessionManager::_instance = 0;
QHash<QString,Profile::Property> Profile::_propertyNames;

Profile::Profile(const Profile* parent)
    : _parent(parent)
{
    setProperty(Command,getenv("SHELL"));
    setProperty(Arguments,QStringList() << getenv("SHELL"));
    setProperty(Font,QFont("Monospace"));
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
        qDebug() << "full command = " << fullCommand;
        profile->setProperty(Profile::Command,fullCommand.section(QChar(' '),0,0));
        profile->setProperty(Profile::Arguments,QStringList() << profile->command());//fullCommand.split(QChar(' ')));
    
        qDebug() << "command = " << profile->command();
        qDebug() << "argumetns = " << profile->arguments();
    }

    delete desktopFile;
    delete config;

    return true;
}

#if 0
Profile::Profile(const QString& path)
{
    //QString fileName = QFileInfo(path).fileName();

    //QString fullPath = KStandardDirs::locate("data","konsole/"+fileName);
    //Q_ASSERT( QFile::exists(fullPath) );

    _desktopFile = new KDesktopFile(path);
    _config = new KConfigGroup( _desktopFile->desktopGroup() );

    _path = path;

}
Profile::~Profile()
{
    delete _config; _config = 0;
    delete _desktopFile; _desktopFile = 0;
}

void Profile::setParent( Profile* parent )
{
    _parent = parent;
}
Profile* Profile::parent() const
{
    return _parent;
}
void Profile::setProperty( Property property , const QVariant& value )
{
    _properties[property] = value;
}
QVariant Profile::property( Property property ) const
{
    if ( _properties.contains(property) )
    {
        return _properties[property];
    }
    else
    {
        switch ( property )
        {
            case Name:
                return name();
                break;
            case Icon:
                return icon();
                break;
            default:
                return QVariant();
        }
    }
}

QString Profile::name() const
{
    return _config->readEntry("Name");
}

QString Profile::icon() const
{
    return _config->readEntry("Icon","konsole");
}


bool Profile::isRootSession() const
{
    const QString& cmd = _config->readEntry("Exec");

    return ( cmd.startsWith("su") );
}

QString Profile::command(bool stripRoot , bool stripArguments) const
{
    QString fullCommand = _config->readEntry("Exec");

    //if the .desktop file for this session doesn't specify a binary to run
    //(eg. No 'Exec' entry or empty 'Exec' entry) then use the user's standard SHELL
    if ( fullCommand.isEmpty() )
        fullCommand = getenv("SHELL");

    if ( isRootSession() && stripRoot )
    {
        //command is of the form "su -flags 'commandname'"
        //we need to strip out and return just the command name part.
        fullCommand = fullCommand.section('\'',1,1);
    }

    if ( fullCommand.isEmpty() )
        fullCommand = getenv("SHELL");

    if ( stripArguments )
        return fullCommand.section(QChar(' '),0,0);
    else
        return fullCommand;
}

QStringList Profile::arguments() const
{
    QString commandString = command(false,false);

    //FIXME:  This wll fail where single arguments contain spaces (because slashes or quotation
    //marks are used) - eg. vi My\ File\ Name
    return commandString.split(QChar(' '));
}

bool Profile::isAvailable() const
{
    //TODO:  Is it necessary to cache the result of the search?

    QString binary = KRun::binaryName( command(true) , false );
    binary = KShell::tildeExpand(binary);

    QString fullBinaryPath = KGlobal::dirs()->findExe(binary);

    if ( fullBinaryPath.isEmpty() )
        return false;
    else
        return true;
}

QString Profile::path() const
{
    return _path;
}


QString Profile::newSessionText() const
{
    QString commentEntry = _config->readEntry("Comment");

    if ( commentEntry.isEmpty() )
        return i18n("New %1",name());
    else
        return commentEntry;
}

QString Profile::terminal() const
{
    return _config->readEntry("Term","xterm");
}
QString Profile::keyboardSetup() const
{
    return _config->readEntry("KeyTab",QString());
}
QString Profile::colorScheme() const
{
    //TODO Pick a default color scheme
    return _config->readEntry("Schema").replace(".schema",QString::null);
}
QFont Profile::defaultFont() const
{
    //TODO Use system default font here
    const QFont& font = QFont("Monospace");

    if (_config->hasKey("Font"))
    {
        // it is possible for the Font key to exist, but to be empty, in which
        // case '_config->readEntry("Font")' will return the default application
        // font, which will most likely not be suitable for use in the terminal
        QString fontEntry = _config->readEntry("Font");
        if (!fontEntry.isEmpty())
            return QVariant(fontEntry).value<QFont>();
        else
            return font;
    }
    else
        return font;
}
QString Profile::defaultWorkingDirectory() const
{
    return _config->readPathEntry("Cwd");
}
#endif

SessionManager::SessionManager()
{
    ProfileReader* kde3Reader = new KDE3ProfileReader;

    //locate default session
    KSharedConfigPtr appConfig = KGlobal::config();
    //KConfig* appConfig = new KConfig("konsolerc");
    const KConfigGroup group = appConfig->group( "Desktop Entry" );

   QString defaultSessionFilename = group.readEntry("DefaultSession","shell.desktop");

    //locate config files and extract the most important properties of them from
    //the config files.
    //
    //the sessions are only parsed completely when a session of this type
    //is actually created
    QList<QString> files = KGlobal::dirs()->findAllResources("data", "konsole/*.desktop", KStandardDirs::NoDuplicates);

    QListIterator<QString> fileIter(files);

    while (fileIter.hasNext())
    {

        QString configFile = fileIter.next();
        Profile* newType = new Profile();
        newType->setProperty(Profile::Path,configFile);

        kde3Reader->readProfile(configFile,newType);

        QString sessionKey = addProfile( newType );

        if ( QFileInfo(configFile).fileName() == defaultSessionFilename )
            _defaultProfile = sessionKey;
    }

    Q_ASSERT( _types.count() > 0 );
    Q_ASSERT( !_defaultProfile.isEmpty() );

    // now that the session types have been loaded,
    // get the list of favorite sessions
    loadFavorites();

    delete kde3Reader;
}

SessionManager::~SessionManager()
{
    QListIterator<Profile*> infoIter(_types.values());

    while (infoIter.hasNext())
        delete infoIter.next();

    // failure to call KGlobal::config()->sync() here results in a crash on exit and
    // configuraton information not being saved to disk.
    // my understanding of the documentation is that KConfig is supposed to save
    // the data automatically when the application exits.  need to discuss this
    // with people who understand KConfig better.
    qWarning() << "Manually syncing configuration information - this should be done automatically.";
    KGlobal::config()->sync();
}

const QList<Session*> SessionManager::sessions()
{
    return _sessions;
}

void SessionManager::pushSessionSettings( const Profile* info )
{
    addSetting( InitialWorkingDirectory , SessionConfig , info->defaultWorkingDirectory() );
    addSetting( ColorScheme , SessionConfig , info->colorScheme() );
}

Session* SessionManager::createSession(QString key )
{
    Session* session = 0;
    
    const Profile* info = 0;

    if ( key.isEmpty() )
        info = defaultProfile();
    else
        info = _types[key];

        if ( true )
        {
            //supply settings from session config
            pushSessionSettings( info );

            //configuration information found, create a new session based on this
            session = new Session();
            session->setType(key);

            QListIterator<QString> iter(info->arguments());
            while (iter.hasNext())
                kDebug() << "running " << info->command() << ": argument " << iter.next() << endl;
           
            session->setInitialWorkingDirectory( activeSetting(InitialWorkingDirectory).toString() ); 
            session->setProgram( info->command() );
            session->setArguments( info->arguments() );
            session->setTitle( info->name() );
            session->setIconName( info->icon() );

            //session->setSchema( _colorSchemeList->find(activeSetting(ColorScheme).toString()) );
            session->setTerminalType( info->terminal() );

                //temporary
                session->setHistory( HistoryTypeBuffer(200) );
                //session->setHistory( HistoryTypeFile() );

            //ask for notification when session dies
            connect( session , SIGNAL(done(Session*)) , SLOT(sessionTerminated(Session*)) );

            //add session to active list
            _sessions << session;

        }

    Q_ASSERT( session );

    return session;
}

void SessionManager::sessionTerminated(Session* session)
{
    _sessions.removeAll(session);
    session->deleteLater();
}

QList<QString> SessionManager::availableProfiles() const
{
    return _types.keys();
}

Profile* SessionManager::profile(const QString& key) const
{
    if ( key.isEmpty() )
        return defaultProfile();

    if ( _types.contains(key) )
        return _types[key];
    else
        return 0;
}

Profile* SessionManager::defaultProfile() const
{
    return _types[_defaultProfile];
}

QString SessionManager::defaultProfileKey() const
{
    return _defaultProfile;
}

void SessionManager::addSetting( Setting setting, Source source, const QVariant& value)
{
    _settings[setting] << SourceVariant(source,value);
}

QVariant SessionManager::activeSetting( Setting setting ) const
{
    QListIterator<SourceVariant>  sourceIter( _settings[setting] );


    Source highestPrioritySource = ApplicationDefault;
    QVariant value;

    while (sourceIter.hasNext())
    {
        QPair<Source,QVariant> sourceSettingPair = sourceIter.next();

        if ( sourceSettingPair.first >= highestPrioritySource )
        {
            value = sourceSettingPair.second;
            highestPrioritySource = sourceSettingPair.first;
        }
    }

    return value;
}

QString SessionManager::addProfile(Profile* type)
{
    QString key;

    for ( int counter = 0;;counter++ )
    {
        if ( !_types.contains(type->path() + QString::number(counter)) )
        {
            key = type->path() + QString::number(counter);
            break;
        }
    }
    
    _types.insert(key,type);

    emit profileAdded(key);

    return key;
}

void SessionManager::deleteProfile(const QString& key)
{
    Profile* type = profile(key);

    setFavorite(key,false);

    if ( type )
    {
        _types.remove(key);
        delete type;
    }

    emit profileRemoved(key); 

    qWarning() << __FUNCTION__ << "TODO: Make this change persistant.";
    //TODO Store this information persistantly
}
void SessionManager::setDefaultProfile(const QString& key)
{
   Q_ASSERT ( _types.contains(key) );

   _defaultProfile = key;

   Profile* info = profile(key);
  
   Q_ASSERT( QFile::exists(info->path()) );
   QFileInfo fileInfo(info->path());

   qDebug() << "setting default session type to " << fileInfo.fileName();

   KConfigGroup group = KGlobal::config()->group("Desktop Entry");
   group.writeEntry("DefaultSession",fileInfo.fileName());
}
QSet<QString> SessionManager::favorites() const
{
    return _favorites;
}
void SessionManager::setFavorite(const QString& key , bool favorite)
{
    Q_ASSERT( _types.contains(key) );

    if ( favorite && !_favorites.contains(key) )
    {
        qDebug() << "adding favorite - " << key;

        _favorites.insert(key);
        emit favoriteStatusChanged(key,favorite);
    
        saveFavorites();
    }
    else if ( !favorite && _favorites.contains(key) )
    {
        qDebug() << "removing favorite - " << key;
        _favorites.remove(key);
        emit favoriteStatusChanged(key,favorite);
    
        saveFavorites();
    }

}
void SessionManager::loadFavorites()
{
    KSharedConfigPtr appConfig = KGlobal::config();
    KConfigGroup favoriteGroup = appConfig->group("Favorite Profiles");

    qDebug() << "loading favorites";

    if ( favoriteGroup.hasKey("Favorites") )
    {
        qDebug() << "found favorites key";
       QStringList list = favoriteGroup.readEntry("Favorites",QStringList());

       qDebug() << "found " << list.count() << "entries";

       QListIterator<QString> lit(list);
       while (lit.hasNext())
           qDebug() << "entry: " << lit.next();

       QHashIterator<QString,Profile*> iter(_types);
       while ( iter.hasNext() )
       {
            iter.next();
            if ( list.contains( iter.value()->name() ) )
                _favorites.insert( iter.key() );
       } 
    }
}
void SessionManager::saveFavorites()
{
    KSharedConfigPtr appConfig = KGlobal::config();
    KConfigGroup favoriteGroup = appConfig->group("Favorite Profiles");

    QStringList names;
    QSetIterator<QString> keyIter(_favorites);
    while ( keyIter.hasNext() )
    {
        const QString& key = keyIter.next();

        Q_ASSERT( _types.contains(key) && profile(key) != 0 );

        names << profile(key)->name();
    }

    favoriteGroup.writeEntry("Favorites",names);
}
void SessionManager::setInstance(SessionManager* instance)
{
    _instance = instance;
}
SessionManager* SessionManager::instance()
{
    return _instance;
}

#include "SessionManager.moc"

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
#include "SessionManager.h"

// Qt
#include <QtCore/QFileInfo>
#include <QtCore/QList>
#include <QtCore/QSignalMapper>
#include <QtCore/QString>

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
#include "ShellCommand.h"

using namespace Konsole;

SessionManager* SessionManager::_instance = 0;

#if 0

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
#endif

SessionManager::SessionManager()
    : _loadedAllProfiles(false)
{
    //map finished() signals from sessions
    _sessionMapper = new QSignalMapper(this);
    connect( _sessionMapper , SIGNAL(mapped(QObject*)) , this ,
            SLOT(sessionTerminated(QObject*)) );

    //load fallback profile
    addProfile( new FallbackProfile );

    //locate and load default profile
    KSharedConfigPtr appConfig = KGlobal::config();
    const KConfigGroup group = appConfig->group( "Desktop Entry" );
    QString defaultSessionFilename = group.readEntry("DefaultProfile","Shell.profile");

    QString path = KGlobal::dirs()->findResource("data","konsole/"+defaultSessionFilename);
    if (!path.isEmpty())
    {
        const QString& key = loadProfile(path);
        if ( !key.isEmpty() )
            _defaultProfile = key;
    }

    Q_ASSERT( _types.count() > 0 );
    Q_ASSERT( !_defaultProfile.isEmpty() );

    // get shortcuts and paths of profiles associated with
    // them - this doesn't load the shortcuts themselves,
    // that is done on-demand.
    loadShortcuts();
}
QString SessionManager::loadProfile(const QString& path)
{
    // check that we have not already loaded this profile
    QHashIterator<QString,Profile*> iter(_types);
    while ( iter.hasNext() )
    {
        iter.next();
        if ( iter.value()->path() == path )
            return iter.key();
    }

    // load the profile
    ProfileReader* reader = 0;
    if ( path.endsWith(".desktop") )
        reader = 0; //new KDE3ProfileReader;
    else
        reader = new KDE4ProfileReader;

    if (!reader)
        return QString();

    Profile* newProfile = new Profile(defaultProfile());
    newProfile->setProperty(Profile::Path,path);

    bool result = reader->readProfile(path,newProfile);

    delete reader;

    if (!result)
    {
        delete newProfile;
        return QString();
    }
    else
        return addProfile(newProfile);
}
void SessionManager::loadAllProfiles()
{
    if ( _loadedAllProfiles )
        return;

    qDebug() << "Loading all profiles";

    KDE3ProfileReader kde3Reader;
    KDE4ProfileReader kde4Reader;

    QStringList profiles;
    profiles += kde3Reader.findProfiles();
    profiles += kde4Reader.findProfiles();
    
    QListIterator<QString> iter(profiles);
    while (iter.hasNext())
        loadProfile(iter.next());

    _loadedAllProfiles = true;
}
SessionManager::~SessionManager()
{    
    // save default profile
    setDefaultProfile( _defaultProfile );

    // save shortcuts
    saveShortcuts();

    // free profiles
    QListIterator<Profile*> infoIter(_types.values());

    while (infoIter.hasNext())
        delete infoIter.next();
}

const QList<Session*> SessionManager::sessions()
{
    return _sessions;
}

void SessionManager::updateSession(Session* session)
{
    Profile* info = profile(session->profileKey());

    Q_ASSERT( info );

    applyProfile(session,info,false);
}

Session* SessionManager::createSession(const QString& key )
{
    Session* session = 0;
    
    const Profile* info = 0;

    if ( key.isEmpty() )
        info = defaultProfile();
    else
        info = _types[key];

    //configuration information found, create a new session based on this
    session = new Session();
    session->setProfileKey(key);
    
    applyProfile(session,info,false);

    //ask for notification when session dies
    _sessionMapper->setMapping(session,session);
    connect( session , SIGNAL(finished()) , _sessionMapper , 
             SLOT(map()) );

    //add session to active list
    _sessions << session;

    Q_ASSERT( session );

    return session;
}

void SessionManager::sessionTerminated(QObject* sessionObject)
{
    Session* session = qobject_cast<Session*>(sessionObject);

    qDebug() << "Session finished: " << session->title(Session::NameRole);

    Q_ASSERT( session );

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

void SessionManager::saveProfile(const QString& path , Profile* info)
{
    ProfileWriter* writer = new KDE4ProfileWriter;

    QString newPath = path;

    if ( newPath.isEmpty() )
        newPath = writer->getPath(info);

    writer->writeProfile(newPath,info);

    delete writer;
}

void SessionManager::changeProfile(const QString& key , 
                                   QHash<Profile::Property,QVariant> propertyMap, bool persistant)
{
    Profile* info = profile(key);

    if (!info || key.isEmpty())
    {
        qWarning() << "Profile for key" << key << "not found.";
        return;
    }

    qDebug() << "Profile about to change: " << info->name();
    
    // insert the changes into the existing Profile instance
    QListIterator<Profile::Property> iter(propertyMap.keys());
    while ( iter.hasNext() )
    {
        const Profile::Property property = iter.next();
        info->setProperty(property,propertyMap[property]);
    }
    
    qDebug() << "Profile changed: " << info->name();

    // apply the changes to existing sessions
    applyProfile(key,true);

    // notify the world about the change
    emit profileChanged(key);

    if ( persistant )
    {
        // save the changes to disk
        // the path may be empty here, in which case it is up
        // to the profile writer to generate or request a path name
        if ( info->isPropertySet(Profile::Path) )
        {
            qDebug() << "Profile saved to existing path: " << info->path();
            saveProfile(info->path(),info);
        }
        else
        {
            qDebug() << "Profile saved to new path.";
            saveProfile(QString(),info);
        }
    }
}
void SessionManager::applyProfile(const QString& key , bool modifiedPropertiesOnly)
{
    Profile* info = profile(key);

    QListIterator<Session*> iter(_sessions);
    while ( iter.hasNext() )
    {
        Session* next = iter.next();
        if ( next->profileKey() == key )
            applyProfile(next,info,modifiedPropertiesOnly);        
    }
}
void SessionManager::applyProfile(Session* session, const Profile* info , bool modifiedPropertiesOnly)
{
    session->setProfileKey( _types.key((Profile*)info) );

    // Basic session settings
    if ( !modifiedPropertiesOnly || info->isPropertySet(Profile::Name) )
        session->setTitle(Session::NameRole,info->name());

    if ( !modifiedPropertiesOnly || info->isPropertySet(Profile::Command) )
        session->setProgram(info->command());

    if ( !modifiedPropertiesOnly || info->isPropertySet(Profile::Arguments) )
        session->setArguments(info->arguments());

    if ( !modifiedPropertiesOnly || info->isPropertySet(Profile::Directory) )
        session->setInitialWorkingDirectory(info->defaultWorkingDirectory());

    if ( !modifiedPropertiesOnly || info->isPropertySet(Profile::Icon) )
        session->setIconName(info->icon());

    // Key bindings
    if ( !modifiedPropertiesOnly || info->isPropertySet(Profile::KeyBindings) )
        session->setKeyBindings(info->property(Profile::KeyBindings).value<QString>());

    // Tab formats
    if ( !modifiedPropertiesOnly || info->isPropertySet(Profile::LocalTabTitleFormat) )
        session->setTabTitleFormat( Session::LocalTabTitle ,
                                    info->property(Profile::LocalTabTitleFormat).value<QString>());
    if ( !modifiedPropertiesOnly || info->isPropertySet(Profile::RemoteTabTitleFormat) )
        session->setTabTitleFormat( Session::RemoteTabTitle ,
                                    info->property(Profile::RemoteTabTitleFormat).value<QString>());

    // Scrollback / history
    if ( !modifiedPropertiesOnly 
         || info->isPropertySet(Profile::HistoryMode) 
         || info->isPropertySet(Profile::HistorySize) )
    {
        int mode = info->property(Profile::HistoryMode).value<int>();
        switch ((Profile::HistoryModeEnum)mode)
        {
            case Profile::DisableHistory:
                    session->setHistoryType( HistoryTypeNone() );
                break;
            case Profile::FixedSizeHistory:
                {
                    int lines = info->property(Profile::HistorySize).value<int>();
                    session->setHistoryType( HistoryTypeBuffer(lines) );
                }
                break;
            case Profile::UnlimitedHistory:
                    session->setHistoryType( HistoryTypeFile() );
                break;
        }
    }

    // Terminal features
    if ( !modifiedPropertiesOnly || info->isPropertySet(Profile::FlowControlEnabled) )
        session->setFlowControlEnabled( info->property(Profile::FlowControlEnabled)
                .value<bool>() );
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

    if ( _types.isEmpty() )
        _defaultProfile = key;
 
    _types.insert(key,type);

    emit profileAdded(key);

    return key;
}

void SessionManager::deleteProfile(const QString& key)
{
    Profile* type = profile(key);

    setFavorite(key,false);

    bool wasDefault = ( type == defaultProfile() );

    if ( type )
    {
        // try to delete the config file
        if ( type->isPropertySet(Profile::Path) && QFile::exists(type->path()) )
        {
            if (!QFile::remove(type->path()))
            {
                qWarning() << "Could not delete config file: " << type->path()
                    << "The file is most likely in a directory which is read-only.";
                qWarning() << "TODO: Hide this file instead.";
            }
        }

        _types.remove(key);
        delete type;
    }

    // if we just deleted the default session type,
    // replace it with the first type in the list
    if ( wasDefault )
    {
        setDefaultProfile( _types.keys().first() );
    }

    emit profileRemoved(key); 
}
void SessionManager::setDefaultProfile(const QString& key)
{
   Q_ASSERT ( _types.contains(key) );

   _defaultProfile = key;

   Profile* info = profile(key);

   QString path = info->path();  
   
   if ( path.isEmpty() )
       path = KDE4ProfileWriter().getPath(info);

   QFileInfo fileInfo(path);

   qDebug() << "setting default session type to " << fileInfo.fileName();

   KConfigGroup group = KGlobal::config()->group("Desktop Entry");
   group.writeEntry("DefaultProfile",fileInfo.fileName());
}
QSet<QString> SessionManager::findFavorites() 
{
    if (_favorites.isEmpty())
        loadFavorites();

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
void SessionManager::loadShortcuts()
{
    KSharedConfigPtr appConfig = KGlobal::config();
    KConfigGroup shortcutGroup = appConfig->group("Profile Shortcuts");

    QMap<QString,QString> entries = shortcutGroup.entryMap();

    QMapIterator<QString,QString> iter(entries);
    while ( iter.hasNext() )
    {
        iter.next();

        QKeySequence shortcut = QKeySequence::fromString(iter.key());
        QString profilePath = iter.value();

        ShortcutData data;
        data.profilePath = profilePath;

        _shortcuts.insert(shortcut,data);
    }
}
void SessionManager::saveShortcuts()
{
    KSharedConfigPtr appConfig = KGlobal::config();
    KConfigGroup shortcutGroup = appConfig->group("Profile Shortcuts");
    shortcutGroup.deleteGroup();

    QMapIterator<QKeySequence,ShortcutData> iter(_shortcuts);
    while ( iter.hasNext() )
    {
        iter.next();

        QString shortcutString = iter.key().toString();

        shortcutGroup.writeEntry(shortcutString,
                iter.value().profilePath);
    }    
}
void SessionManager::setShortcut(const QString& profileKey , 
                                 const QKeySequence& keySequence )
{
    QKeySequence existingShortcut = shortcut(profileKey);
    _shortcuts.remove(existingShortcut);

    ShortcutData data;
    data.profileKey = profileKey;
    data.profilePath = profile(profileKey)->path();
    // TODO - This won't work if the profile doesn't 
    // have a path yet
    _shortcuts.insert(keySequence,data);
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

        QSet<QString> favoriteSet = QSet<QString>::fromList(list);

       // look for favorites amongst those already loaded
       QHashIterator<QString,Profile*> iter(_types);
       while ( iter.hasNext() )
       {
            iter.next();
            const QString& path = iter.value()->path();
            if ( favoriteSet.contains( path ) )
            {
                _favorites.insert( iter.key() );
                favoriteSet.remove(path);
            }
       }
      // load any remaining favorites
      QSetIterator<QString> unloadedFavoriteIter(favoriteSet);
      while ( unloadedFavoriteIter.hasNext() )
      {
            const QString& key = loadProfile(unloadedFavoriteIter.next());
            if (!key.isEmpty())
                _favorites.insert(key);
      } 
    }
}
void SessionManager::saveFavorites()
{
    KSharedConfigPtr appConfig = KGlobal::config();
    KConfigGroup favoriteGroup = appConfig->group("Favorite Profiles");

    QStringList paths;
    QSetIterator<QString> keyIter(_favorites);
    while ( keyIter.hasNext() )
    {
        const QString& key = keyIter.next();

        Q_ASSERT( _types.contains(key) && profile(key) != 0 );

        paths << profile(key)->path();
    }

    favoriteGroup.writeEntry("Favorites",paths);
}

QList<QKeySequence> SessionManager::shortcuts() 
{
    return _shortcuts.keys();
}

QString SessionManager::findByShortcut(const QKeySequence& shortcut)
{
    Q_ASSERT( _shortcuts.contains(shortcut) );

    if ( _shortcuts[shortcut].profileKey.isEmpty() )
    {
        QString key = loadProfile(_shortcuts[shortcut].profilePath);
        _shortcuts[shortcut].profileKey = key;
    }

    return _shortcuts[shortcut].profileKey;
}

QKeySequence SessionManager::shortcut(const QString& profileKey) const
{
    Profile* info = profile(profileKey);

    QMapIterator<QKeySequence,ShortcutData> iter(_shortcuts);
    while (iter.hasNext())
    {
        iter.next();
        if ( iter.value().profileKey == profileKey 
             || iter.value().profilePath == info->path() )
            return iter.key();
    }
    // dummy
    return QKeySequence(Qt::CTRL+Qt::ALT+Qt::Key_B);
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

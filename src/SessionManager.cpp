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
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QList>
#include <QtCore/QSignalMapper>
#include <QtCore/QStack>
#include <QtCore/QString>
#include <QtCore/QTextCodec>

// KDE
#include <klocale.h>
#include <krun.h>
#include <kshell.h>
#include <kconfig.h>
#include <kglobal.h>
#include <kdebug.h>
#include <kconfiggroup.h>
#include <kstandarddirs.h>
#include <kdesktopfile.h>

// Konsole
#include "ColorScheme.h"
#include "Session.h"
#include "History.h"
#include "ShellCommand.h"

using namespace Konsole;

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
QString SessionManager::loadProfile(const QString& shortPath)
{
    QString path = shortPath;

    // add a suggested suffix and relative prefix if missing
    QFileInfo fileInfo(path);
    if ( fileInfo.suffix().isEmpty() )
        path.append(".profile");
    if ( fileInfo.path().isEmpty() || fileInfo.path() == "." )
        path.prepend(QString("konsole")+QDir::separator());

    // if the file is not an absolute path, look it up 
    if ( !fileInfo.isAbsolute() )
        path = KStandardDirs::locate("data",path);

	// guard to prevent problems if a profile specifies itself as its parent
	// or if there is recursion in the "inheritance" chain
	// (eg. two profiles, A and B, specifying each other as their parents)
	static QStack<QString> recursionGuard;
	if (recursionGuard.contains(path))
		return QString();
	else
		recursionGuard.push(path);


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
        reader = 0; // new KDE3ProfileReader;
    else
        reader = new KDE4ProfileReader;

    if (!reader)
	{
		recursionGuard.pop();
        return QString();
	}

    Profile* newProfile = new Profile(defaultProfile());
    newProfile->setProperty(Profile::Path,path);

    QString parentProfile;
    bool result = reader->readProfile(path,newProfile,parentProfile);

    if ( !parentProfile.isEmpty() )
    {
        QString parentKey = loadProfile(parentProfile);
        newProfile->setParent(profile(parentKey));
    }

    delete reader;
	recursionGuard.pop();

    if (!result)
    {
        delete newProfile;
        return QString();
    }
    else
        return addProfile(newProfile);
}
QList<QString> SessionManager::availableProfilePaths() const
{
    KDE4ProfileReader reader;

    return reader.findProfiles();    
}

void SessionManager::loadAllProfiles()
{
    if ( _loadedAllProfiles )
        return;

    kDebug() << "Loading all profiles";

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

    // save favorites
    saveFavorites();

    // delete remaining sessions
    foreach( Session* session , _sessions )
    {
        session->close();
        delete session;
    }
    
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

    // FIXME - This may update a lot more than just the session
    // of interest. 
    emit sessionUpdated(session);
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

    connect( session , SIGNAL(profileChanged(const QString&)) , this , 
            SLOT(sessionProfileChanged()) );

    connect( session , SIGNAL(profileChangeCommandReceived(const QString&)) , this ,
            SLOT(sessionProfileCommandReceived(const QString&)) );

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

    //kDebug() << "Session finished: " << session->title(Session::NameRole);

    Q_ASSERT( session );

    _sessions.removeAll(session);
    session->deleteLater();
}

QList<QString> SessionManager::loadedProfiles() const
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

QString SessionManager::saveProfile(Profile* info)
{
    ProfileWriter* writer = new KDE4ProfileWriter;

    QString newPath = writer->getPath(info);

    writer->writeProfile(newPath,info);

    delete writer;

    return newPath;
}

void SessionManager::changeProfile(const QString& key , 
                                   QHash<Profile::Property,QVariant> propertyMap, bool persistant)
{
    Profile* info = profile(key);

    if (!info || key.isEmpty())
    {
        kWarning() << "Profile for key" << key << "not found.";
        return;
    }

    kDebug() << "Profile about to change: " << info->name();
    
    // insert the changes into the existing Profile instance
    QListIterator<Profile::Property> iter(propertyMap.keys());
    while ( iter.hasNext() )
    {
        const Profile::Property property = iter.next();
        info->setProperty(property,propertyMap[property]);
    }
    
    kDebug() << "Profile changed: " << info->name();

    // apply the changes to existing sessions
    applyProfile(key,true);

    // notify the world about the change
    emit profileChanged(key);

	// save changes to disk, unless the profile is hidden, in which case
	// it has no file on disk 
    if ( persistant && !info->isHidden() )
	{
        info->setProperty(Profile::Path,saveProfile(info));
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
    QString key = _types.key((Profile*)info);
    if ( session->profileKey() != key )
        session->setProfileKey(key); 

	ShouldApplyProperty apply(info,modifiedPropertiesOnly);

    // Basic session settings
	if ( apply.shouldApply(Profile::Name) )
		session->setTitle(Session::NameRole,info->name());

    if ( apply.shouldApply(Profile::Command) )
        session->setProgram(info->command());

    if ( apply.shouldApply(Profile::Arguments) )
        session->setArguments(info->arguments());

    if ( apply.shouldApply(Profile::Directory) )
        session->setInitialWorkingDirectory(info->defaultWorkingDirectory());

    if ( apply.shouldApply(Profile::Environment) )
        session->setEnvironment(info->property<QStringList>(Profile::Environment));

    if ( apply.shouldApply(Profile::Icon) )
        session->setIconName(info->icon());

    // Key bindings
    if ( apply.shouldApply(Profile::KeyBindings) )
        session->setKeyBindings(info->property<QString>(Profile::KeyBindings));

    // Tab formats
    if ( apply.shouldApply(Profile::LocalTabTitleFormat) )
        session->setTabTitleFormat( Session::LocalTabTitle ,
                                    info->property<QString>(Profile::LocalTabTitleFormat));
    if ( apply.shouldApply(Profile::RemoteTabTitleFormat) )
        session->setTabTitleFormat( Session::RemoteTabTitle ,
                                    info->property<QString>(Profile::RemoteTabTitleFormat));

    // Scrollback / history
    if ( apply.shouldApply(Profile::HistoryMode) || apply.shouldApply(Profile::HistorySize) ) 
    {
        int mode = info->property<int>(Profile::HistoryMode);
        switch ((Profile::HistoryModeEnum)mode)
        {
            case Profile::DisableHistory:
                    session->setHistoryType( HistoryTypeNone() );
                break;
            case Profile::FixedSizeHistory:
                {
                    int lines = info->property<int>(Profile::HistorySize);
                    session->setHistoryType( HistoryTypeBuffer(lines) );
                }
                break;
            case Profile::UnlimitedHistory:
                    session->setHistoryType( HistoryTypeFile() );
                break;
        }
    }

    // Terminal features
    if ( apply.shouldApply(Profile::FlowControlEnabled) )
        session->setFlowControlEnabled( info->property<bool>(Profile::FlowControlEnabled) );

    // Encoding
    if ( apply.shouldApply(Profile::DefaultEncoding) )
    {
        QByteArray name = info->property<QString>(Profile::DefaultEncoding).toUtf8();
        session->setCodec( QTextCodec::codecForName(name) );
    } 
}

QString SessionManager::addProfile(Profile* type)
{
    QString key;

    // choose a key for the Profile
    for ( int counter = 0;;counter++ )
    {
        //if ( !_types.contains(type->path() + QString::number(counter)) )
        
        if ( !_types.contains(type->name() + QString::number(counter)) )
        {
            key = type->name() + QString::number(counter);
            break;
        }
    }

    if ( _types.isEmpty() )
        _defaultProfile = key;
 
    _types.insert(key,type);

    emit profileAdded(key);

    return key;
}

bool SessionManager::deleteProfile(const QString& key)
{
    Profile* type = profile(key);

    bool wasDefault = ( type == defaultProfile() );

    if ( type )
    {
        // try to delete the config file
        if ( type->isPropertySet(Profile::Path) && QFile::exists(type->path()) )
        {
            if (!QFile::remove(type->path()))
            {
                kWarning() << "Could not delete profile: " << type->path()
                    << "The file is most likely in a directory which is read-only.";

                return false;
            }
        }

        setFavorite(key,false);
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

    return true; 
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

   kDebug() << "setting default session type to " << fileInfo.fileName();

   KSharedConfigPtr config = KGlobal::config();
   KConfigGroup group = config->group("Desktop Entry");
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
        kDebug() << "adding favorite - " << key;

        _favorites.insert(key);
        emit favoriteStatusChanged(key,favorite);
    }
    else if ( !favorite && _favorites.contains(key) )
    {
        kDebug() << "removing favorite - " << key;
        _favorites.remove(key);
        emit favoriteStatusChanged(key,favorite);
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

	emit shortcutChanged(profileKey,keySequence);
}
void SessionManager::loadFavorites()
{
    KSharedConfigPtr appConfig = KGlobal::config();
    KConfigGroup favoriteGroup = appConfig->group("Favorite Profiles");

    QSet<QString> favoriteSet;

    if ( favoriteGroup.hasKey("Favorites") )
    {
       QStringList list = favoriteGroup.readEntry("Favorites", QStringList());
       favoriteSet = QSet<QString>::fromList(list);
    }
    else
    {
       // if there is no favorites key at all, mark the 
       // supplied 'Shell.profile' as the only favorite
       favoriteSet << "Shell.profile";
    }

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

void SessionManager::sessionProfileChanged()
{
    Session* session = qobject_cast<Session*>(sender());
    Q_ASSERT( session );

    updateSession(session); 
}

void SessionManager::sessionProfileCommandReceived(const QString& text)
{
    // FIXME: This is inefficient, it creates a new profile instance for
    // each set of changes applied.  Instead a new profile should be created
    // only the first time changes are applied to a session

    Session* session = qobject_cast<Session*>(sender());
    Q_ASSERT( session );

    ProfileCommandParser parser;
    QHash<Profile::Property,QVariant> changes = parser.parse(text);

    Profile* newProfile = new Profile( profile(session->profileKey()) );
    
    QHashIterator<Profile::Property,QVariant> iter(changes);
    while ( iter.hasNext() )
    {
        iter.next();
        newProfile->setProperty(iter.key(),iter.value());
    } 

    session->setProfileKey( addProfile(newProfile) );
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
    
    return QKeySequence();
}


K_GLOBAL_STATIC( SessionManager , theSessionManager )
SessionManager* SessionManager::instance()
{
	return theSessionManager;
}


#include "SessionManager.moc"

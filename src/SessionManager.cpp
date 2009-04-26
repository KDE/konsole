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
#include "SessionManager.h"

// Qt
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QList>
#include <QtCore/QSignalMapper>
#include <QtCore/QString>
#include <QtCore/QTextCodec>

// KDE
#include <klocale.h>
#include <kicon.h>
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

bool profileIndexLessThan(const Profile::Ptr &p1, const Profile::Ptr &p2)
{
    return p1->menuIndexAsInt() <= p2->menuIndexAsInt();
}

bool profileNameLessThan(const Profile::Ptr &p1, const Profile::Ptr &p2)
{
    return QString::localeAwareCompare(p1->name(), p2->name()) <= 0;
}

static void sortByIndexProfileList(QList<Profile::Ptr> &list)
{
   qStableSort(list.begin(), list.end(), profileIndexLessThan);
}

static void sortByNameProfileList(QList<Profile::Ptr> &list)
{
    qStableSort(list.begin(), list.end(), profileNameLessThan);
}

SessionManager::SessionManager()
    : _loadedAllProfiles(false)
    , _loadedFavorites(false)
{
    //map finished() signals from sessions
    _sessionMapper = new QSignalMapper(this);
    connect( _sessionMapper , SIGNAL(mapped(QObject*)) , this ,
            SLOT(sessionTerminated(QObject*)) );

    //load fallback profile
    _fallbackProfile = Profile::Ptr(new FallbackProfile);
    addProfile(_fallbackProfile);

    //locate and load default profile
    KSharedConfigPtr appConfig = KSharedConfig::openConfig("konsolerc");
    const KConfigGroup group = appConfig->group( "Desktop Entry" );
    QString defaultSessionFilename = group.readEntry("DefaultProfile","Shell.profile");

    QString path = KGlobal::dirs()->findResource("data","konsole/"+defaultSessionFilename);
    if (!path.isEmpty())
    {
        Profile::Ptr profile = loadProfile(path);
        if ( profile )
            _defaultProfile = profile;
    }

    Q_ASSERT( _types.count() > 0 );
    Q_ASSERT( _defaultProfile );

    // get shortcuts and paths of profiles associated with
    // them - this doesn't load the shortcuts themselves,
    // that is done on-demand.
    loadShortcuts();
}
Profile::Ptr SessionManager::loadProfile(const QString& shortPath)
{
    // the fallback profile has a 'special' path name, "FALLBACK/"
    if (shortPath == _fallbackProfile->property<QString>(Profile::Path))
        return _fallbackProfile;

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

    // check that we have not already loaded this profile
    QSetIterator<Profile::Ptr> iter(_types);
    while ( iter.hasNext() )
    {
        Profile::Ptr profile = iter.next();
        if ( profile->path() == path )
            return profile;
    }

    // guard to prevent problems if a profile specifies itself as its parent
    // or if there is recursion in the "inheritance" chain
    // (eg. two profiles, A and B, specifying each other as their parents)
    static QStack<QString> recursionGuard;
    PopStackOnExit<QString> popGuardOnExit(recursionGuard);

    if (recursionGuard.contains(path))
    {
        kWarning() << "Ignoring attempt to load profile recursively from" << path;
        return _fallbackProfile;
    }
    else
        recursionGuard.push(path);

    // load the profile
    ProfileReader* reader = 0;
    if ( path.endsWith(QLatin1String(".desktop")) )
        reader = 0; // new KDE3ProfileReader;
    else
        reader = new KDE4ProfileReader;

    if (!reader)
    {
        kWarning() << "Could not create loader to read profile from" << path;
        return Profile::Ptr();
    }

    Profile::Ptr newProfile = Profile::Ptr(new Profile(defaultProfile()));
    newProfile->setProperty(Profile::Path,path);

    QString parentProfilePath;
    bool result = reader->readProfile(path,newProfile,parentProfilePath);

    if ( !parentProfilePath.isEmpty() )
    {
        Profile::Ptr parentProfile = loadProfile(parentProfilePath);
        newProfile->setParent(parentProfile);
    }

    delete reader;

    if (!result)
    {
        kWarning() << "Could not load profile from " << path;
        return Profile::Ptr();
    }
    else
    {
        addProfile(newProfile);
        return newProfile;
    }
}
QStringList SessionManager::availableProfilePaths() const
{
    KDE3ProfileReader kde3Reader;
    KDE4ProfileReader kde4Reader;

    QStringList profiles;
    profiles += kde3Reader.findProfiles();
    profiles += kde4Reader.findProfiles();

    return profiles;
}

void SessionManager::loadAllProfiles()
{
    if ( _loadedAllProfiles )
        return;

    QStringList profiles = availableProfilePaths();
    
    QListIterator<QString> iter(profiles);
    while (iter.hasNext())
        loadProfile(iter.next());

    _loadedAllProfiles = true;
}

void SessionManager::sortProfiles(QList<Profile::Ptr> &list)
{

    QList<Profile::Ptr> lackingIndices;
    QList<Profile::Ptr> havingIndices;

    for (int i = 0; i < list.size(); ++i)
    {
        // dis-regard the fallback profile
        if (list.at(i)->path() == _fallbackProfile->property<QString>(Profile::Path))
            continue;

        if (list.at(i)->menuIndexAsInt() == 0)
            lackingIndices.append(list.at(i));
        else
            havingIndices.append(list.at(i));
    }

    // sort by index
    sortByIndexProfileList(havingIndices);

    // sort alphabetically those w/o an index
    sortByNameProfileList(lackingIndices);

    // Put those with indices in sequential order w/o any gaps
    int i = 0;
    for (i = 0; i < havingIndices.size(); ++i)
    {
        Profile::Ptr tempProfile = havingIndices.at(i);
        tempProfile->setProperty(Profile::MenuIndex, QString::number(i+1));
        havingIndices.replace(i, tempProfile);
    }
    // Put those w/o indices in sequential order
    for (int j = 0; j < lackingIndices.size(); ++j)
    {
        Profile::Ptr tempProfile = lackingIndices.at(j);
        tempProfile->setProperty(Profile::MenuIndex, QString::number(j+1+i));
        lackingIndices.replace(j, tempProfile);
    }

    // combine the 2 list: first those who had indices
    list.clear();
    list.append(havingIndices);
    list.append(lackingIndices);
}

void SessionManager::saveState()
{
    // save default profile
    setDefaultProfile( _defaultProfile );

    // save shortcuts
    saveShortcuts();

    // save favorites
    saveFavorites();
}
void SessionManager::closeAll()
{
    // close remaining sessions
    foreach( Session* session , _sessions )
    {
        session->close();
    }
    _sessions.clear();    
}
SessionManager::~SessionManager()
{
    if (_sessions.count() > 0)
    {
        kWarning() << "Konsole SessionManager destroyed with sessions still alive";
        // ensure that the Session doesn't later try to call back and do things to the 
        // SessionManager
        foreach(Session* session , _sessions)
            disconnect(session , 0 , this , 0);
    }
}

const QList<Session*> SessionManager::sessions()
{
    return _sessions;
}

void SessionManager::updateSession(Session* session)
{
    Profile::Ptr info = _sessionProfiles[session]; 

    Q_ASSERT( info );

    applyProfile(session,info,false);

    // FIXME - This may update a lot more than just the session
    // of interest. 
    emit sessionUpdated(session);
}

Session* SessionManager::createSession(Profile::Ptr info)
{
    Session* session = 0;
    
    if (!info)
        info = defaultProfile();
   
    if (!_types.contains(info))
        addProfile(info);

    //configuration information found, create a new session based on this
    session = new Session();
    applyProfile(session,info,false);

    connect( session , SIGNAL(profileChangeCommandReceived(QString)) , this ,
            SLOT(sessionProfileCommandReceived(QString)) );

    //ask for notification when session dies
    _sessionMapper->setMapping(session,session);
    connect( session , SIGNAL(finished()) , _sessionMapper , 
             SLOT(map()) );

    //add session to active list
    _sessions << session;
    _sessionProfiles.insert(session,info);

    Q_ASSERT( session );

    return session;
}

void SessionManager::sessionTerminated(QObject* sessionObject)
{
    Session* session = qobject_cast<Session*>(sessionObject);

    Q_ASSERT( session );

    _sessions.removeAll(session);
    session->deleteLater();
}

QList<Profile::Ptr> SessionManager::sortedFavorites()
{
    QList<Profile::Ptr> favorites = findFavorites().toList();

    sortProfiles(favorites);
    return favorites;
}

QList<Profile::Ptr> SessionManager::loadedProfiles() const
{
    return _types.toList();
}

Profile::Ptr SessionManager::defaultProfile() const
{
    return _defaultProfile;
}
Profile::Ptr SessionManager::fallbackProfile() const
{ return _fallbackProfile; }

QString SessionManager::saveProfile(Profile::Ptr info)
{
    ProfileWriter* writer = new KDE4ProfileWriter;

    QString newPath = writer->getPath(info);

    writer->writeProfile(newPath,info);

    delete writer;

    return newPath;
}

void SessionManager::changeProfile(Profile::Ptr info , 
                                   QHash<Profile::Property,QVariant> propertyMap, bool persistant)
{
    Q_ASSERT(info); 

    // insert the changes into the existing Profile instance
    QListIterator<Profile::Property> iter(propertyMap.keys());
    while ( iter.hasNext() )
    {
        const Profile::Property property = iter.next();
        info->setProperty(property,propertyMap[property]);
    }
    
    // when changing a group, iterate through the profiles
    // in the group and call changeProfile() on each of them
    //
    // this is so that each profile in the group, the profile is 
    // applied, a change notification is emitted and the profile
    // is saved to disk
    ProfileGroup::Ptr group = info->asGroup();
    if (group)
    {
        foreach(const Profile::Ptr &profile, group->profiles())
            changeProfile(profile,propertyMap,persistant);
        return;
    }
    
    // apply the changes to existing sessions
    applyProfile(info,true);

    // notify the world about the change
    emit profileChanged(info);

    // save changes to disk, unless the profile is hidden, in which case
    // it has no file on disk 
    if ( persistant && !info->isHidden() )
    {
        info->setProperty(Profile::Path,saveProfile(info));
    }
}
void SessionManager::applyProfile(Profile::Ptr info , bool modifiedPropertiesOnly)
{
    QListIterator<Session*> iter(_sessions);
    while ( iter.hasNext() )
    {
        Session* next = iter.next();
        if ( _sessionProfiles[next] == info )
            applyProfile(next,info,modifiedPropertiesOnly);        
    }
}
Profile::Ptr SessionManager::sessionProfile(Session* session) const
{
    return _sessionProfiles[session];
}
void SessionManager::setSessionProfile(Session* session, Profile::Ptr profile)
{
    _sessionProfiles[session] = profile;
    updateSession(session);
}
void SessionManager::applyProfile(Session* session, const Profile::Ptr info , bool modifiedPropertiesOnly)
{
    Q_ASSERT(info);

    _sessionProfiles[session] = info;

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
    {
        // add environment variable containing home directory of current profile
        // (if specified)
        QStringList environment = info->property<QStringList>(Profile::Environment);
        environment << QString("PROFILEHOME=%1").arg(info->defaultWorkingDirectory());

        session->setEnvironment(environment);
    }

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

void SessionManager::addProfile(Profile::Ptr type)
{
    if ( _types.isEmpty() )
        _defaultProfile = type;
 
    _types.insert(type);

    emit profileAdded(type);
}

bool SessionManager::deleteProfile(Profile::Ptr type)
{
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

        // remove from favorites, profile list, shortcut list etc.
        setFavorite(type,false);
        setShortcut(type,QKeySequence());
        _types.remove(type);

        // mark the profile as hidden so that it does not show up in the 
        // Manage Profiles dialog and is not saved to disk
        type->setHidden(true);
    }

    // if we just deleted the default session type,
    // replace it with a random type from the list
    if ( wasDefault )
    {
        setDefaultProfile( _types.toList().first() );
    }

    emit profileRemoved(type);

    return true; 
}
void SessionManager::setDefaultProfile(Profile::Ptr info)
{
   Q_ASSERT ( _types.contains(info) );

   _defaultProfile = info;

   QString path = info->path();  
   
   if ( path.isEmpty() )
       path = KDE4ProfileWriter().getPath(info);

   QFileInfo fileInfo(path);

   KSharedConfigPtr config = KGlobal::config();
   KConfigGroup group = config->group("Desktop Entry");
   group.writeEntry("DefaultProfile",fileInfo.fileName());
}
QSet<Profile::Ptr> SessionManager::findFavorites() 
{
    if (!_loadedFavorites)
        loadFavorites();

    return _favorites;
}
void SessionManager::setFavorite(Profile::Ptr info , bool favorite)
{
    if (!_types.contains(info))
        addProfile(info);

    if ( favorite && !_favorites.contains(info) )
    {
        _favorites.insert(info);
        emit favoriteStatusChanged(info,favorite);
    }
    else if ( !favorite && _favorites.contains(info) )
    {
        _favorites.remove(info);
        emit favoriteStatusChanged(info,favorite);
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
void SessionManager::setShortcut(Profile::Ptr info , 
                                 const QKeySequence& keySequence )
{
    QKeySequence existingShortcut = shortcut(info);
    _shortcuts.remove(existingShortcut);

    if (keySequence.isEmpty())
        return;

    ShortcutData data;
    data.profileKey = info;
    data.profilePath = info->path();
    // TODO - This won't work if the profile doesn't 
    // have a path yet
    _shortcuts.insert(keySequence,data);

    emit shortcutChanged(info,keySequence);
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
    QSetIterator<Profile::Ptr> iter(_types);
    while ( iter.hasNext() )
    {
         Profile::Ptr profile = iter.next();
         const QString& path = profile->path();
         if ( favoriteSet.contains( path ) )
         {
             _favorites.insert( profile );
             favoriteSet.remove(path);
         }
    }
    // load any remaining favorites
    QSetIterator<QString> unloadedFavoriteIter(favoriteSet);
    while ( unloadedFavoriteIter.hasNext() )
    {
          Profile::Ptr profile = loadProfile(unloadedFavoriteIter.next());
          if (profile)
              _favorites.insert(profile);
    }

    _loadedFavorites = true;
}
void SessionManager::saveFavorites()
{
    KSharedConfigPtr appConfig = KGlobal::config();
    KConfigGroup favoriteGroup = appConfig->group("Favorite Profiles");

    QStringList paths;
    QSetIterator<Profile::Ptr> keyIter(_favorites);
    while ( keyIter.hasNext() )
    {
        Profile::Ptr profile = keyIter.next();

        Q_ASSERT( _types.contains(profile) && profile );

        paths << profile->path();
    }

    favoriteGroup.writeEntry("Favorites",paths);
}

QList<QKeySequence> SessionManager::shortcuts() 
{
    return _shortcuts.keys();
}

Profile::Ptr SessionManager::findByShortcut(const QKeySequence& shortcut)
{
    Q_ASSERT( _shortcuts.contains(shortcut) );

    if ( !_shortcuts[shortcut].profileKey )
    {
        Profile::Ptr key = loadProfile(_shortcuts[shortcut].profilePath);
        if (!key)
        {
            _shortcuts.remove(shortcut);
            return Profile::Ptr();
        }
        _shortcuts[shortcut].profileKey = key;
    }

    return _shortcuts[shortcut].profileKey;
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

    Profile::Ptr newProfile = Profile::Ptr(new Profile(_sessionProfiles[session]));
    
    QHashIterator<Profile::Property,QVariant> iter(changes);
    while ( iter.hasNext() )
    {
        iter.next();
        newProfile->setProperty(iter.key(),iter.value());
    } 

    _sessionProfiles[session] = newProfile;
    applyProfile(newProfile,true);
    emit sessionUpdated(session);
}

QKeySequence SessionManager::shortcut(Profile::Ptr info) const
{
    QMapIterator<QKeySequence,ShortcutData> iter(_shortcuts);
    while (iter.hasNext())
    {
        iter.next();
        if ( iter.value().profileKey == info 
             || iter.value().profilePath == info->path() )
            return iter.key();
    }
    
    return QKeySequence();
}

void SessionManager::saveSessions(KConfig* config)
{
    // The session IDs can't be restored.
    // So we need to map the old ID to the future new ID.
    int n = 1;
    _restoreMapping.clear();

    foreach(Session* session, _sessions)
    {
        QString name = QLatin1String("Session") + QString::number(n);
        KConfigGroup group(config, name);

        group.writePathEntry("Profile",
                             _sessionProfiles.value(session)->path());
        session->saveSession(group);
        _restoreMapping.insert(session, n);
        n++;
    }

    KConfigGroup group(config, "Number");
    group.writeEntry("NumberOfSessions", _sessions.count());
}

int SessionManager::getRestoreId(Session* session)
{
    return _restoreMapping.value(session);
}

void SessionManager::restoreSessions(KConfig* config)
{
    KConfigGroup group(config, "Number");
    int sessions;

    // Any sessions saved?
    if ((sessions = group.readEntry("NumberOfSessions", 0)) > 0)
    {
        for (int n = 1; n <= sessions; n++)
        {
            QString name = QLatin1String("Session") + QString::number(n);
            KConfigGroup sessionGroup(config, name);

            QString profile = sessionGroup.readPathEntry("Profile", QString());
            Profile::Ptr ptr = defaultProfile();
            if (!profile.isEmpty()) ptr = loadProfile(profile);

            Session* session = createSession(ptr);
            session->restoreSession(sessionGroup);
        }
    }
}

Session* SessionManager::idToSession(int id)
{
    Q_ASSERT(id); 
    foreach(Session* session, _sessions)
        if (session->sessionId() == id)
            return session;

    // this should not happen
    Q_ASSERT(0);
    return 0;
}

K_GLOBAL_STATIC( SessionManager , theSessionManager )
SessionManager* SessionManager::instance()
{
    return theSessionManager;
}

SessionListModel::SessionListModel(QObject* parent)
: QAbstractListModel(parent)
{
}

void SessionListModel::setSessions(const QList<Session*>& sessions)
{
    _sessions = sessions;

    foreach(Session* session, sessions)
        connect(session,SIGNAL(finished()),this,SLOT(sessionFinished()));

    reset();
}
QVariant SessionListModel::data(const QModelIndex& index, int role) const
{
    Q_ASSERT(index.isValid());
    
    int row = index.row();
    int column = index.column();

    Q_ASSERT( row >= 0 && row < _sessions.count() );
    Q_ASSERT( column >= 0 && column < 2 );

    switch (role)
    {
        case Qt::DisplayRole:
            if (column == 1)
                return _sessions[row]->title(Session::DisplayedTitleRole);
            else if (column == 0)
                return _sessions[row]->sessionId();
            break;
        case Qt::DecorationRole:
            if (column == 1)
                return KIcon(_sessions[row]->iconName());
            else
                return QVariant();
    }

    return QVariant();
}
QVariant SessionListModel::headerData(int section, Qt::Orientation orientation, 
                        int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Vertical)
        return QVariant();
    else
    {
        switch (section)
        {
            case 0:
                return i18n("Number");
            case 1:
                return i18n("Title");
            default:
                return QVariant();
        }    
    }
}

int SessionListModel::columnCount(const QModelIndex&) const
{
    return 2;
}
int SessionListModel::rowCount(const QModelIndex&) const
{
    return _sessions.count();
}
QModelIndex SessionListModel::parent(const QModelIndex&) const
{
    return QModelIndex();
}
void SessionListModel::sessionFinished()
{
    Session* session = qobject_cast<Session*>(sender());
    int row = _sessions.indexOf(session);
    
    if (row != -1)
    {
        beginRemoveRows(QModelIndex(),row,row);
        sessionRemoved(session);
        _sessions.removeAt(row);
        endRemoveRows();
    }
}
QModelIndex SessionListModel::index(int row, int column, const QModelIndex& parent) const
{
    if (hasIndex(row,column,parent))
        return createIndex(row,column,_sessions[row]);
    else
        return QModelIndex();
}

#include "SessionManager.moc"


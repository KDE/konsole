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

SessionInfo::SessionInfo(const QString& path)
{
    //QString fileName = QFileInfo(path).fileName();

    //QString fullPath = KStandardDirs::locate("data","konsole/"+fileName);
    //Q_ASSERT( QFile::exists(fullPath) );

    _desktopFile = new KDesktopFile(path);
    _config = new KConfigGroup( _desktopFile->desktopGroup() );

    _path = path;

}
SessionInfo::~SessionInfo()
{
    delete _config; _config = 0;
    delete _desktopFile; _desktopFile = 0;
}

void SessionInfo::setParent( SessionInfo* parent )
{
    _parent = parent;
}
SessionInfo* SessionInfo::parent() const
{
    return _parent;
}
void SessionInfo::setProperty( Property property , const QVariant& value )
{
    _properties[property] = value;
}
QVariant SessionInfo::property( Property property ) const
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

QString SessionInfo::name() const
{
    return _config->readEntry("Name");
}

QString SessionInfo::icon() const
{
    return _config->readEntry("Icon","konsole");
}


bool SessionInfo::isRootSession() const
{
    const QString& cmd = _config->readEntry("Exec");

    return ( cmd.startsWith("su") );
}

QString SessionInfo::command(bool stripRoot , bool stripArguments) const
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

QStringList SessionInfo::arguments() const
{
    QString commandString = command(false,false);

    //FIXME:  This wll fail where single arguments contain spaces (because slashes or quotation
    //marks are used) - eg. vi My\ File\ Name
    return commandString.split(QChar(' '));
}

bool SessionInfo::isAvailable() const
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

QString SessionInfo::path() const
{
    return _path;
}


QString SessionInfo::newSessionText() const
{
    QString commentEntry = _config->readEntry("Comment");

    if ( commentEntry.isEmpty() )
        return i18n("New %1",name());
    else
        return commentEntry;
}

QString SessionInfo::terminal() const
{
    return _config->readEntry("Term","xterm");
}
QString SessionInfo::keyboardSetup() const
{
    return _config->readEntry("KeyTab",QString());
}
QString SessionInfo::colorScheme() const
{
    //TODO Pick a default color scheme
    return _config->readEntry("Schema").replace(".schema",QString::null);
}
QFont SessionInfo::defaultFont() const
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
QString SessionInfo::defaultWorkingDirectory() const
{
    return _config->readPathEntry("Cwd");
}

/*MutableSessionInfo::MutableSessionInfo(const QString& path)
 : SessionInfo(path) {}

void MutableSessionInfo::setName(const QString& name) { _name = name; }

QString MutableSessionInfo::name() const { return _name; }

void MutableSessionInfo::setCommand(const QString& command) { _command = command; }
QString MutableSessionInfo::command(bool,bool) const { return _command; }

void MutableSessionInfo::setArguments(const QStringList& arguments){  _arguments = arguments; }
QStringList MutableSessionInfo::arguments() const { return _arguments; }

void MutableSessionInfo::setTerminal(const QString& terminal) { _terminal = terminal; }
QString MutableSessionInfo::terminal() const { return _terminal; }

void MutableSessionInfo::setKeyboardSetup(const QString& keyboard) { _keyboardSetup = keyboard; }
QString MutableSessionInfo::keyboardSetup() const { return _keyboardSetup; }

void MutableSessionInfo::setColorScheme(const QString& colorScheme) { _colorScheme = colorScheme; }
QString MutableSessionInfo::colorScheme() const { return _colorScheme; }

void MutableSessionInfo::setDefaultWorkingDirectory(const QString& dir) { _defaultWorkingDirectory = dir; }
QString MutableSessionInfo::defaultWorkingDirectory() const { return _defaultWorkingDirectory; }

void MutableSessionInfo::setNewSessionText(const QString& text) { _newSessionText = text; }
QString MutableSessionInfo::newSessionText() const { return _newSessionText; }

void MutableSessionInfo::setDefaultFont(const QFont& font) { _defaultFont = font; }
QFont MutableSessionInfo::defaultFont() const { return _defaultFont ; }

void MutableSessionInfo::setIcon(const QString& icon) { _icon = icon; }
QString MutableSessionInfo::icon() const { return _icon; }
*/

SessionManager::SessionManager()
{
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
        SessionInfo* newType = new SessionInfo(configFile);

        QString sessionKey = addSessionType( newType );

        if ( QFileInfo(configFile).fileName() == defaultSessionFilename )
            _defaultSessionType = sessionKey;
    }

    Q_ASSERT( _types.count() > 0 );
    Q_ASSERT( !_defaultSessionType.isEmpty() );

    // now that the session types have been loaded,
    // get the list of favorite sessions
    loadFavorites();
}

SessionManager::~SessionManager()
{
    QListIterator<SessionInfo*> infoIter(_types.values());

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

void SessionManager::pushSessionSettings( const SessionInfo* info )
{
    addSetting( InitialWorkingDirectory , SessionConfig , info->defaultWorkingDirectory() );
    addSetting( ColorScheme , SessionConfig , info->colorScheme() );
}

Session* SessionManager::createSession(QString key )
{
    Session* session = 0;
    
    const SessionInfo* info = 0;

    if ( key.isEmpty() )
        info = defaultSessionType();
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
                kDebug() << "running " << info->command(false) << ": argument " << iter.next() << endl;
           
            session->setInitialWorkingDirectory( activeSetting(InitialWorkingDirectory).toString() ); 
            session->setProgram( info->command(false) );
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

QList<QString> SessionManager::availableSessionTypes() const
{
    return _types.keys();
}

SessionInfo* SessionManager::sessionType(const QString& key) const
{
    if ( key.isEmpty() )
        return defaultSessionType();

    if ( _types.contains(key) )
        return _types[key];
    else
        return 0;
}

SessionInfo* SessionManager::defaultSessionType() const
{
    return _types[_defaultSessionType];
}

QString SessionManager::defaultSessionKey() const
{
    return _defaultSessionType;
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

QString SessionManager::addSessionType(SessionInfo* type)
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

    emit sessionTypeAdded(key);

    return key;
}

void SessionManager::deleteSessionType(const QString& key)
{
    SessionInfo* type = sessionType(key);

    setFavorite(key,false);

    if ( type )
    {
        _types.remove(key);
        delete type;
    }

    emit sessionTypeRemoved(key); 

    qWarning() << __FUNCTION__ << "TODO: Make this change persistant.";
    //TODO Store this information persistantly
}
void SessionManager::setDefaultSessionType(const QString& key)
{
   Q_ASSERT ( _types.contains(key) );

   _defaultSessionType = key;

   SessionInfo* info = sessionType(key);
  
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
    KConfigGroup favoriteGroup = appConfig->group("Favorite Sessions");

    qDebug() << "loading favorites";

    if ( favoriteGroup.hasKey("Favorites") )
    {
        qDebug() << "found favorites key";
       QStringList list = favoriteGroup.readEntry("Favorites",QStringList());

       qDebug() << "found " << list.count() << "entries";

       QListIterator<QString> lit(list);
       while (lit.hasNext())
           qDebug() << "entry: " << lit.next();

       QHashIterator<QString,SessionInfo*> iter(_types);
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
    KConfigGroup favoriteGroup = appConfig->group("Favorite Sessions");

    QStringList names;
    QSetIterator<QString> keyIter(_favorites);
    while ( keyIter.hasNext() )
    {
        const QString& key = keyIter.next();

        Q_ASSERT( _types.contains(key) && sessionType(key) != 0 );

        names << sessionType(key)->name();
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

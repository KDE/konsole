/*
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
#include "Application.h"

// std
#include <iostream>

// Qt
#include <QtCore/QHashIterator>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>

// KDE
#include <KAction>
#include <KCmdLineArgs>
#include <KUrl>
#include <KDebug>
#include <KWindowSystem>

// Konsole
#include "ColorScheme.h"
#include "ProfileList.h"
#include "SessionManager.h"
#include "KeyboardTranslator.h"
#include "MainWindow.h"
#include "Session.h"
#include "TerminalDisplay.h"
#include "ViewManager.h"

using namespace Konsole;

Application::Application() : KUniqueApplication()
{
    init();
}

void Application::init()
{
    _backgroundInstance = 0;

    // check for compositing functionality
    TerminalDisplay::setTransparencyEnabled( KWindowSystem::compositingActive() );

#if defined(Q_WS_MAC) && QT_VERSION >= 0x040600
    // this ensures that Ctrl and Meta are not swapped, so CTRL-C and friends
    // will work correctly in the terminal
    setAttribute(Qt::AA_MacDontSwapCtrlAndMeta);

    // KDE's menuBar()->isTopLevel() hasn't worked in a while.
    // For now, put menus inside Konsole window; this also make
    // the keyboard shortcut to show menus look reasonable.
    setAttribute(Qt::AA_DontUseNativeMenuBar);
#endif
}

Application* Application::self()
{
    return (Application*)KApp;
}

MainWindow* Application::newMainWindow()
{
    MainWindow* window = new MainWindow();
    window->setSessionList( new ProfileList(true,window) );

    connect( window , SIGNAL(newSessionRequest(Profile::Ptr,QString,ViewManager*)),
            this , SLOT(createSession(Profile::Ptr,QString,ViewManager*)));
    connect( window , SIGNAL(newSSHSessionRequest(Profile::Ptr,KUrl,ViewManager*)),
            this , SLOT(createSSHSession(Profile::Ptr,KUrl,ViewManager*)));
    connect( window , SIGNAL(newWindowRequest(Profile::Ptr,QString)),
            this , SLOT(createWindow(Profile::Ptr,QString)) );
    connect( window->viewManager() , SIGNAL(viewDetached(Session*)) ,
            this , SLOT(detachView(Session*)) );

    return window;
}

void Application::listAvailableProfiles()
{
    QList<QString> paths = SessionManager::instance()->availableProfilePaths();
    QListIterator<QString> iter(paths);

    while ( iter.hasNext() )
    {
        QFileInfo info(iter.next());
        std::cout << info.completeBaseName().toLocal8Bit().data() << std::endl;
    }
    quit();
}

void Application::listProfilePropertyInfo()
{
    Profile::Ptr tempProfile = SessionManager::instance()->defaultProfile();
    const QStringList names = tempProfile->propertiesInfoList();
    for (int i = 0; i < names.size(); ++i)
        std::cout << names.at(i).toLocal8Bit().data() << std::endl;
    quit();
}

int Application::newInstance()
{
    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
    static bool firstInstance = true;

    // handle session management
    if ((args->count() != 0) || !firstInstance || !isSessionRestored())
    {
        // check for arguments to print help or other information to the terminal,
        // quit if such an argument was found
        if ( processHelpArgs(args) )
            return 0;

        // create a new window or use an existing one
        MainWindow* window = processWindowArgs(args);

        // select profile to use
        processProfileSelectArgs(args,window);

        // process various command-line options which cause a property of the
        // default profile to be changed
        processProfileChangeArgs(args,window);

        if ( !args->isSet("tabs-from-file") )
        {
            // create new session
            Session* session = createSession(window->defaultProfile(),
                                             QString(),
                                             window->viewManager());
            if ( !args->isSet("close") ) {
                session->setAutoClose(false);
            }

            // run a custom command, don't store it in the default profile.
            // Otherwise it will become the default for new tabs.
            if ( args->isSet("e") )
            {
                QStringList arguments;
                arguments << args->getOption("e");
                for ( int i = 0 ; i < args->count() ; i++ )
                    arguments << args->arg(i);

                QString exec = args->getOption("e");
                if (exec.startsWith(QLatin1String("./")))
                    exec = QDir::currentPath() + exec.mid(1);

                session->setProgram(exec);
                session->setArguments(arguments);
            }
        }
        else
        {
            // create new session(s) as described in file
            processTabsFromFileArgs(args, window);
        }

        // if the background-mode argument is supplied, start the background session
        // ( or bring to the front if it already exists )
        if ( args->isSet("background-mode") )
            startBackgroundMode(window);
        else
        {
            // Qt constrains top-level windows which have not been manually resized
            // (via QWidget::resize()) to a maximum of 2/3rds of the screen size.
            //
            // This means that the terminal display might not get the width/height
            // it asks for.  To work around this, the widget must be manually resized
            // to its sizeHint().
            //
            // This problem only affects the first time the application is run.  After
            // that KMainWindow will have manually resized the window to its saved size
            // at this point (so the Qt::WA_Resized attribute will be set)
            if (!window->testAttribute(Qt::WA_Resized))
                window->resize(window->sizeHint());

            window->show();
        }
    }

    firstInstance = false;
    args->clear();
    return 0;
}

/* Documentation for tab file:
 * ;; is the token separator
 * # at the beginning of line results in line being ignored
 * tokens are title:, command:, profile: (not used currently)
 * Note that the title is static and the tab will close when the
 * command is complete (do not use --noclose).  You can start new tabs.
 * Examples:
title: This is the title;; command: ssh jupiter
title: Top this!;; command: top
#title: This is commented out;; command: ssh jupiter
command: ssh  earth
*/
void Application::processTabsFromFileArgs(KCmdLineArgs* args, MainWindow* window)
{
    // Open tab configuration file
    const QString tabsFileName(args->getOption("tabs-from-file"));
    QFile tabsFile(tabsFileName);
    if(!tabsFile.open(QFile::ReadOnly)) {
        kWarning() << "ERROR: Cannot open tabs file "
                   << tabsFileName.toLocal8Bit().data();
        quit();
    }

    unsigned int sessions = 0;
    while (!tabsFile.atEnd()) {
        QString lineString(tabsFile.readLine());
        if ((lineString.isEmpty()) || (lineString[0] == '#'))
            continue;

        QHash<QString, QString> lineTokens;
        QStringList lineParts = lineString.split(";;", QString::SkipEmptyParts);

        for (int i = 0; i < lineParts.size(); ++i) {
            QString key = lineParts.at(i).section(':', 0, 0).trimmed().toLower();
            QString value = lineParts.at(i).section(':', 1, -1).trimmed();
            lineTokens[key] = value;

        }
        // command: is the only token required
        if (lineTokens.contains("command")) {
            createTabFromArgs(args, window, lineTokens);
            sessions++;
        } else {
            kWarning() << "Tab file line must have the 'command:' entry.";
        }
    }
    tabsFile.close();

    if (sessions < 1) {
        kWarning() << "No valid lines found in "
                   << tabsFileName.toLocal8Bit().data();
       quit();
    }
}

void Application::createTabFromArgs(KCmdLineArgs* args, MainWindow* window, const QHash<QString, QString>& tokens)
{
    QString title = tokens["title"];
    QString command = tokens["command"];
    QString profile = tokens["profile"]; // currently not used

    // FIXME: A lot of duplicate code below

    // Get the default profile
    Profile::Ptr defaultProfile = window->defaultProfile();
    if (!defaultProfile) {
        defaultProfile = SessionManager::instance()->defaultProfile();
    }

    // Create profile setting, with command and workdir
    Profile::Ptr newProfile = Profile::Ptr(new Profile(defaultProfile));
    newProfile->setHidden(true);
    newProfile->setProperty(Profile::Command,   command);
    newProfile->setProperty(Profile::Arguments, command.split(' '));
    if(args->isSet("workdir")) {
        newProfile->setProperty(Profile::Directory,args->getOption("workdir"));
    }
    if(!newProfile->isEmpty()) {
        window->setDefaultProfile(newProfile);
    }

    // Create the new session
    Session* session = createSession(window->defaultProfile(), QString(), window->viewManager());
    session->setTabTitleFormat(Session::LocalTabTitle, title);
    session->setTabTitleFormat(Session::RemoteTabTitle, title);
    session->setTitle(Session::DisplayedTitleRole, title);   // Ensure that new title is displayed
    if ( !args->isSet("close") ) {
        session->setAutoClose(false);
    }
    if (!window->testAttribute(Qt::WA_Resized)) {
        window->resize(window->sizeHint());
    }

    // Reset the profile to default. Otherwise, the next manually
    // created tab would have the command above!
    newProfile = Profile::Ptr(new Profile(defaultProfile));
    newProfile->setHidden(true);
    window->setDefaultProfile(newProfile);

}

MainWindow* Application::processWindowArgs(KCmdLineArgs* args)
{
    MainWindow* window = 0;
    if ( args->isSet("new-tab") )
    {
        QListIterator<QWidget*> iter(topLevelWidgets());
        iter.toBack();
        while ( iter.hasPrevious() )
        {
            window = qobject_cast<MainWindow*>(iter.previous());
            if ( window != 0 )
                break;
        }
    }

    if ( window == 0 )
    {
        window = newMainWindow();
    }
    return window;
}

void Application::processProfileSelectArgs(KCmdLineArgs* args,MainWindow* window)
{
    if ( args->isSet("profile") )
    {
        Profile::Ptr profile = SessionManager::instance()->loadProfile(args->getOption("profile"));
        if (!profile)
            profile = SessionManager::instance()->defaultProfile();

         window->setDefaultProfile(profile);
    }
}

bool Application::processHelpArgs(KCmdLineArgs* args)
{
    if ( args->isSet("list-profiles") )
    {
        listAvailableProfiles();
        return true;
    }
    else if ( args->isSet("list-profile-properties") )
    {
        listProfilePropertyInfo();
        return true;
    }
    return false;
}
void Application::processProfileChangeArgs(KCmdLineArgs* args,MainWindow* window)
{
    Profile::Ptr defaultProfile = window->defaultProfile();
    if (!defaultProfile)
        defaultProfile = SessionManager::instance()->defaultProfile();
    Profile::Ptr newProfile = Profile::Ptr(new Profile(defaultProfile));
    newProfile->setHidden(true);

    // change the initial working directory
    if( args->isSet("workdir") )
    {
        newProfile->setProperty(Profile::Directory,args->getOption("workdir"));
    }

    // temporary changes to profile options specified on the command line
    foreach( const QString& value , args->getOptionList("p") )
    {
        ProfileCommandParser parser;

        QHashIterator<Profile::Property,QVariant> iter(parser.parse(value));
        while ( iter.hasNext() )
        {
            iter.next();
            newProfile->setProperty(iter.key(),iter.value());
        }
    }

    if (!newProfile->isEmpty())
    {
        window->setDefaultProfile(newProfile);
    }
}

void Application::startBackgroundMode(MainWindow* window)
{
    if ( _backgroundInstance )
    {
        return;
    }

    KAction* action = new KAction(window);
    action->setObjectName( QLatin1String("Konsole Background Mode" ));
    //TODO - Customizable key sequence for this
    action->setGlobalShortcut( KShortcut(QKeySequence(Qt::Key_F12)) );

    _backgroundInstance = window;

    connect( action , SIGNAL(triggered()) , this , SLOT(toggleBackgroundInstance()) );
}

void Application::toggleBackgroundInstance()
{
    Q_ASSERT( _backgroundInstance );

    if ( !_backgroundInstance->isVisible() )
    {
        _backgroundInstance->show();
        // ensure that the active terminal display has the focus.
        // without this, an odd problem occurred where the focus widget would change
        // each time the background instance was shown
        _backgroundInstance->viewManager()->activeView()->setFocus();
    }
    else
    {
        _backgroundInstance->hide();
    }
}

Application::~Application()
{
    SessionManager::instance()->closeAll();
    SessionManager::instance()->saveSettings();
}

void Application::detachView(Session* session)
{
    MainWindow* window = newMainWindow();
    window->viewManager()->createView(session);
    window->show();
}

void Application::createWindow(Profile::Ptr profile , const QString& directory)
{
    MainWindow* window = newMainWindow();
    window->setDefaultProfile(profile);
    createSession(profile,directory,window->viewManager());
    window->show();
}

Session* Application::createSession(Profile::Ptr profile, const QString& directory , ViewManager* viewManager)
{
    if (!profile)
        profile = SessionManager::instance()->defaultProfile();

    Session* session = SessionManager::instance()->createSession(profile);

    if (!directory.isEmpty() && profile->property<bool>(Profile::StartInCurrentSessionDir))
        session->setInitialWorkingDirectory(directory);

    // create view before starting the session process so that the session doesn't suffer
    // a change in terminal size right after the session starts.  Some applications such as GNU Screen
    // and Midnight Commander don't like this happening
    viewManager->createView(session);

    return session;
}

Session* Application::createSSHSession(Profile::Ptr profile, const KUrl& url, ViewManager* viewManager)
{
    if (!profile)
        profile = SessionManager::instance()->defaultProfile();

    Session* session = SessionManager::instance()->createSession(profile);

    session->sendText("ssh ");

    if ( url.port() > -1 )
        session->sendText("-p " + QString::number(url.port()) + ' ' );
    if ( url.hasUser() )
        session->sendText(url.user() + '@');
    if ( url.hasHost() )
        session->sendText(url.host() + '\r');

    // create view before starting the session process so that the session doesn't suffer
    // a change in terminal size right after the session starts.  some applications such as GNU Screen
    // and Midnight Commander don't like this happening
    viewManager->createView(session);
    session->run();

    return session;
}

#include "Application.moc"


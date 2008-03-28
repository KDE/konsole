/*
    Copyright (C) 2006-2007 by Robert Knight <robertknight@gmail.com>

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

#include "kdebug.h"

// Qt
#include <QHashIterator>
#include <QFileInfo>

// KDE
#include <KAction>
#include <KCmdLineArgs>
#include <KIcon>
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

#ifdef Q_WS_X11
Application::Application(Display* display , Qt::HANDLE visual, Qt::HANDLE colormap)
    : KUniqueApplication(display,visual,colormap) 
{
    init();
}
#endif

Application::Application() : KUniqueApplication()
{
    init();
}

void Application::init()
{
    _sessionList = 0;
    _backgroundInstance = 0;

    // check for compositing functionality
    TerminalDisplay::setTransparencyEnabled( KWindowSystem::compositingActive() );

    setWindowIcon(KIcon("utilities-terminal"));
}

Application* Application::self()
{
    return (Application*)KApp;
}

MainWindow* Application::newMainWindow()
{
    MainWindow* window = new MainWindow();
    window->setSessionList( new ProfileList(true,window) );

    connect( window , SIGNAL(newSessionRequest(Profile::Ptr,const QString&,ViewManager*)), 
                      this , SLOT(createSession(Profile::Ptr,const QString&,ViewManager*)));
    connect( window , SIGNAL(newWindowRequest(Profile::Ptr,const QString&)),
                      this , SLOT(createWindow(Profile::Ptr,const QString&)) );
    connect( window->viewManager() , SIGNAL(viewDetached(Session*)) , this , SLOT(detachView(Session*)) );

    return window;
}

void Application::listAvailableProfiles()
{
    QList<QString> paths = SessionManager::instance()->availableProfilePaths();
    QListIterator<QString> iter(paths);

    while ( iter.hasNext() )
    {
        QFileInfo info(iter.next());
        std::cout << info.baseName().toLocal8Bit().data() << std::endl;
    }
}

int Application::newInstance()
{
    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();

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

    // create new session
    Session* session = createSession( window->defaultProfile() , QString() , window->viewManager() );
	if ( !args->isSet("close") )
		session->setAutoClose(false);

    // if the background-mode argument is supplied, start the background session
    // ( or bring to the front if it already exists )
    if ( args->isSet("background-mode") )
        startBackgroundMode(window);
    else
        window->show();

    return 0;
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
    return false;
}
void Application::processProfileChangeArgs(KCmdLineArgs* args,MainWindow* window) 
{
	Profile::Ptr defaultProfile = window->defaultProfile();
    Profile::Ptr newProfile = Profile::Ptr(new Profile(defaultProfile));
	newProfile->setHidden(true);

    // run a custom command
    if ( args->isSet("e") ) 
    {
        QStringList arguments;
        arguments << args->getOption("e");
        for ( int i = 0 ; i < args->count() ; i++ )
           arguments << args->arg(i); 
   
        newProfile->setProperty(Profile::Command,args->getOption("e"));
        newProfile->setProperty(Profile::Arguments,arguments);
    }

    // change the initial working directory
    if( args->isSet("workdir") )
    {
        newProfile->setProperty(Profile::Directory,args->getOption("workdir"));
    }

    // temporary changes to profile options specified on the command line
    foreach( QString value , args->getOptionList("p") ) 
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
        KShortcut shortcut = action->shortcut();
        action->setObjectName("Konsole Background Mode");
        //TODO - Customisable key sequence for this
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
        // without this, an odd problem occurred where the focus widgetwould change
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
	SessionManager::instance()->saveState();
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

Session* Application::createSession(Profile::Ptr profile, const QString& directory , ViewManager* view)
{
	if (!profile)
		profile = SessionManager::instance()->defaultProfile();

    Session* session = SessionManager::instance()->createSession(profile);

    if (!directory.isEmpty() && profile->property<bool>(Profile::StartInCurrentSessionDir))
        session->setInitialWorkingDirectory(directory);

    // create view before starting the session process so that the session doesn't suffer
    // a change in terminal size right after the session starts.  some applications such as GNU Screen
    // and Midnight Commander don't like this happening
    view->createView(session);
    session->run();

	return session;
}

#include "Application.moc"

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
#include <QFileInfo>

// KDE
#include <KAction>
#include <KCmdLineArgs>
#include <KWindowSystem>

// Konsole
#include "ColorScheme.h"
#include "ProfileList.h"
#include "SessionManager.h"
#include "KeyTrans.h"
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

    // create session manager
    SessionManager::setInstance( new SessionManager() );

    // create color scheme manager
    ColorSchemeManager::setInstance( new ColorSchemeManager() );

    // new keyboard translator manager
    KeyboardTranslatorManager::setInstance( new KeyboardTranslatorManager() );

    // old keyboard translator manager
    // (for use until the new one is completed)
    KeyTrans::loadAll();

    // check for compositing functionality
    TerminalDisplay::setTransparencyEnabled( KWindowSystem::compositingActive() );
}

Application* Application::self()
{
    return (Application*)KApp;
}

MainWindow* Application::newMainWindow()
{
    MainWindow* window = new MainWindow();
    window->setSessionList( new ProfileList(true,window) );

    connect( window , SIGNAL(newSessionRequest(const QString&,const QString&,ViewManager*)), 
                      this , SLOT(createSession(const QString&,const QString&,ViewManager*)));
    connect( window , SIGNAL(newWindowRequest(const QString&,const QString&)),
                      this , SLOT(createWindow(const QString&,const QString&)) );
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
  
    if ( args->isSet("list-profiles") )
    {
        listAvailableProfiles();
        return 0;
    }

    // create a new window and session to run in it 
    MainWindow* window = newMainWindow();

    if ( args->isSet("profile") )
    {
        QString key = SessionManager::instance()->loadProfile(args->getOption("profile"));
        window->setDefaultProfile(key);
    }

    createSession( window->defaultProfile() , QString() , window->viewManager() );

    // if the background-mode argument is supplied, start the background session
    // ( or bring to the front if it already exists )
    if ( args->isSet("background-mode") )
        startBackgroundMode(window);
    else
        window->show();

    return 0;
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
    delete SessionManager::instance();
    delete ColorSchemeManager::instance();
    delete KeyboardTranslatorManager::instance();

    SessionManager::setInstance(0);
    ColorSchemeManager::setInstance(0);
    KeyboardTranslatorManager::setInstance(0);
}

void Application::detachView(Session* session)
{
    MainWindow* window = newMainWindow();
    window->viewManager()->createView(session);
    window->show();
}

void Application::createWindow(const QString& key , const QString& directory)
{
    MainWindow* window = newMainWindow();
    window->setDefaultProfile(key);
    createSession(key,directory,window->viewManager());
    window->show();
}

void Application::createSession(const QString& key , const QString& directory , ViewManager* view)
{
    Session* session = SessionManager::instance()->createSession(key);

    if (!directory.isEmpty())
        session->setInitialWorkingDirectory(directory);

    // create view before starting the session process so that the session doesn't suffer
    // a change in terminal size right after the session starts.  some applications such as GNU Screen
    // and Midnight Commander don't like this happening
    view->createView(session);
    session->run();
}

#include "Application.moc"

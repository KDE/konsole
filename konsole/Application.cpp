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

#include "kdebug.h"

#include "SessionList.h"
#include "SessionManager.h"
#include "KeyTrans.h"
#include "Application.h"
#include "MainWindow.h"
#include "Session.h"
#include "ViewManager.h"

// temporary - testing history feature
#include "History.h"

using namespace Konsole;

// global variable to determine whether or not true transparency should be used
// this should be made into a static class variable of Application
int true_transparency = true;

Application::Application()
    : _sessionList(0)
{
    // create session manager
    _sessionManager = new SessionManager();

    // load keyboard layouts
    KeyTrans::loadAll();
};

Application* Application::self()
{
    return (Application*)KApp;
}

MainWindow* Application::newMainWindow()
{
    MainWindow* window = new MainWindow();
    window->setSessionList( new SessionList(sessionManager(),window) );

    connect( window , SIGNAL(requestSession(const QString&,ViewManager*)), 
                      this , SLOT(createSession(const QString&,ViewManager*)));
    connect( window->viewManager() , SIGNAL(viewDetached(Session*)) , this , SLOT(detachView(Session*)) );

    return window;
}

int Application::newInstance()
{
    MainWindow* window = newMainWindow();

    createSession( QString() , window->viewManager() );
    window->show();

    return 0;
}

SessionManager* Application::sessionManager()
{
    return _sessionManager;
}

void Application::detachView(Session* session)
{
    MainWindow* window = newMainWindow();
    window->viewManager()->createView(session);
    window->show();
}

void Application::createSession(const QString& key , ViewManager* view)
{
    Session* session = _sessionManager->createSession(key);
    session->setListenToKeyPress(true); 
    //session->setConnect(true);
    
    // create view before starting the session process so that the session doesn't suffer
    // a change in terminal size right after the session starts.  some applications such as GNU Screen
    // and Midnight Commander don't like this happening
    view->createView(session);
    session->run();
}

#include "Application.moc"

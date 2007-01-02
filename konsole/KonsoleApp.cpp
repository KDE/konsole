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
#include "KonsoleApp.h"
#include "KonsoleMainWindow.h"
#include "TESession.h"
#include "ViewManager.h"

// temporary - testing history feature
#include "TEHistory.h"

// global variable to determine whether or not true transparency should be used
// this should be made into a static class variable of KonsoleApp
int true_transparency = true;

KonsoleApp::KonsoleApp()
    : _sessionList(0)
{
    // create session manager
    _sessionManager = new SessionManager();

    // load keyboard layouts
    KeyTrans::loadAll();
};

KonsoleApp* KonsoleApp::self()
{
    return (KonsoleApp*)KApp;
}

KonsoleMainWindow* KonsoleApp::newMainWindow()
{
    KonsoleMainWindow* window = new KonsoleMainWindow();
    window->setSessionList( new SessionList(sessionManager(),window) );

    connect( window , SIGNAL(requestSession(const QString&,ViewManager*)), 
                      this , SLOT(createSession(const QString&,ViewManager*)));
    connect( window->viewManager() , SIGNAL(viewDetached(TESession*)) , this , SLOT(detachView(TESession*)) );

    return window;
}

int KonsoleApp::newInstance()
{
    KonsoleMainWindow* window = newMainWindow();

    createSession( QString() , window->viewManager() );
    window->show();

    return 0;
}

SessionManager* KonsoleApp::sessionManager()
{
    return _sessionManager;
}

void KonsoleApp::detachView(TESession* session)
{
    KonsoleMainWindow* window = newMainWindow();
    window->viewManager()->createView(session);
    window->show();
}

void KonsoleApp::createSession(const QString& key , ViewManager* view)
{
    TESession* session = _sessionManager->createSession(key);
    session->setConnect(true);
    session->run();
    view->createView(session);
}

#include "KonsoleApp.moc"

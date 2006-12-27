/*
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

#include "SessionManager.h"
#include "keytrans.h"
#include "KonsoleApp.h"
#include "KonsoleMainWindow.h"

// global variable to determine whether or not true transparency should be used
// this should be made into a static class variable of KonsoleApp
int true_transparency = true;

KonsoleApp::KonsoleApp()
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

int KonsoleApp::newInstance()
{
    KonsoleMainWindow* window = new KonsoleMainWindow();
        
    window->show();

    return 0;
}

SessionManager* KonsoleApp::sessionManager()
{
    return _sessionManager;
}

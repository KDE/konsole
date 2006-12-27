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

// KDE
#include <KAction>
#include <KActionCollection>
#include <KActionMenu>
#include <KLocale>
#include <KMenu>
#include <kstandardaction.h>

// Konsole
#include "konsolebookmarkhandler.h"

#include "KonsoleApp.h"
#include "KonsoleMainWindow.h"
#include "ViewManager.h"
#include "ViewSplitter.h"

KonsoleMainWindow::KonsoleMainWindow()
 : KMainWindow()
{
    // create main window widgets
    setupWidgets();

    // create actions for menus
    setupActions();

    // create view manager
    _viewManager = new ViewManager(this);

    // create menus
    setXMLFile("konsoleui.rc");
    createGUI();
}

void KonsoleMainWindow::setupActions()
{
    // File Menu
    new KAction( i18n("New &Tab") , actionCollection() , "new-tab" ); 
    new KAction( i18n("New &Window") , actionCollection() , "new-window" );

    KStandardAction::quit( KonsoleApp::self() , SLOT(quit()) , actionCollection() ,"exit");

    // Bookmark Menu
    KActionMenu* bookmarkMenu = new KActionMenu(i18n("&Bookmarks"), actionCollection(),"bookmark");
    new KonsoleBookmarkHandler( this , bookmarkMenu->menu() , true );

    // Settings Menu
    KStandardAction::configureNotifications( 0 , 0 , actionCollection() , "configure-notifications" );
    KStandardAction::keyBindings( 0 , 0 , actionCollection() , "configure-shortcuts" );
    KStandardAction::preferences( 0 , 0 , actionCollection() , "configure-application" ); 
}

void KonsoleMainWindow::setupWidgets()
{
    // create main view area
    _viewSplitter = new ViewSplitter(this);
    setCentralWidget(_viewSplitter);


}

#include "KonsoleMainWindow.moc"

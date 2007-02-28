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

// KDE
#include <KAction>
#include <KActionCollection>
#include <KActionMenu>
#include <KKeyDialog>
#include <KLocale>
#include <KMenu>
#include <KService>
#include <KToolInvocation>
#include <kstandardaction.h>
#include <KXMLGUIFactory>

// Konsole
#include "KonsoleApp.h"
#include "KonsoleBookmarkHandler.h"
#include "KonsoleMainWindow.h"
#include "SessionList.h"
#include "ViewManager.h"
#include "ViewSplitter.h"

KonsoleMainWindow::KonsoleMainWindow()
 : KMainWindow() ,
   _bookmarkHandler(0)
{
    // add a small amount of space between the top of the window and the main widget
    // to prevent the menu bar and main widget borders touching (which looks very ugly) in styles
    // where the menu bar has a lower border
    setContentsMargins(0,2,0,0);
    
    // create main window widgets
    setupWidgets();

    // create actions for menus
    setupActions();

    // create view manager
    setXMLFile("konsoleui.rc");
    _viewManager = new ViewManager(this);
    connect( _viewManager , SIGNAL(empty()) , this , SLOT(close()) );

    // create menus
    createGUI();
}

ViewManager* KonsoleMainWindow::viewManager()
{
    return _viewManager;
}

void KonsoleMainWindow::setupActions()
{
    KActionCollection* collection = actionCollection();

    // File Menu
    QAction* newTabAction = collection->addAction("new-tab"); 
    newTabAction->setIcon( KIcon("openterm") );
    newTabAction->setText( i18n("New &Tab") );

    QAction* newWindowAction = collection->addAction("new-window"); 
    newWindowAction->setIcon( KIcon("window_new") );
    newWindowAction->setText( i18n("New &Window") );

    connect( newTabAction , SIGNAL(triggered()) , this , SLOT(newTab()) );
    connect( newWindowAction , SIGNAL(triggered()) , this , SLOT(newWindow()) );

    KStandardAction::quit( KonsoleApp::self() , SLOT(quit()) , collection );

    // Bookmark Menu
    KActionMenu* bookmarkMenu = new KActionMenu(i18n("&Bookmarks") , collection );
    _bookmarkHandler = new KonsoleBookmarkHandler( collection , bookmarkMenu->menu() , true );
    collection->addAction("bookmark" , bookmarkMenu);

    // Settings Menu
    KStandardAction::configureNotifications( 0 , 0 , collection  );
    KStandardAction::keyBindings( this , SLOT(showShortcutsDialog()) , collection  );
    KStandardAction::preferences( this , SLOT(showPreferencesDialog()) , collection ); 
}

KonsoleBookmarkHandler* KonsoleMainWindow::bookmarkHandler() const
{
    return _bookmarkHandler;
}

void KonsoleMainWindow::setSessionList(SessionList* list)
{
    unplugActionList("new-session-types");
    plugActionList( "new-session-types" , list->actions() );

    connect( list , SIGNAL(sessionSelected(const QString&)) , this , 
            SLOT(sessionSelected(const QString&)) );
}

void KonsoleMainWindow::newTab()
{
    emit requestSession(QString(),_viewManager);
}

void KonsoleMainWindow::newWindow()
{
    KonsoleApp::self()->newInstance();
}

void KonsoleMainWindow::showShortcutsDialog()
{
    KKeyDialog::configure( actionCollection() );
}

void KonsoleMainWindow::sessionSelected(const QString& key)
{
    emit requestSession(key,_viewManager);
}

void KonsoleMainWindow::showPreferencesDialog()
{
    KToolInvocation::startServiceByDesktopName("konsole",QString());
}

void KonsoleMainWindow::mergeWindows()
{
    // merges all of the open Konsole windows into this window
    // by merging the view manager associated with the other Konsole windows
    // into this window's view manager

    QListIterator<QWidget*> topLevelIter( KonsoleApp::topLevelWidgets() );

    while (topLevelIter.hasNext())
    {
        KonsoleMainWindow* window = dynamic_cast<KonsoleMainWindow*>(topLevelIter.next());
        if ( window && window != this )
        {
            _viewManager->merge( window->_viewManager );
            window->deleteLater();
        }
    }
}

void KonsoleMainWindow::setupWidgets()
{


}

#include "KonsoleMainWindow.moc"

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

// Konsole
#include "KonsoleApp.h"
#include "KonsoleBookmarkHandler.h"
#include "KonsoleMainWindow.h"
#include "SessionList.h"
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
    connect( _viewManager , SIGNAL(empty()) , this , SLOT(deleteLater()) );

    // create menus
    setXMLFile("konsoleui.rc");
    createGUI();
}

ViewManager* KonsoleMainWindow::viewManager()
{
    return _viewManager;
}

void KonsoleMainWindow::setupActions()
{
    // File Menu
    KAction* newTabAction = new KAction( i18n("New &Tab") , actionCollection() , "new-tab" ); 
    KAction* newWindowAction = new KAction( i18n("New &Window") , actionCollection() , "new-window" );

    connect( newTabAction , SIGNAL(triggered()) , this , SLOT(newTab()) );
    connect( newWindowAction , SIGNAL(triggered()) , this , SLOT(newWindow()) );

    KStandardAction::quit( KonsoleApp::self() , SLOT(quit()) , actionCollection() ,"exit");

    // Bookmark Menu
    KActionMenu* bookmarkMenu = new KActionMenu(i18n("&Bookmarks"), actionCollection(),"bookmark");
    new KonsoleBookmarkHandler( this , bookmarkMenu->menu() , true );

    // Settings Menu
    KStandardAction::configureNotifications( 0 , 0 , actionCollection() , "configure-notifications" );
    KStandardAction::keyBindings( this , SLOT(showShortcutsDialog()) , actionCollection() , "configure-shortcuts" );
    KStandardAction::preferences( this , SLOT(showPreferencesDialog()) , actionCollection() , 
                                  "configure-application" ); 
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

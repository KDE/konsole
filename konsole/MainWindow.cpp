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

// Qt
#include <QVBoxLayout>

// KDE
#include <KAction>
#include <KActionCollection>
#include <KActionMenu>
#include <KShortcutsDialog>
#include <KLocale>
#include <KMenu>
#include <KMenuBar>
#include <KService>
#include <KToolInvocation>
#include <kstandardaction.h>
#include <KXMLGUIFactory>

// Konsole
#include "Application.h"
#include "BookmarkHandler.h"
#include "IncrementalSearchBar.h"
#include "MainWindow.h"
#include "RemoteConnectionDialog.h"
#include "SessionList.h"
#include "ViewManager.h"
#include "ViewSplitter.h"

using namespace Konsole;

MainWindow::MainWindow()
 : KMainWindow() ,
   _bookmarkHandler(0)
{
    // add a small amount of space between the top of the window and the main widget
    // to prevent the menu bar and main widget borders touching (which looks very ugly) in styles
    // where the menu bar has a lower border
    setContentsMargins(0,2,0,0);
   
    // create actions for menus
    setupActions();

    // create view manager
    setXMLFile("konsoleui.rc");
    _viewManager = new ViewManager(this);
    connect( _viewManager , SIGNAL(empty()) , this , SLOT(close()) );

    // create main window widgets
    setupWidgets();

    // create menus
    createGUI();
}

ViewManager* MainWindow::viewManager() const
{
    return _viewManager;
}

IncrementalSearchBar* MainWindow::searchBar() const
{
    return _searchBar;
}

void MainWindow::setupActions()
{
    KActionCollection* collection = actionCollection();

    // File Menu
    QAction* newTabAction = collection->addAction("new-tab"); 
    newTabAction->setIcon( KIcon("openterm") );
    newTabAction->setText( i18n("New &Tab") );
    newTabAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_N) );
    connect( newTabAction , SIGNAL(triggered()) , this , SLOT(newTab()) );

    QAction* newWindowAction = collection->addAction("new-window"); 
    newWindowAction->setIcon( KIcon("window-new") );
    newWindowAction->setText( i18n("New &Window") );
    newWindowAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_M) );
    connect( newWindowAction , SIGNAL(triggered()) , this , SLOT(newWindow()) );

    QAction* remoteConnectionAction = collection->addAction("remote-connection");
    remoteConnectionAction->setText( i18n("Remote Connection...") );
    remoteConnectionAction->setIcon( KIcon("network") );
    remoteConnectionAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_R) );
    connect( remoteConnectionAction , SIGNAL(triggered()) , this , SLOT(showRemoteConnectionDialog()) ); 

    QAction* customSessionAction = collection->addAction("custom-session");
    customSessionAction->setText( i18n("Custom Session...") );
    KStandardAction::quit( Application::self() , SLOT(quit()) , collection );

    // Bookmark Menu
    KActionMenu* bookmarkMenu = new KActionMenu(i18n("&Bookmarks") , collection );
    _bookmarkHandler = new BookmarkHandler( collection , bookmarkMenu->menu() , true );
    collection->addAction("bookmark" , bookmarkMenu);

    // View Menu
    QAction* hideMenuBarAction = collection->addAction("hide-menubar");
    hideMenuBarAction->setText( i18n("Hide MenuBar") );
    connect( hideMenuBarAction , SIGNAL(triggered()) , menuBar() , SLOT(hide()) );

    // Settings Menu
    KStandardAction::configureNotifications( 0 , 0 , collection  );
    KStandardAction::keyBindings( this , SLOT(showShortcutsDialog()) , collection  );
    KStandardAction::preferences( this , SLOT(showPreferencesDialog()) , collection ); 
}

BookmarkHandler* MainWindow::bookmarkHandler() const
{
    return _bookmarkHandler;
}

void MainWindow::setSessionList(SessionList* list)
{
    unplugActionList("new-session-types");
    plugActionList( "new-session-types" , list->actions() );

    connect( list , SIGNAL(sessionSelected(const QString&)) , this , 
            SLOT(sessionSelected(const QString&)) );
}

void MainWindow::newTab()
{
    emit requestSession(QString(),_viewManager);
}

void MainWindow::newWindow()
{
    Application::self()->newInstance();
}

void MainWindow::showShortcutsDialog()
{
    KShortcutsDialog::configure( actionCollection() );
}

void MainWindow::sessionSelected(const QString& key)
{
    emit requestSession(key,_viewManager);
}

void MainWindow::showPreferencesDialog()
{
    KToolInvocation::startServiceByDesktopName("konsole",QString());
}

void MainWindow::showRemoteConnectionDialog()
{
    RemoteConnectionDialog dialog(this);
    if ( dialog.exec() == QDialog::Accepted )
        emit requestSession(dialog.sessionKey(),_viewManager);
}

void MainWindow::mergeWindows()
{
    // merges all of the open Konsole windows into this window
    // by merging the view manager associated with the other Konsole windows
    // into this window's view manager

    QListIterator<QWidget*> topLevelIter( Application::topLevelWidgets() );

    while (topLevelIter.hasNext())
    {
        MainWindow* window = dynamic_cast<MainWindow*>(topLevelIter.next());
        if ( window && window != this )
        {
            _viewManager->merge( window->_viewManager );
            window->deleteLater();
        }
    }
}

void MainWindow::setupWidgets()
{
    QWidget* widget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout();

    _searchBar = new IncrementalSearchBar( IncrementalSearchBar::AllFeatures , this);
    _searchBar->setVisible(false);

    layout->addWidget( _viewManager->widget() );
    layout->addWidget( _searchBar );
    layout->setMargin(0);
    layout->setSpacing(0);

    widget->setLayout(layout);
    
    setCentralWidget(widget);
}

#include "MainWindow.moc"

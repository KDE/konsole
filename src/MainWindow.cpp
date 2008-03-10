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
#include "MainWindow.h"

// Qt
#include <QtGui/QBoxLayout>

// KDE
#include <KAction>
#include <KActionCollection>
#include <KActionMenu>
#include <KApplication>
#include <KShortcutsDialog>
#include <KLocale>
#include <KMenu>
#include <KMenuBar>
#include <KMessageBox>
#include <KService>
#include <KToggleAction>
#include <KToggleFullScreenAction>
#include <KToolInvocation>
#include <KStandardAction>
#include <KStandardGuiItem>
#include <KXMLGUIFactory>
#include <KNotifyConfigWidget>

// Konsole
#include "BookmarkHandler.h"
#include "IncrementalSearchBar.h"
#include "RemoteConnectionDialog.h"
#include "SessionController.h"
#include "ProfileList.h"
#include "ManageProfilesDialog.h"
#include "Session.h"
#include "ViewManager.h"
#include "ViewSplitter.h"

using namespace Konsole;

MainWindow::MainWindow()
 : KXmlGuiWindow() ,
   _bookmarkHandler(0),
   _pluggedController(0),
   _menuBarVisibilitySet(false)
{
    // create actions for menus
    // the directory ('konsole') is included in the path here so that the XML
    // file can be found when this code is being used in the Konsole part.
    setXMLFile("konsole/konsoleui.rc");

    setupActions();

    // create view manager
        _viewManager = new ViewManager(this,actionCollection());
    connect( _viewManager , SIGNAL(empty()) , this , SLOT(close()) );
    connect( _viewManager , SIGNAL(activeViewChanged(SessionController*)) , this ,
            SLOT(activeViewChanged(SessionController*)) );
    connect( _viewManager , SIGNAL(viewPropertiesChanged(const QList<ViewProperties*>&)) ,
           bookmarkHandler() , SLOT(setViews(const QList<ViewProperties*>&)) );

    connect( _viewManager , SIGNAL(setMenuBarVisibleRequest(bool)) , this ,
            SLOT(setMenuBarVisibleOnce(bool)) );
    connect( _viewManager , SIGNAL(newViewRequest()) , this , SLOT(newTab()) );

    // create main window widgets
    setupWidgets();

    // create menus
    createGUI();

    // replace standard shortcuts which cannot be used in a terminal
    // (as they are reserved for use by terminal programs)
    correctShortcuts();

    // enable save and restore of window size
    setAutoSaveSettings("MainWindow",true);
}

void MainWindow::setMenuBarVisibleOnce(bool visible)
{
	if (_menuBarVisibilitySet || menuBar()->isTopLevelMenu() )
		return;

	menuBar()->setVisible(visible);
	_toggleMenuBarAction->setChecked(visible);

	_menuBarVisibilitySet = true;	
}

void MainWindow::correctShortcuts()
{
    // replace F1 shortcut for help contents
    QAction* helpAction = actionCollection()->action("help_contents");

    Q_ASSERT( helpAction );

    helpAction->setShortcut( QKeySequence() );
}

void MainWindow::setDefaultProfile(const QString& key)
{
    _defaultProfile = key;
}
QString MainWindow::defaultProfile() const
{
    return _defaultProfile;
}

ViewManager* MainWindow::viewManager() const
{
    return _viewManager;
}

void MainWindow::disconnectController(SessionController* controller)
{
	disconnect( controller , SIGNAL(titleChanged(ViewProperties*))
                     , this , SLOT(activeViewTitleChanged(ViewProperties*)) );

	// KXmlGuiFactory::removeClient() will try to access actions associated
	// with the controller internally, which may not be valid after the controller
	// itself is no longer valid (after the associated session and or view have
	// been destroyed)
	if (controller->isValid())
    	guiFactory()->removeClient(controller);

    controller->setSearchBar(0);
}

void MainWindow::activeViewChanged(SessionController* controller)
{
    // associate bookmark menu with current session
    bookmarkHandler()->setActiveView(controller);
    disconnect( bookmarkHandler() , SIGNAL(openUrl(const KUrl&)) , 0 , 0 );
    connect( bookmarkHandler() , SIGNAL(openUrl(const KUrl&)) , controller ,
             SLOT(openUrl(const KUrl&)) );

    if ( _pluggedController )
		disconnectController(_pluggedController);

    // listen for title changes from the current session
    Q_ASSERT( controller );

    connect( controller , SIGNAL(titleChanged(ViewProperties*)) ,
            this , SLOT(activeViewTitleChanged(ViewProperties*)) );

    controller->setShowMenuAction( _toggleMenuBarAction );
    guiFactory()->addClient(controller);

    // set the current session's search bar
    controller->setSearchBar( searchBar() );

    // update session title to match newly activated session
    activeViewTitleChanged(controller);

    _pluggedController = controller;
}

void MainWindow::activeViewTitleChanged(ViewProperties* properties)
{
    setPlainCaption(properties->title());
}

IncrementalSearchBar* MainWindow::searchBar() const
{
    return _searchBar;
}

void MainWindow::setupActions()
{
    KActionCollection* collection = actionCollection();

    // File Menu
    KAction* newTabAction = collection->addAction("new-tab");
    newTabAction->setIcon( KIcon("tab-new") );
    newTabAction->setText( i18n("New &Tab") );
    newTabAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_N) );
    connect( newTabAction , SIGNAL(triggered()) , this , SLOT(newTab()) );

    KAction* newWindowAction = collection->addAction("new-window");
    newWindowAction->setIcon( KIcon("window-new") );
    newWindowAction->setText( i18n("New &Window") );
    newWindowAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_M) );
    connect( newWindowAction , SIGNAL(triggered()) , this , SLOT(newWindow()) );

    KAction* remoteConnectionAction = collection->addAction("remote-connection");
    remoteConnectionAction->setText( i18n("Remote Connection...") );
    remoteConnectionAction->setIcon( KIcon("network-connect") );
    remoteConnectionAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_R) );
    connect( remoteConnectionAction , SIGNAL(triggered()) , this , SLOT(showRemoteConnectionDialog()) );

    KAction* quitAction = KStandardAction::quit( this , SLOT(close()) , collection );
    // the default shortcut for quit is typically Ctrl+[Some Letter, usually Q] but that is reserved for
    // use by terminal applications
    quitAction->setShortcut(Qt::CTRL+Qt::SHIFT+Qt::Key_Q);

    // Bookmark Menu
    KActionMenu* bookmarkMenu = new KActionMenu(i18n("&Bookmarks") , collection );
    _bookmarkHandler = new BookmarkHandler( collection , bookmarkMenu->menu() , true , this );
    collection->addAction("bookmark" , bookmarkMenu);

	connect( _bookmarkHandler , SIGNAL(openUrls(QList<KUrl>)) , this , SLOT(openUrls(QList<KUrl>)) );

    //TODO - The 'Add Bookmark' menu action currently has a Ctrl+B shortcut by
    // default which cannot be overridden

    // View Menu
    _toggleMenuBarAction = new KToggleAction(this);
    _toggleMenuBarAction->setText( i18n("Show Menu Bar") );
    _toggleMenuBarAction->setIcon( KIcon("show-menu") );
    _toggleMenuBarAction->setChecked( !menuBar()->isHidden() );
    connect( _toggleMenuBarAction , SIGNAL(toggled(bool)) , menuBar() , SLOT(setVisible(bool)) );
    collection->addAction("show-menubar",_toggleMenuBarAction);

    // Hide the Show/Hide menubar item if the menu bar is a MacOS-style menu bar
    if ( menuBar()->isTopLevelMenu() )
        _toggleMenuBarAction->setVisible(false);

    // Full Screen
    KToggleFullScreenAction* fullScreenAction = new KToggleFullScreenAction(this);
    fullScreenAction->setWindow(this);
    fullScreenAction->setShortcut( Qt::CTRL + Qt::SHIFT + Qt::Key_F11 );
    collection->addAction("view-full-screen",fullScreenAction);
    connect( fullScreenAction , SIGNAL(toggled(bool)) , this , SLOT(viewFullScreen(bool)) );

    // Settings Menu
    KStandardAction::configureNotifications( this , SLOT(configureNotifications()) , collection  );
    KStandardAction::keyBindings( this , SLOT(showShortcutsDialog()) , collection  );

    KAction* manageProfilesAction = collection->addAction("manage-profiles");
    manageProfilesAction->setText( i18n("Manage Profiles...") );
    manageProfilesAction->setIcon( KIcon("configure") );
    connect( manageProfilesAction , SIGNAL(triggered()) , this , SLOT(showManageProfilesDialog()) );

}

void MainWindow::viewFullScreen(bool fullScreen)
{
    if ( fullScreen )
        setWindowState( windowState() | Qt::WindowFullScreen );
    else
        setWindowState( windowState() & ~Qt::WindowFullScreen );
}

BookmarkHandler* MainWindow::bookmarkHandler() const
{
    return _bookmarkHandler;
}

void MainWindow::setSessionList(ProfileList* list)
{
    sessionListChanged(list->actions());

    connect( list , SIGNAL(profileSelected(const QString&)) , this ,
            SLOT(newFromProfile(const QString&)) );

    connect( list , SIGNAL(actionsChanged(const QList<QAction*>&)) , this ,
            SLOT(sessionListChanged(const QList<QAction*>&)) );
}

void MainWindow::sessionListChanged(const QList<QAction*>& actions)
{
    unplugActionList("favorite-profiles");
    plugActionList("favorite-profiles",actions);
}

QString MainWindow::activeSessionDir() const
{
    if ( _pluggedController )
        return _pluggedController->currentDir();
    else
        return QString();
}

void MainWindow::openUrls(const QList<KUrl>& urls)
{
	// TODO Implement support for SSH bookmarks here
	foreach( const KUrl& url , urls )
	{
		if ( url.isLocalFile() )
			emit newSessionRequest( _defaultProfile , url.path() , _viewManager );
	}
}

void MainWindow::newTab()
{
    emit newSessionRequest( _defaultProfile , activeSessionDir() , _viewManager);
}

void MainWindow::newWindow()
{
    emit newWindowRequest( _defaultProfile , activeSessionDir() );
}

bool MainWindow::queryClose()
{
    if (kapp->sessionSaving() ||
        _viewManager->viewProperties().count() < 2)
        return true;

    int result = KMessageBox::warningYesNoCancel(this,
                i18n("You have multiple tabs in this window, " 
                     "are you sure you want to quit?"),
                i18n("Confirm close"),
                KStandardGuiItem::quit(),
                KGuiItem(i18n("Close current tab"), "tab-close"),
                KStandardGuiItem::cancel(),
                "CloseAllTabs");

    switch (result) 
    {
    case KMessageBox::Yes:
        return true;
    case KMessageBox::No:
        if (_pluggedController && _pluggedController->session())
		{
			disconnectController(_pluggedController);
			_pluggedController->session()->close();
		}
        return false;
    case KMessageBox::Cancel:
        return false;
    }

    return true;
}

void MainWindow::showShortcutsDialog()
{
    KShortcutsDialog::configure( actionCollection() ,
                                 KShortcutsEditor::LetterShortcutsDisallowed, this );
}

void MainWindow::newFromProfile(const QString& key)
{
    emit newSessionRequest(key, activeSessionDir(), _viewManager);
}
void MainWindow::showManageProfilesDialog()
{
    ManageProfilesDialog* dialog = new ManageProfilesDialog(this);
    dialog->show();
}

void MainWindow::showRemoteConnectionDialog()
{
    RemoteConnectionDialog dialog(this);
    if ( dialog.exec() == QDialog::Accepted )
        emit newSessionRequest(dialog.sessionKey(),QString(),_viewManager);
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

void MainWindow::configureNotifications()
{
    KNotifyConfigWidget::configure( this );
}

#include "MainWindow.moc"

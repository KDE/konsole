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

// System
#include <assert.h>

// KDE
#include <kdebug.h>
#include <KLocale>
#include <KToggleAction>
#include <KXMLGUIFactory>

// Konsole
#include "BookmarkHandler.h"
#include "MainWindow.h"
#include "Session.h"
#include "TerminalDisplay.h"
#include "schema.h"
#include "SessionController.h"
#include "SessionManager.h"
#include "ViewContainer.h"
#include "ViewSplitter.h"
#include "ViewManager.h"

using namespace Konsole;

ViewManager::ViewManager(MainWindow* mainWindow)
    : QObject(mainWindow)
    , _mainWindow(mainWindow)
    , _viewSplitter(0)
    , _pluggedController(0)
{
    // setup actions which relating to the view
    setupActions();

    // create main view area
    _viewSplitter = new ViewSplitter(_mainWindow);

    // emit a signal when all of the views held by this view manager are destroyed
    connect( _viewSplitter , SIGNAL(allContainersEmpty()) , this , SIGNAL(empty()) );
    connect( _viewSplitter , SIGNAL(empty(ViewSplitter*)) , this , SIGNAL(empty()) );
}

ViewManager::~ViewManager()
{
}

QWidget* ViewManager::widget() const
{
    return _viewSplitter;
}

void ViewManager::setupActions()
{
    KActionCollection* collection = _mainWindow->actionCollection();

    _splitViewAction = new KToggleAction( KIcon("view-top-bottom"),i18n("&Split View") , this);
    _splitViewAction->setCheckedState( KGuiItem(i18n("&Remove Split") , KIcon("view-remove") ) );
    _splitViewAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_S) );
    collection->addAction("split-view",_splitViewAction);
    connect( _splitViewAction , SIGNAL(toggled(bool)) , this , SLOT(splitView(bool)));


    QAction* detachViewAction = collection->addAction("detach-view");
    detachViewAction->setIcon( KIcon("tab-breakoff") );
    detachViewAction->setText( i18n("&Detach View") );
    // Ctrl+Shift+D is not used as a shortcut by default because it is too close
    // to Ctrl+D - which will terminate the session in many cases
    detachViewAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_T) );

    connect( detachViewAction , SIGNAL(triggered()) , this , SLOT(detachActiveView()) );

    QAction* mergeAction = collection->addAction("merge-windows");
    mergeAction->setText( i18n("&Merge Windows") );
        
    connect( mergeAction , SIGNAL(triggered()) , _mainWindow , SLOT(mergeWindows()) );


    KAction* nextViewAction = new KAction( i18n("Next View") , this );
    collection->addAction("next-view",nextViewAction);
    KShortcut nextViewShortcut = nextViewAction->shortcut();
    nextViewShortcut.setAlternate( QKeySequence(Qt::SHIFT+Qt::Key_Right) );
    nextViewShortcut.setPrimary( QKeySequence(Qt::CTRL+Qt::Key_PageUp) );
    nextViewAction->setShortcut(nextViewShortcut); 
    connect( nextViewAction, SIGNAL(triggered()) , this , SLOT(nextView()) );
    _mainWindow->addAction(nextViewAction);

    KAction* previousViewAction = new KAction( i18n("Previous View") , this );
    collection->addAction("previous-view",previousViewAction);
    KShortcut previousViewShortcut = previousViewAction->shortcut();
    previousViewShortcut.setPrimary( QKeySequence(Qt::SHIFT+Qt::Key_Left) );
    previousViewShortcut.setAlternate( QKeySequence(Qt::CTRL+Qt::Key_PageDown) );
    previousViewAction->setShortcut(previousViewShortcut);
    connect( previousViewAction, SIGNAL(triggered()) , this , SLOT(previousView()) );
    _mainWindow->addAction(previousViewAction);

    QAction* nextContainerAction = collection->addAction("next-container");
    nextContainerAction->setText( i18n("Next View Container") );
    nextContainerAction->setShortcut( QKeySequence(Qt::SHIFT+Qt::Key_Tab) );
    connect( nextContainerAction , SIGNAL(triggered()) , this , SLOT(nextContainer()) );
    _mainWindow->addAction(nextContainerAction);
}

void ViewManager::nextContainer()
{
    _viewSplitter->activateNextContainer();
}

void ViewManager::nextView()
{
    ViewContainer* container = _viewSplitter->activeContainer();

    Q_ASSERT( container );

    container->activateNextView();
}

void ViewManager::previousView()
{
    ViewContainer* container = _viewSplitter->activeContainer();

    Q_ASSERT( container );

    container->activatePreviousView();
}

void ViewManager::detachActiveView()
{
    // find the currently active view and remove it from its container 
    ViewContainer* container = _viewSplitter->activeContainer();
    TerminalDisplay* activeView = dynamic_cast<TerminalDisplay*>(container->activeView());

    if (!activeView)
        return;

    emit viewDetached(_sessionMap[activeView]);
    
    _sessionMap.remove(activeView);

    // remove the view from this window
    container->removeView(activeView);
    delete activeView;


    // if the container from which the view was removed is now empty then it can be deleted,
    // unless it is the only container in the window, in which case it is left empty
    // so that there is always an active container
    if ( _viewSplitter->containers().count() > 1 && 
         container->views().count() == 0 )
    {
        delete container;

        // this will need to be removed if Konsole is modified so the menu item to
        // split the view is no longer one toggle-able item
        _splitViewAction->setChecked(false);
    }

}

void ViewManager::sessionFinished( Session* session )
{
    QList<TerminalDisplay*> children = _viewSplitter->findChildren<TerminalDisplay*>();

    foreach ( TerminalDisplay* view , children )
    {
        if ( _sessionMap[view] == session )
        {
            _sessionMap.remove(view);
            delete view;
        }
    }

    focusActiveView(); 
}

void ViewManager::focusActiveView()
{
    // give the active view in a container the focus.  this ensures 
    // that controller associated with that view is activated and the session-specific
    // menu items are replaced with the ones for the newly focused view

    // see the viewFocused() method

    ViewContainer* container = _viewSplitter->activeContainer(); 
    if ( container )
    {
        QWidget* activeView = container->activeView();
        if ( activeView )
        {
            activeView->setFocus(Qt::MouseFocusReason);
        }
    }
}


void ViewManager::viewActivated( QWidget* view )
{
    Q_ASSERT( view != 0 );

    // focus the activated view, this will cause the SessionController
    // to notify the world that the view has been focused and the appropriate UI
    // actions will be plugged in.
    view->setFocus(Qt::OtherFocusReason);
}

void ViewManager::activeViewTitleChanged(ViewProperties* properties)
{
    // set a plain caption (ie. without the automatic addition of " - AppName" at the end)
    // to make the taskbar entry cleaner and easier to read
    _mainWindow->setPlainCaption( properties->title() );
}

void ViewManager::viewFocused( SessionController* controller )
{
    // if a view is given the focus which is different to the one for which menu items
    // are currently being shown then unplug the current session-specific menu items
    // and plug in the ones for the newly focused session

    if ( _pluggedController != controller )
    {
        // remove existing session specific menu items if there are any
        if ( _pluggedController )
        {
            _mainWindow->guiFactory()->removeClient(_pluggedController);
            disconnect( controller , SIGNAL(titleChanged(ViewProperties*)),
                        this , SLOT(activeViewTitleChanged(ViewProperties*)) );
            disconnect( _mainWindow->bookmarkHandler() , 0 , controller , 0 );
            _pluggedController->setSearchBar( 0 );
        }
        
        _pluggedController = controller;

        // update the menus in the main window to use the actions from the active
        // controller 
        _mainWindow->guiFactory()->addClient(controller);
      
        // make the bookmark menu operate on the currently focused session 
        _mainWindow->bookmarkHandler()->setController(controller);
        connect( _mainWindow->bookmarkHandler() , SIGNAL(openUrl(const KUrl&)) , controller , 
                SLOT(openUrl(const KUrl&)) );

        // associated main window's search bar with session
        controller->setSearchBar( _mainWindow->searchBar() );

        // update the caption of the main window to match that of the focused session
        activeViewTitleChanged(controller);

        // listen for future changes in the focused session's title
        connect( controller , SIGNAL(titleChanged(ViewProperties*)),
                 this       , SLOT(activeViewTitleChanged(ViewProperties*)) );        
        
    

        //kDebug() << "Plugged actions for " << controller->session()->displayTitle() << endl;
    }
}

void ViewManager::splitView(bool splitView)
{
    if (splitView)
    {
        // iterate over each session which has a view in the current active
        // container and create a new view for that session in a new container 
        QListIterator<QWidget*> existingViewIter(_viewSplitter->activeContainer()->views());
        
        ViewContainer* container = createContainer(); 

        while (existingViewIter.hasNext())
        {
            Session* session = _sessionMap[(TerminalDisplay*)existingViewIter.next()];
            TerminalDisplay* display = createTerminalDisplay();
            loadViewSettings(display,session); 
            ViewProperties* properties = createController(session,display);

            _sessionMap[display] = session;

            container->addView(display,properties);
            session->addView( display );
        }

        _viewSplitter->addContainer(container,Qt::Vertical);
    }
    else
    {
        // delete the active container when unsplitting the view unless it is the last
        // one
        if ( _viewSplitter->containers().count() > 1 )
        {
            ViewContainer* container = _viewSplitter->activeContainer();
        
            delete container;
        }
    }

    // ensure that the active view is focused after the split / unsplit
    _viewSplitter->activeContainer()->activeView()->setFocus(Qt::OtherFocusReason);
}

SessionController* ViewManager::createController(Session* session , TerminalDisplay* view)
{
    // create a new controller for the session, and ensure that this view manager
    // is notified when the view gains the focus
    SessionController* controller = new SessionController(session,view,this);
    connect( controller , SIGNAL(focused(SessionController*)) , this , SLOT(viewFocused(SessionController*)) );

    return controller;
}

void ViewManager::createView(Session* session)
{
    // create the default container
    if (_viewSplitter->containers().count() == 0)
    {
        _viewSplitter->addContainer( createContainer() , Qt::Vertical );
    }

    // notify this view manager when the session finishes so that its view
    // can be deleted
    connect( session , SIGNAL(done(Session*)) , this , SLOT(sessionFinished(Session*)) );
   
    // iterate over the view containers owned by this view manager
    // and create a new terminal display for the session in each of them, along with
    // a controller for the session/display pair 
    ViewContainer* const activeContainer = _viewSplitter->activeContainer();
    QListIterator<ViewContainer*> containerIter(_viewSplitter->containers());

    while ( containerIter.hasNext() )
    {
        ViewContainer* container = containerIter.next();
        TerminalDisplay* display = createTerminalDisplay();
        loadViewSettings(display,session);
        ViewProperties* properties = createController(session,display);

        _sessionMap[display] = session; 
        container->addView(display,properties);
        session->addView(display);

        if ( container == activeContainer ) 
        {
            container->setActiveView(display);
            display->setFocus( Qt::OtherFocusReason );
        }
    }
}

ViewContainer* ViewManager::createContainer()
{
    ViewContainer* container = new TabbedViewContainerV2(_viewSplitter); 
/*
    if ( _mainWindow->factory() )
    {
        QMenu* menu = (QMenu*)_mainWindow->factory()->container("new-session-popup",_mainWindow);
        
        if ( menu )
            container->setNewSessionMenu(menu);
    }
    else
    {
       kDebug() << __FILE__ << __LINE__ << ": ViewManager attempted to create a view before" <<
          " the main window GUI was created - unable to create popup menus for container." << endl;  
    }
*/

    // connect signals and slots
    connect( container , SIGNAL(closeRequest(QWidget*)) , this , SLOT(viewCloseRequest(QWidget*)) );

    connect( container , SIGNAL(activeViewChanged(QWidget*)) , this , SLOT(viewActivated(QWidget*)));
    return container;
}

void ViewManager::viewCloseRequest(QWidget* view)
{
    // 1. detach view from session
    // 2. if the session has no views left, close it
    
    TerminalDisplay* display = (TerminalDisplay*)view;
    Session* session = _sessionMap[ display ];
    if ( session )
    {
        delete display;
        
        if ( session->views().count() == 0 )
            session->closeSession();
    }
    else
    {
        kDebug() << __FILE__ << __LINE__ << ": received close request from unknown view." << endl;
    }
}

void ViewManager::merge(ViewManager* otherManager)
{
    // iterate over the views in otherManager's active container and take them from that
    // manager and put them in the active container in this manager
    //
    // TODO - This currently does not consider views in containers other than
    //        the active one in the other manager
    //
    ViewSplitter* otherSplitter = otherManager->_viewSplitter;
    ViewContainer* otherContainer = otherSplitter->activeContainer();

    QListIterator<QWidget*> otherViewIter(otherContainer->views());

    ViewContainer* activeContainer = _viewSplitter->activeContainer();

    while ( otherViewIter.hasNext() )
    {
        TerminalDisplay* view = dynamic_cast<TerminalDisplay*>(otherViewIter.next());
        
        assert(view);

        takeView(otherManager,otherContainer,activeContainer,view);
    } 
}


void ViewManager::takeView(ViewManager* otherManager , ViewContainer* otherContainer, 
                           ViewContainer* newContainer, TerminalDisplay* view)
{
    // FIXME - the controller associated with the display which is being moved
    //         may have signals which are connected to otherManager.  they need
    //         to be redirected to slots in this view manager
    ViewProperties* properties = otherContainer->viewProperties(view);
    otherContainer->removeView(view);

    newContainer->addView(view,properties);

    // transfer the session map entries
    _sessionMap.insert(view,otherManager->_sessionMap[view]);
    otherManager->_sessionMap.remove(view);
}

TerminalDisplay* ViewManager::createTerminalDisplay()
{
   TerminalDisplay* display = new TerminalDisplay(0);

   //TODO Temporary settings used here
   display->setBellMode(0);
   display->setVTFont( QFont("Monospace") );
   display->setTerminalSizeHint(false);
   display->setCutToBeginningOfLine(true);
   display->setTerminalSizeStartup(false);
   display->setSize(80,40);
   display->setScrollbarLocation(TerminalDisplay::SCROLLBAR_RIGHT);

   return display;
}

void ViewManager::loadViewSettings(TerminalDisplay* view , Session* session)
{
    // load colour scheme
    view->setColorTable( session->schema()->table() );

}

#include "ViewManager.moc"

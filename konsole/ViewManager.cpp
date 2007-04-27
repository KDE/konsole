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

// Qt
#include <QSignalMapper>

// KDE
#include <kdebug.h>
#include <KLocale>
#include <KToggleAction>
#include <KXMLGUIFactory>

// Konsole
#include "BookmarkHandler.h"
#include "ColorScheme.h"
#include "Session.h"
#include "TerminalDisplay.h"
#include "SessionController.h"
#include "SessionManager.h"
#include "ViewContainer.h"
#include "ViewSplitter.h"
#include "ViewManager.h"

using namespace Konsole;

ViewManager::ViewManager(QObject* parent , KActionCollection* collection)
    : QObject(parent)
    , _viewSplitter(0)
    , _actionCollection(collection)
    , _containerSignalMapper(new QSignalMapper(this))
{
    // create main view area
    _viewSplitter = new ViewSplitter(0);   
    // the ViewSplitter class supports both recursive and non-recursive splitting,
    // in non-recursive mode, all containers are inserted into the same top-level splitter
    // widget, and all the divider lines between the containers have the same orientation
    //
    // the ViewManager class is not currently able to handle a ViewSplitter in recursive-splitting
    // mode 
    _viewSplitter->setRecursiveSplitting(false);

    // setup actions which relating to the view
    setupActions();

    // emit a signal when all of the views held by this view manager are destroyed
    connect( _viewSplitter , SIGNAL(allContainersEmpty()) , this , SIGNAL(empty()) );
    connect( _viewSplitter , SIGNAL(empty(ViewSplitter*)) , this , SIGNAL(empty()) );

    // listen for addition or removal of views from associated containers
    connect( _containerSignalMapper , SIGNAL(mapped(QObject*)) , this , 
            SLOT(containerViewsChanged(QObject*)) ); 
}

ViewManager::~ViewManager()
{
}
QWidget* ViewManager::activeView() const
{
    ViewContainer* container = _viewSplitter->activeContainer();
    if ( container )
    {
        return container->activeView();
    }
    else
    {
        return 0;
    }
}

QWidget* ViewManager::widget() const
{
    return _viewSplitter;
}

void ViewManager::setupActions()
{
    KActionCollection* collection = _actionCollection;

    KAction* nextViewAction = new KAction( i18n("Next View") , this );
    KAction* previousViewAction = new KAction( i18n("Previous View") , this );
    QAction* nextContainerAction = new QAction( i18n("Next View Container") , this);
    
    if ( collection )
    {
        KAction* splitLeftRightAction = new KAction( KIcon("view-left-right"),
                                                      i18n("Split View Left/Right"),
                                                      this );
        splitLeftRightAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_L) );
        collection->addAction("split-view-left-right",splitLeftRightAction);
        connect( splitLeftRightAction , SIGNAL(triggered()) , this , SLOT(splitLeftRight()) );

        KAction* splitTopBottomAction = new KAction( KIcon("view-top-bottom") , 
                                             i18n("Split View Top/Bottom"),this);
        splitTopBottomAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_T) );
        collection->addAction("split-view-top-bottom",splitTopBottomAction);
        connect( splitTopBottomAction , SIGNAL(triggered()) , this , SLOT(splitTopBottom()));

        KAction* closeActiveAction = new KAction( i18n("Close Active") , this );
        closeActiveAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_S) );
        closeActiveAction->setEnabled(false);
        collection->addAction("close-active-view",closeActiveAction);
        connect( closeActiveAction , SIGNAL(triggered()) , this , SLOT(closeActiveView()) );
        connect( this , SIGNAL(splitViewToggle(bool)) , closeActiveAction , SLOT(setEnabled(bool)) );
        
        KAction* closeOtherAction = new KAction( i18n("Close Others") , this );
        closeOtherAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_O) );
        closeOtherAction->setEnabled(false);
        collection->addAction("close-other-views",closeOtherAction);
        connect( closeOtherAction , SIGNAL(triggered()) , this , SLOT(closeOtherViews()) );
        connect( this , SIGNAL(splitViewToggle(bool)) , closeOtherAction , SLOT(setEnabled(bool)) );

        QAction* detachViewAction = collection->addAction("detach-view");
        detachViewAction->setIcon( KIcon("tab-breakoff") );
        detachViewAction->setText( i18n("&Detach View") );
        // Ctrl+Shift+D is not used as a shortcut by default because it is too close
        // to Ctrl+D - which will terminate the session in many cases
        detachViewAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_H) );

        connect( detachViewAction , SIGNAL(triggered()) , this , SLOT(detachActiveView()) );
    
        collection->addAction("next-view",nextViewAction);
        collection->addAction("previous-view",previousViewAction);
        collection->addAction("next-container",nextContainerAction);

    }

    KShortcut nextViewShortcut = nextViewAction->shortcut();
    nextViewShortcut.setAlternate( QKeySequence(Qt::SHIFT+Qt::Key_Right) );
    nextViewShortcut.setPrimary( QKeySequence(Qt::CTRL+Qt::Key_PageUp) );
    nextViewAction->setShortcut(nextViewShortcut); 
    connect( nextViewAction, SIGNAL(triggered()) , this , SLOT(nextView()) );
    _viewSplitter->addAction(nextViewAction);

    KShortcut previousViewShortcut = previousViewAction->shortcut();
    previousViewShortcut.setPrimary( QKeySequence(Qt::SHIFT+Qt::Key_Left) );
    previousViewShortcut.setAlternate( QKeySequence(Qt::CTRL+Qt::Key_PageDown) );
    previousViewAction->setShortcut(previousViewShortcut);
    connect( previousViewAction, SIGNAL(triggered()) , this , SLOT(previousView()) );
    _viewSplitter->addAction(previousViewAction);

    nextContainerAction->setShortcut( QKeySequence(Qt::SHIFT+Qt::Key_Tab) );
    connect( nextContainerAction , SIGNAL(triggered()) , this , SLOT(nextContainer()) );
    _viewSplitter->addAction(nextContainerAction);
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
    activeView->deleteLater();

    // if the container from which the view was removed is now empty then it can be deleted,
    // unless it is the only container in the window, in which case it is left empty
    // so that there is always an active container
    if ( _viewSplitter->containers().count() > 1 && 
         container->views().count() == 0 )
    {
        removeContainer(container);

        // this will need to be removed if Konsole is modified so the menu item to
        // split the view is no longer one toggle-able item
        //_splitViewAction->setChecked(false);
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

void ViewManager::splitLeftRight()
{
    splitView(Qt::Horizontal);
}
void ViewManager::splitTopBottom()
{
    splitView(Qt::Vertical);
}

void ViewManager::splitView(Qt::Orientation orientation)
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

    _viewSplitter->addContainer(container,orientation);
    emit splitViewToggle(_viewSplitter->containers().count() > 0);

    // focus the new container
    container->containerWidget()->setFocus();

    // ensure that the active view is focused after the split / unsplit
    _viewSplitter->activeContainer()->activeView()->setFocus(Qt::OtherFocusReason);
}
void ViewManager::removeContainer(ViewContainer* container)
{
    delete container;
    emit splitViewToggle(_viewSplitter->containers().count() > 1);
}
void ViewManager::closeActiveView()
{
    // only do something if there is more than one container active
    if ( _viewSplitter->containers().count() > 1 )
    {
        ViewContainer* container = _viewSplitter->activeContainer();

        removeContainer(container);

        // focus next container so that user can continue typing 
        // without having to manually focus it themselves
        nextContainer();
    }
}
void ViewManager::closeOtherViews()
{
    ViewContainer* active = _viewSplitter->activeContainer();

    QListIterator<ViewContainer*> iter(_viewSplitter->containers());
    while ( iter.hasNext() )
    {
        ViewContainer* next = iter.next();
        if ( next != active )
            delete next;
    }
}

SessionController* ViewManager::createController(Session* session , TerminalDisplay* view)
{
    // create a new controller for the session, and ensure that this view manager
    // is notified when the view gains the focus
    SessionController* controller = new SessionController(session,view,this);
    connect( controller , SIGNAL(focused(SessionController*)) , this , SIGNAL(activeViewChanged(SessionController*)) );
    connect( session , SIGNAL(destroyed()) , controller , SLOT(deleteLater()) );
    connect( view , SIGNAL(destroyed()) , controller , SLOT(deleteLater()) );
    
    return controller;
}

void ViewManager::createView(Session* session)
{
    // create the default container
    if (_viewSplitter->containers().count() == 0)
    {
        _viewSplitter->addContainer( createContainer() , Qt::Vertical );
        emit splitViewToggle(false);
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

    // connect signals and slots
    connect( container , SIGNAL(viewAdded(QWidget*,ViewProperties*)) , _containerSignalMapper ,
           SLOT(map()) );
    connect( container , SIGNAL(viewRemoved(QWidget*)) , _containerSignalMapper ,
           SLOT(map()) ); 
    _containerSignalMapper->setMapping(container,container);

    connect( container , SIGNAL(viewRemoved(QWidget*)) , this , SLOT(viewCloseRequest(QWidget*)) );
    connect( container , SIGNAL(closeRequest(QWidget*)) , this , SLOT(viewCloseRequest(QWidget*)) );
    connect( container , SIGNAL(activeViewChanged(QWidget*)) , this , SLOT(viewActivated(QWidget*)));
    
    return container;
}

void ViewManager::containerViewsChanged(QObject* container)
{
    qDebug() << "Container views changed";

    if ( container == _viewSplitter->activeContainer() )
    {
        emit viewPropertiesChanged( viewProperties() );
    } 
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

    qDebug() << "Closing view";
    focusActiveView();
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
   display->setTerminalSizeHint(false);
   display->setCutToBeginningOfLine(true);
   display->setTerminalSizeStartup(false);
   display->setScrollBarLocation(TerminalDisplay::SCROLLBAR_RIGHT);

   return display;
}

void ViewManager::loadViewSettings(TerminalDisplay* view , Session* session)
{
    SessionInfo* info = SessionManager::instance()->sessionType(session->type());
    Q_ASSERT( info );

    const ColorScheme* colorScheme = ColorSchemeManager::instance()->
                                            findColorScheme(info->colorScheme());
    
    Q_ASSERT( colorScheme );

    // load colour scheme
    view->setColorTable(colorScheme->colorTable());
    
    // load font, fall back to system monospace font if not specified
    view->setVTFont(info->defaultFont());
   
    // set initial size
    // temporary default used for now
    view->setSize(80,40);
}

QList<ViewProperties*> ViewManager::viewProperties() const
{
    QList<ViewProperties*> list;

    ViewContainer* container = _viewSplitter->activeContainer();

    Q_ASSERT( container );

    QListIterator<QWidget*> viewIter(container->views());
    while ( viewIter.hasNext() )
    {
        ViewProperties* properties = container->viewProperties(viewIter.next());        Q_ASSERT( properties );
        list << properties; 
    } 

    return list;
}

#include "ViewManager.moc"

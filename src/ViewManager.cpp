/*
    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>

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
#include "ViewManager.h"

// System
#include <assert.h>

// Qt
#include <QtCore/QDateTime>
#include <QtCore/QSignalMapper>
#include <QtGui/QMenu>

// KDE
#include <kdebug.h>
#include <KAcceleratorManager>
#include <KGlobal>
#include <KLocale>
#include <KToggleAction>
#include <KXMLGUIFactory>
#include <QStringList>

// Konsole
#include "ColorScheme.h"
#include "ProfileList.h"
#include "Session.h"
#include "TerminalDisplay.h"
#include "SessionController.h"
#include "SessionManager.h"
#include "ViewContainer.h"
#include "ViewSplitter.h"
#include "konsoleadaptor.h"
#include "Profile.h"

using namespace Konsole;

ViewManager::ViewManager(QObject* parent , KActionCollection* collection)
    : QObject(parent)
    , _viewSplitter(0)
    , _actionCollection(collection)
    , _containerSignalMapper(new QSignalMapper(this))
    , _navigationMethod(TabbedNavigation)
    , _newViewMenu(0)
{
    // create main view area
    _viewSplitter = new ViewSplitter(0);  
    KAcceleratorManager::setNoAccel(_viewSplitter);

    // the ViewSplitter class supports both recursive and non-recursive splitting,
    // in non-recursive mode, all containers are inserted into the same top-level splitter
    // widget, and all the divider lines between the containers have the same orientation
    //
    // the ViewManager class is not currently able to handle a ViewSplitter in recursive-splitting
    // mode 
    _viewSplitter->setRecursiveSplitting(false);
    _viewSplitter->setFocusPolicy(Qt::NoFocus);

    // setup actions which relating to the view
    setupActions();

    // emit a signal when all of the views held by this view manager are destroyed
    connect( _viewSplitter , SIGNAL(allContainersEmpty()) , this , SIGNAL(empty()) );
    connect( _viewSplitter , SIGNAL(empty(ViewSplitter*)) , this , SIGNAL(empty()) );

    // listen for addition or removal of views from associated containers
    connect( _containerSignalMapper , SIGNAL(mapped(QObject*)) , this , 
            SLOT(containerViewsChanged(QObject*)) ); 

    // listen for profile changes
    connect( SessionManager::instance() , SIGNAL(profileChanged(Profile::Ptr)) , this,
            SLOT(profileChanged(Profile::Ptr)) );
    connect( SessionManager::instance() , SIGNAL(sessionUpdated(Session*)) , this,
            SLOT(updateViewsForSession(Session*)) );

    //prepare DBus communication
    new KonsoleAdaptor(this);
    QDBusConnection::sessionBus().registerObject(QLatin1String("/Konsole"), this);

}

ViewManager::~ViewManager()
{
    delete _newViewMenu;
}
QMenu* ViewManager::createNewViewMenu() 
{
    if (_newViewMenu)
        return _newViewMenu;

    _newViewMenu = new QMenu(0);
    ProfileList* newViewProfiles = new ProfileList(false,_newViewMenu);
    newViewProfiles->syncWidgetActions(_newViewMenu,true);
    connect(newViewProfiles,SIGNAL(profileSelected(Profile::Ptr)),this,
        SIGNAL(newViewRequest(Profile::Ptr)));

    return _newViewMenu;
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
    KAction* nextContainerAction = new KAction( i18n("Next View Container") , this);
  
    KAction* moveViewLeftAction = new KAction( i18n("Move View Left") , this );
    KAction* moveViewRightAction = new KAction( i18n("Move View Right") , this );

    // list of actions that should only be enabled when there are multiple view
    // containers open
    QList<QAction*> multiViewOnlyActions;
    multiViewOnlyActions << nextContainerAction;

    if ( collection )
    {
        KAction* splitLeftRightAction = new KAction( KIcon("view-split-left-right"),
                                                      i18nc("@action:inmenu", "Split View Left/Right"),
                                                      this );
        splitLeftRightAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_L) );
        collection->addAction("split-view-left-right",splitLeftRightAction);
        connect( splitLeftRightAction , SIGNAL(triggered()) , this , SLOT(splitLeftRight()) );

        KAction* splitTopBottomAction = new KAction( KIcon("view-split-top-bottom") , 
                                             i18nc("@action:inmenu", "Split View Top/Bottom"),this);
        splitTopBottomAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_T) );
        collection->addAction("split-view-top-bottom",splitTopBottomAction);
        connect( splitTopBottomAction , SIGNAL(triggered()) , this , SLOT(splitTopBottom()));

        KAction* closeActiveAction = new KAction( i18nc("@action:inmenu Close Active View", "Close Active") , this );
        closeActiveAction->setIcon(KIcon("view-close"));
        closeActiveAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_S) );
        closeActiveAction->setEnabled(false);
        collection->addAction("close-active-view",closeActiveAction);
        connect( closeActiveAction , SIGNAL(triggered()) , this , SLOT(closeActiveView()) );
      
        multiViewOnlyActions << closeActiveAction; 

        KAction* closeOtherAction = new KAction( i18nc("@action:inmenu Close Other Views", "Close Others") , this );
        closeOtherAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_O) );
        closeOtherAction->setEnabled(false);
        collection->addAction("close-other-views",closeOtherAction);
        connect( closeOtherAction , SIGNAL(triggered()) , this , SLOT(closeOtherViews()) );

        multiViewOnlyActions << closeOtherAction;

        KAction* detachViewAction = collection->addAction("detach-view");
        detachViewAction->setIcon( KIcon("tab-detach") );
        detachViewAction->setText( i18n("&Detach View") );
        // Ctrl+Shift+D is not used as a shortcut by default because it is too close
        // to Ctrl+D - which will terminate the session in many cases
        detachViewAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_H) );
       
          connect( this , SIGNAL(splitViewToggle(bool)) , this , SLOT(updateDetachViewState()) ); 
        connect( detachViewAction , SIGNAL(triggered()) , this , SLOT(detachActiveView()) );
   
        // Expand & Shrink Active View
        KAction* expandActiveAction = new KAction( i18nc("@action:inmenu", "Expand View") , this );
        expandActiveAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_BracketRight) );
        collection->addAction("expand-active-view",expandActiveAction);
        connect( expandActiveAction , SIGNAL(triggered()) , this , SLOT(expandActiveView()) );

        multiViewOnlyActions << expandActiveAction;

        KAction* shrinkActiveAction = new KAction( i18nc("@action:inmenu", "Shrink View") , this );
        shrinkActiveAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_BracketLeft) );
        collection->addAction("shrink-active-view",shrinkActiveAction);
        connect( shrinkActiveAction , SIGNAL(triggered()) , this , SLOT(shrinkActiveView()) );

        multiViewOnlyActions << shrinkActiveAction;

        // Next / Previous View , Next Container
        collection->addAction("next-view",nextViewAction);
        collection->addAction("previous-view",previousViewAction);
        collection->addAction("next-container",nextContainerAction);
        collection->addAction("move-view-left",moveViewLeftAction);
        collection->addAction("move-view-right",moveViewRightAction);

        // Switch to tab N shortcuts
        const int SWITCH_TO_TAB_COUNT = 10;
        QSignalMapper* switchToTabMapper = new QSignalMapper(this);
        connect(switchToTabMapper,SIGNAL(mapped(int)),this,SLOT(switchToView(int)));
        for (int i=0;i < SWITCH_TO_TAB_COUNT;i++)
        {
            KAction* switchToTabAction = new KAction(i18n("Switch to Tab %1",i+1),this);
            switchToTabMapper->setMapping(switchToTabAction,i);
            connect(switchToTabAction,SIGNAL(triggered()),switchToTabMapper,
                    SLOT(map()));
            collection->addAction(QString("switch-to-tab-%1").arg(i),switchToTabAction);
        }
    }

    QListIterator<QAction*> iter(multiViewOnlyActions);
    while ( iter.hasNext() )
    {
        connect( this , SIGNAL(splitViewToggle(bool)) , iter.next() , SLOT(setEnabled(bool)) );
    }

    // keyboard shortcut only actions
    KShortcut nextViewShortcut = nextViewAction->shortcut();
    nextViewShortcut.setPrimary( QKeySequence(Qt::SHIFT+Qt::Key_Right) );
    nextViewShortcut.setAlternate( QKeySequence(Qt::CTRL+Qt::Key_PageUp) );
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

    moveViewLeftAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_Left) );
    connect( moveViewLeftAction , SIGNAL(triggered()) , this , SLOT(moveActiveViewLeft()) );
    _viewSplitter->addAction(moveViewLeftAction);
    moveViewRightAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_Right) );
    connect( moveViewRightAction , SIGNAL(triggered()) , this , SLOT(moveActiveViewRight()) );
    _viewSplitter->addAction(moveViewRightAction);
}
void ViewManager::switchToView(int index)
{
    Q_ASSERT(index >= 0);
    ViewContainer* container = _viewSplitter->activeContainer();
    Q_ASSERT( container );
    QList<QWidget*> containerViews = container->views();
    if (index >= containerViews.count())
        return;
    container->setActiveView(containerViews.at(index));
}
void ViewManager::updateDetachViewState()
{
    if (!_actionCollection)
        return;


    bool splitView = _viewSplitter->containers().count() >= 2;
    bool shouldEnable = splitView || _viewSplitter->activeContainer()->views().count() >= 2;

    QAction* detachAction = _actionCollection->action("detach-view");

    if ( detachAction && shouldEnable != detachAction->isEnabled() )
        detachAction->setEnabled(shouldEnable);
}
void ViewManager::moveActiveViewLeft()
{
    ViewContainer* container = _viewSplitter->activeContainer();
    Q_ASSERT( container );
    container->moveActiveView( ViewContainer::MoveViewLeft );
}
void ViewManager::moveActiveViewRight()
{
    ViewContainer* container = _viewSplitter->activeContainer();
    Q_ASSERT( container );
    container->moveActiveView( ViewContainer::MoveViewRight );
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
    }

}

void ViewManager::sessionFinished()
{
    // if this slot is called after the view manager's main widget
    // has been destroyed, do nothing
    if (!_viewSplitter)
        return;

    Session* session = qobject_cast<Session*>(sender());

    if ( _sessionMap[qobject_cast<TerminalDisplay*>(activeView())] == session )
    {
        // switch to the next view before deleting the session views to prevent flicker 
        // occurring as a result of an interval between removing the active view and switching
        // to the next view
        nextView();
    }

    Q_ASSERT(session);

    // close attached views
    QList<TerminalDisplay*> children = _viewSplitter->findChildren<TerminalDisplay*>();

    foreach ( TerminalDisplay* view , children )
    {
        if ( _sessionMap[view] == session )
        {
            _sessionMap.remove(view);
            view->deleteLater();
        }
    }

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
    
    ViewContainer* container = 0; 

    while (existingViewIter.hasNext())
    {
        Session* session = _sessionMap[(TerminalDisplay*)existingViewIter.next()];
        TerminalDisplay* display = createTerminalDisplay(session);
        applyProfile(display,SessionManager::instance()->sessionProfile(session),false);
        ViewProperties* properties = createController(session,display);

        _sessionMap[display] = session;

        // create a container using settings from the first 
        // session in the previous container
        if ( !container )
            container = createContainer(SessionManager::instance()->sessionProfile(session));

        container->addView(display,properties);
        session->addView( display );
    }

    _viewSplitter->addContainer(container,orientation);
    emit splitViewToggle(_viewSplitter->containers().count() > 0);

    // focus the new container
    container->containerWidget()->setFocus();

    // ensure that the active view is focused after the split / unsplit
    ViewContainer* activeContainer = _viewSplitter->activeContainer();
    QWidget* activeView = activeContainer ? activeContainer->activeView() : 0;

    if ( activeView )
        activeView->setFocus(Qt::OtherFocusReason);
}
void ViewManager::removeContainer(ViewContainer* container)
{
    // remove session map entries for views in this container
    foreach( QWidget* view , container->views() )
    {
        TerminalDisplay* display = qobject_cast<TerminalDisplay*>(view);
        Q_ASSERT(display);
        _sessionMap.remove(display);
    } 

    _viewSplitter->removeContainer(container);
    container->deleteLater();

    emit splitViewToggle( _viewSplitter->containers().count() > 1 );
}
void ViewManager::expandActiveView()
{
    _viewSplitter->adjustContainerSize(_viewSplitter->activeContainer(),10);
}
void ViewManager::shrinkActiveView()
{
    _viewSplitter->adjustContainerSize(_viewSplitter->activeContainer(),-10);
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
            removeContainer(next);
    }
}

SessionController* ViewManager::createController(Session* session , TerminalDisplay* view)
{
    // create a new controller for the session, and ensure that this view manager
    // is notified when the view gains the focus
    SessionController* controller = new SessionController(session,view,this);
    connect( controller , SIGNAL(focused(SessionController*)) , this , SLOT(controllerChanged(SessionController*)) );
    connect( session , SIGNAL(destroyed()) , controller , SLOT(deleteLater()) );
    connect( view , SIGNAL(destroyed()) , controller , SLOT(deleteLater()) );

    // if this is the first controller created then set it as the active controller
    if (!_pluggedController)
        controllerChanged(controller);

    return controller;
}

void ViewManager::controllerChanged(SessionController* controller)
{
    if ( controller == _pluggedController )
        return;

    _viewSplitter->setFocusProxy(controller->view());

    _pluggedController = controller;
    emit activeViewChanged(controller);
}

SessionController* ViewManager::activeViewController() const
{
    return _pluggedController;
}

IncrementalSearchBar* ViewManager::searchBar() const
{
    return _viewSplitter->activeSplitter()->activeContainer()->searchBar();
}

void ViewManager::createView(Session* session, ViewContainer* container, int index)
{
    // notify this view manager when the session finishes so that its view
    // can be deleted
    //
    // TODO - Find a more efficient a way to avoid multiple connections
    disconnect( session , SIGNAL(finished()) , this , SLOT(sessionFinished()) );
    connect( session , SIGNAL(finished()) , this , SLOT(sessionFinished()) );

     bool isFirst = _sessionMap.isEmpty();
     TerminalDisplay* display = createTerminalDisplay(session);
     applyProfile(display,SessionManager::instance()->sessionProfile(session),isFirst);
     
     // set initial size
     display->setSize(80,40);

     ViewProperties* properties = createController(session,display);

     _sessionMap[display] = session; 
     container->addView(display,properties,index);
     session->addView(display);

     // tell the session whether it has a light or dark background
     const Profile::Ptr profile = SessionManager::instance()->sessionProfile(session);
     session->setDarkBackground( colorSchemeForProfile(profile)->hasDarkBackground() );

     if ( container == _viewSplitter->activeContainer() ) 
     {
         container->setActiveView(display);
         display->setFocus( Qt::OtherFocusReason );
     }
    
     updateDetachViewState();
}

void ViewManager::createView(Session* session)
{
    // create the default container
    if (_viewSplitter->containers().count() == 0)
    {
        _viewSplitter->addContainer( createContainer(SessionManager::instance()->sessionProfile(session)) , 
                                     Qt::Vertical );
        emit splitViewToggle(false);
    }

       
    // iterate over the view containers owned by this view manager
    // and create a new terminal display for the session in each of them, along with
    // a controller for the session/display pair 
    QListIterator<ViewContainer*> containerIter(_viewSplitter->containers());

    while ( containerIter.hasNext() )
    {
        ViewContainer* container = containerIter.next();
        createView(session,container,-1);
    }

}

ViewContainer* ViewManager::createContainer(const Profile::Ptr info)
{
    Q_ASSERT( info );

    const int tabPosition = info->property<int>(Profile::TabBarPosition);

    ViewContainer::NavigationPosition position = ( tabPosition == Profile::TabBarTop ) ?
                                                   ViewContainer::NavigationPositionTop :
                                                   ViewContainer::NavigationPositionBottom;

    ViewContainer* container = 0;

    switch ( _navigationMethod )
    {
        case TabbedNavigation:    
            container = new TabbedViewContainer(position,_viewSplitter);
            break;
        case NoNavigation:
        default:
            container = new StackedViewContainer(_viewSplitter);
    }

    // connect signals and slots
    connect( container , SIGNAL(viewAdded(QWidget*,ViewProperties*)) , _containerSignalMapper ,
           SLOT(map()) );
    connect( container , SIGNAL(viewRemoved(QWidget*)) , _containerSignalMapper ,
           SLOT(map()) ); 
    _containerSignalMapper->setMapping(container,container);

    connect( container, SIGNAL(newViewRequest()), this, SIGNAL(newViewRequest()) );
    connect( container, SIGNAL(moveViewRequest(int,int,bool&)), 
    this , SLOT(containerMoveViewRequest(int,int,bool&)) );
    connect( container , SIGNAL(viewRemoved(QWidget*)) , this , SLOT(viewCloseRequest(QWidget*)) );
    connect( container , SIGNAL(closeRequest(QWidget*)) , this , SLOT(viewCloseRequest(QWidget*)) );
    connect( container , SIGNAL(activeViewChanged(QWidget*)) , this , SLOT(viewActivated(QWidget*)));
    
    return container;
}
void ViewManager::containerMoveViewRequest(int index, int id, bool& moved)
{
    ViewContainer* container = qobject_cast<ViewContainer*>(sender());
    SessionController* controller = qobject_cast<SessionController*>(ViewProperties::propertiesById(id));

    if (!controller)
        return;

    createView(controller->session(),container,index);
    moved = true;
}
void ViewManager::setNavigationMethod(NavigationMethod method)
{
    _navigationMethod = method;

    KActionCollection* collection = _actionCollection;

    if ( collection )
    {
        // FIXME: The following disables certain actions for the KPart that it
        // doesn't actually have a use for, to avoid polluting the action/shortcut
        // namespace of an application using the KPart (otherwise, a shortcut may
        // be in use twice, and the user gets to see an "ambiguous shortcut over-
        // load" error dialog). However, this approach sucks - it's the inverse of
        // what it should be. Rather than disabling actions not used by the KPart,
        // a method should be devised to only enable those that are used, perhaps
        // by using a separate action collection.

        QAction* action;

        action = collection->action( "next-view" );
        if ( action ) action->setEnabled( _navigationMethod != NoNavigation );

        action = collection->action( "previous-view" );
        if ( action ) action->setEnabled( _navigationMethod != NoNavigation );

        action = collection->action( "split-view-left-right" );
        if ( action ) action->setEnabled( _navigationMethod != NoNavigation );

        action = collection->action( "split-view-top-bottom" );
        if ( action ) action->setEnabled( _navigationMethod != NoNavigation );

        action = collection->action( "rename-session" );
        if ( action ) action->setEnabled( _navigationMethod != NoNavigation );

        action = collection->action( "move-view-left" );
        if ( action ) action->setEnabled( _navigationMethod != NoNavigation );

        action = collection->action( "move-view-right" );
        if ( action ) action->setEnabled( _navigationMethod != NoNavigation );
    }
}

ViewManager::NavigationMethod ViewManager::navigationMethod() const { return _navigationMethod; }

void ViewManager::containerViewsChanged(QObject* container)
{
    if (_viewSplitter && container == _viewSplitter->activeContainer() )
    {
        emit viewPropertiesChanged( viewProperties() );
    } 
}

void ViewManager::viewCloseRequest(QWidget* view)
{
    //FIXME Check that this cast is actually legal
    TerminalDisplay* display = (TerminalDisplay*)view;
  
    Q_ASSERT(display);

    // 1. detach view from session
    // 2. if the session has no views left, close it
    Session* session = _sessionMap[ display ];
    _sessionMap.remove(display);
    if ( session )
    {
        display->deleteLater();

        if ( session->views().count() == 0 )
            session->close();
    }
    //we only update the focus if the splitter is still alive
    if (_viewSplitter) {
        focusActiveView();
        updateDetachViewState();
    }
}

TerminalDisplay* ViewManager::createTerminalDisplay(Session* session)
{
   TerminalDisplay* display = new TerminalDisplay(0);

   //TODO Temporary settings used here
   display->setBellMode(TerminalDisplay::NotifyBell);
   display->setTerminalSizeHint(true);
   display->setTripleClickMode(TerminalDisplay::SelectWholeLine);
   display->setTerminalSizeStartup(true);
   display->setScrollBarPosition(TerminalDisplay::ScrollBarRight);
   display->setRandomSeed(session->sessionId() * 31);

   return display;
}

const ColorScheme* ViewManager::colorSchemeForProfile(const Profile::Ptr info) const
{
    const ColorScheme* colorScheme = ColorSchemeManager::instance()->
                                            findColorScheme(info->colorScheme());
    if ( !colorScheme )
       colorScheme = ColorSchemeManager::instance()->defaultColorScheme(); 
    Q_ASSERT( colorScheme );

    return colorScheme;
}

void ViewManager::applyProfile(TerminalDisplay* view , const Profile::Ptr info, 
                               bool applyContainerSettings) 
{
    Q_ASSERT( info );
    
    const ColorScheme* colorScheme = colorSchemeForProfile(info);

    // menu bar visibility
    emit setMenuBarVisibleRequest( info->property<bool>(Profile::ShowMenuBar) );

    // tab bar visibility
    if (applyContainerSettings)
    {
        ViewContainer* container = _viewSplitter->activeContainer();
        int tabBarMode = info->property<int>(Profile::TabBarMode);
        int tabBarPosition = info->property<int>(Profile::TabBarPosition);
        bool showNewCloseButtons = info->property<bool>(Profile::ShowNewAndCloseTabButtons);

        if ( tabBarMode == Profile::AlwaysHideTabBar )
            container->setNavigationDisplayMode(ViewContainer::AlwaysHideNavigation);
        else if ( tabBarMode == Profile::AlwaysShowTabBar )
            container->setNavigationDisplayMode(ViewContainer::AlwaysShowNavigation);
        else if ( tabBarMode == Profile::ShowTabBarAsNeeded )
            container->setNavigationDisplayMode(ViewContainer::ShowNavigationAsNeeded);

        ViewContainer::NavigationPosition position = container->navigationPosition();

        if ( tabBarPosition == Profile::TabBarTop )
            position = ViewContainer::NavigationPositionTop;
        else if ( tabBarPosition == Profile::TabBarBottom )
            position = ViewContainer::NavigationPositionBottom; 

        if ( container->supportedNavigationPositions().contains(position) )
            container->setNavigationPosition(position);
       
        if (showNewCloseButtons)
        {
            container->setFeatures(container->features() 
                               | ViewContainer::QuickNewView | ViewContainer::QuickCloseView);
            container->setNewViewMenu(createNewViewMenu());
        }
        else
            container->setFeatures(container->features() 
                            & ~ViewContainer::QuickNewView & ~ViewContainer::QuickCloseView);
    }

    // load colour scheme
    ColorEntry table[TABLE_COLORS];
    
    colorScheme->getColorTable(table , view->randomSeed() );
    view->setColorTable(table);
    view->setOpacity(colorScheme->opacity());
  
    // load font 
    view->setAntialias(info->property<bool>(Profile::AntiAliasFonts));
    view->setVTFont(info->font());

    // set scroll-bar position
    int scrollBarPosition = info->property<int>(Profile::ScrollBarPosition);

    if ( scrollBarPosition == Profile::ScrollBarHidden )
       view->setScrollBarPosition(TerminalDisplay::NoScrollBar);
    else if ( scrollBarPosition == Profile::ScrollBarLeft )
       view->setScrollBarPosition(TerminalDisplay::ScrollBarLeft);
    else if ( scrollBarPosition == Profile::ScrollBarRight )
       view->setScrollBarPosition(TerminalDisplay::ScrollBarRight);

    // terminal features
    bool blinkingCursor = info->property<bool>(Profile::BlinkingCursorEnabled);
    view->setBlinkingCursor(blinkingCursor);  

    bool blinkingText = info->property<bool>(Profile::BlinkingTextEnabled);
    view->setBlinkingTextEnabled(blinkingText);

    bool bidiEnabled = info->property<bool>(Profile::BidiRenderingEnabled);
    view->setBidiEnabled(bidiEnabled);

    // cursor shape
    int cursorShape = info->property<int>(Profile::CursorShape);

    if ( cursorShape == Profile::BlockCursor )
        view->setKeyboardCursorShape(TerminalDisplay::BlockCursor);  
    else if ( cursorShape == Profile::IBeamCursor )
        view->setKeyboardCursorShape(TerminalDisplay::IBeamCursor);
    else if ( cursorShape == Profile::UnderlineCursor )
        view->setKeyboardCursorShape(TerminalDisplay::UnderlineCursor);

    // cursor color
    bool useCustomColor = info->property<bool>(Profile::UseCustomCursorColor);
    const QColor& cursorColor = info->property<QColor>(Profile::CustomCursorColor);
        
    view->setKeyboardCursorColor(!useCustomColor,cursorColor);

    // word characters
    view->setWordCharacters( info->property<QString>(Profile::WordCharacters) );
}

void ViewManager::updateViewsForSession(Session* session)
{
    QListIterator<TerminalDisplay*> iter(_sessionMap.keys(session));
    while ( iter.hasNext() )
    {
        applyProfile(iter.next(),SessionManager::instance()->sessionProfile(session),false);
    }
}

void ViewManager::profileChanged(Profile::Ptr profile)
{
    QHashIterator<TerminalDisplay*,Session*> iter(_sessionMap);

    while ( iter.hasNext() )
    {
        iter.next();

        // if session uses this profile, update the display
        if ( iter.key() != 0 && 
             iter.value() != 0 && 
             SessionManager::instance()->sessionProfile(iter.value()) == profile ) 
        {
            applyProfile(iter.key(),profile,true);
        }
    }
}

QList<ViewProperties*> ViewManager::viewProperties() const
{
    QList<ViewProperties*> list;

    ViewContainer* container = _viewSplitter->activeContainer();

    Q_ASSERT( container );

    QListIterator<QWidget*> viewIter(container->views());
    while ( viewIter.hasNext() )
    {
        ViewProperties* properties = container->viewProperties(viewIter.next()); 
        Q_ASSERT( properties );
        list << properties; 
    } 

    return list;
}

void ViewManager::saveSessions(KConfigGroup& group)
{
    // find all unique session restore IDs
    QList<int> ids;
    QHash<Session*,int> unique;

    // first: sessions in the active container, preserving the order
    ViewContainer* container = _viewSplitter->activeContainer();
    Q_ASSERT(container);
    TerminalDisplay* activeview = dynamic_cast<TerminalDisplay*>(container->activeView());

    QListIterator<QWidget*> viewIter(container->views());
    int tab = 1;
    while (viewIter.hasNext())
    {
        TerminalDisplay *view = dynamic_cast<TerminalDisplay*>(viewIter.next());
        Q_ASSERT(view);
        Session *session = _sessionMap[view];
        ids << SessionManager::instance()->getRestoreId(session);
        if (view == activeview) group.writeEntry("Active", tab);
        unique.insert(session, 1);
        tab++;
    }

    // second: all other sessions, in random order
    // we don't want to have sessions restored that are not connected
    foreach(Session* session, _sessionMap)
        if (!unique.contains(session))
        {
            ids << SessionManager::instance()->getRestoreId(session);
            unique.insert(session, 1);
        }

    group.writeEntry("Sessions", ids);
}

void ViewManager::restoreSessions(const KConfigGroup& group)
{
    QList<int> ids = group.readEntry("Sessions", QList<int>());
    int activeTab  = group.readEntry("Active", 0);
    TerminalDisplay *display = 0;

    int tab = 1;
    foreach(int id, ids)
    {
        Session *session = SessionManager::instance()->idToSession(id);
        createView(session);
        if (!session->isRunning())
            session->run();
        if (tab++ == activeTab)
            display = dynamic_cast<TerminalDisplay*>(activeView());
    }

    if (display)
    {
        _viewSplitter->activeContainer()->setActiveView(display);
        display->setFocus(Qt::OtherFocusReason);
    }
}

uint qHash(QPointer<TerminalDisplay> display)
{
    return qHash((TerminalDisplay*)display);
}

int ViewManager::sessionCount()
{
    return this->_sessionMap.size();
}

int ViewManager::currentSession()
{
    QHash<TerminalDisplay*,Session*>::iterator i;
    for (i = this->_sessionMap.begin(); i != this->_sessionMap.end(); ++i)
        if (i.key()->isVisible())
            return i.value()->sessionId();
    return -1;
}

int ViewManager::newSession()
{
    Profile::Ptr profile = profile = SessionManager::instance()->defaultProfile();
    Session* session = SessionManager::instance()->createSession(profile);

    this->createView(session);
    session->run();

    return session->sessionId();
}

int ViewManager::newSession(QString profile, QString directory)
{
    QList<Profile::Ptr> profilelist = SessionManager::instance()->loadedProfiles();
    QList<Profile::Ptr>::iterator i = profilelist.begin();

    Profile::Ptr profileptr = SessionManager::instance()->defaultProfile();

    while (i != profilelist.end() )
    {
        Profile::Ptr ptr = *i;
        if ( ptr->name().compare(profile) == 0)
            profileptr = ptr;
        i++;
    }

    Session* session = SessionManager::instance()->createSession(profileptr);
    session->setInitialWorkingDirectory(directory);

    this->createView(session);
    session->run();

    return session->sessionId();
}

QStringList ViewManager::profileList()
{
    QList<Profile::Ptr> profilelist = SessionManager::instance()->loadedProfiles();
    QList<Profile::Ptr>::iterator i = profilelist.begin();
    QStringList list;
    while (i != profilelist.end() )
    {
        Profile::Ptr ptr = *i;
        list.push_back(ptr->name());
        i++;
    }

    return list;
}

void ViewManager::nextSession()
{
    this->nextView();
}

void ViewManager::prevSession()
{
    this->previousView();
}

void ViewManager::moveSessionLeft()
{
    this->moveActiveViewLeft();
}

void ViewManager::moveSessionRight()
{
    this->moveActiveViewRight();
}

#include "ViewManager.moc"


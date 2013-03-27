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

// Qt
#include <QtCore/QSignalMapper>
#include <QtCore/QStringList>
#include <QMenu>
#include <QtDBus/QtDBus>

// KDE
#include <KAcceleratorManager>
#include <KLocalizedString>
#include <KAction>
#include <KActionCollection>
#include <KConfigGroup>

// Konsole
#include <windowadaptor.h>

#include "ColorScheme.h"
#include "ColorSchemeManager.h"
#include "Session.h"
#include "TerminalDisplay.h"
#include "SessionController.h"
#include "SessionManager.h"
#include "ProfileManager.h"
#include "ViewContainer.h"
#include "ViewSplitter.h"
#include "Profile.h"
#include "Enumeration.h"

using namespace Konsole;

int ViewManager::lastManagerId = 0;

ViewManager::ViewManager(QObject* parent , KActionCollection* collection)
    : QObject(parent)
    , _viewSplitter(0)
    , _actionCollection(collection)
    , _containerSignalMapper(new QSignalMapper(this))
    , _navigationMethod(TabbedNavigation)
    , _navigationVisibility(ViewContainer::AlwaysShowNavigation)
    , _navigationPosition(ViewContainer::NavigationPositionTop)
    , _showQuickButtons(false)
    , _newTabBehavior(PutNewTabAtTheEnd)
    , _navigationStyleSheet(QString())
    , _managerId(0)
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

    // setup actions which are related to the views
    setupActions();

    // emit a signal when all of the views held by this view manager are destroyed
    connect(_viewSplitter , SIGNAL(allContainersEmpty()) , this , SIGNAL(empty()));
    connect(_viewSplitter , SIGNAL(empty(ViewSplitter*)) , this , SIGNAL(empty()));

    // listen for addition or removal of views from associated containers
    connect(_containerSignalMapper , SIGNAL(mapped(QObject*)) , this ,
            SLOT(containerViewsChanged(QObject*)));

    // listen for profile changes
    connect(ProfileManager::instance() , SIGNAL(profileChanged(Profile::Ptr)) , this,
            SLOT(profileChanged(Profile::Ptr)));
    connect(SessionManager::instance() , SIGNAL(sessionUpdated(Session*)) , this,
            SLOT(updateViewsForSession(Session*)));

    //prepare DBus communication
    new WindowAdaptor(this);
    // TODO: remove this obsolete and bad name
    QDBusConnection::sessionBus().registerObject(QLatin1String("/Konsole"), this);

    _managerId = ++lastManagerId;
    QDBusConnection::sessionBus().registerObject(QLatin1String("/Windows/") + QString::number(_managerId), this);
}

ViewManager::~ViewManager()
{
}

int ViewManager::managerId() const
{
    return _managerId;
}

QWidget* ViewManager::activeView() const
{
    ViewContainer* container = _viewSplitter->activeContainer();
    if (container) {
        return container->activeView();
    } else {
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

    KAction* nextViewAction = new KAction(i18nc("@action Shortcut entry", "Next Tab") , this);
    KAction* previousViewAction = new KAction(i18nc("@action Shortcut entry", "Previous Tab") , this);
    KAction* lastViewAction = new KAction(i18nc("@action Shortcut entry", "Switch to Last Tab") , this);
    KAction* nextContainerAction = new KAction(i18nc("@action Shortcut entry", "Next View Container") , this);

    KAction* moveViewLeftAction = new KAction(i18nc("@action Shortcut entry", "Move Tab Left") , this);
    KAction* moveViewRightAction = new KAction(i18nc("@action Shortcut entry", "Move Tab Right") , this);

    // list of actions that should only be enabled when there are multiple view
    // containers open
    QList<QAction*> multiViewOnlyActions;
    multiViewOnlyActions << nextContainerAction;

    if (collection) {
        KAction* splitLeftRightAction = new KAction(KIcon("view-split-left-right"),
                i18nc("@action:inmenu", "Split View Left/Right"),
                this);
        splitLeftRightAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_ParenLeft));
        collection->addAction("split-view-left-right", splitLeftRightAction);
        connect(splitLeftRightAction , SIGNAL(triggered()) , this , SLOT(splitLeftRight()));

        KAction* splitTopBottomAction = new KAction(KIcon("view-split-top-bottom") ,
                i18nc("@action:inmenu", "Split View Top/Bottom"), this);
        splitTopBottomAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_ParenRight));
        collection->addAction("split-view-top-bottom", splitTopBottomAction);
        connect(splitTopBottomAction , SIGNAL(triggered()) , this , SLOT(splitTopBottom()));

        KAction* closeActiveAction = new KAction(i18nc("@action:inmenu Close Active View", "Close Active") , this);
        closeActiveAction->setIcon(KIcon("view-close"));
        closeActiveAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_S));
        closeActiveAction->setEnabled(false);
        collection->addAction("close-active-view", closeActiveAction);
        connect(closeActiveAction , SIGNAL(triggered()) , this , SLOT(closeActiveContainer()));

        multiViewOnlyActions << closeActiveAction;

        KAction* closeOtherAction = new KAction(i18nc("@action:inmenu Close Other Views", "Close Others") , this);
        closeOtherAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_O));
        closeOtherAction->setEnabled(false);
        collection->addAction("close-other-views", closeOtherAction);
        connect(closeOtherAction , SIGNAL(triggered()) , this , SLOT(closeOtherContainers()));

        multiViewOnlyActions << closeOtherAction;

        // Expand & Shrink Active View
        KAction* expandActiveAction = new KAction(i18nc("@action:inmenu", "Expand View") , this);
        expandActiveAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_BracketRight));
        expandActiveAction->setEnabled(false);
        collection->addAction("expand-active-view", expandActiveAction);
        connect(expandActiveAction , SIGNAL(triggered()) , this , SLOT(expandActiveContainer()));

        multiViewOnlyActions << expandActiveAction;

        KAction* shrinkActiveAction = new KAction(i18nc("@action:inmenu", "Shrink View") , this);
        shrinkActiveAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_BracketLeft));
        shrinkActiveAction->setEnabled(false);
        collection->addAction("shrink-active-view", shrinkActiveAction);
        connect(shrinkActiveAction , SIGNAL(triggered()) , this , SLOT(shrinkActiveContainer()));

        multiViewOnlyActions << shrinkActiveAction;

        KAction* detachViewAction = collection->addAction("detach-view");
        detachViewAction->setIcon(KIcon("tab-detach"));
        detachViewAction->setText(i18nc("@action:inmenu", "D&etach Current Tab"));
        // Ctrl+Shift+D is not used as a shortcut by default because it is too close
        // to Ctrl+D - which will terminate the session in many cases
        detachViewAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_H));

        connect(this , SIGNAL(splitViewToggle(bool)) , this , SLOT(updateDetachViewState()));
        connect(detachViewAction , SIGNAL(triggered()) , this , SLOT(detachActiveView()));

        // Next / Previous View , Next Container
        collection->addAction("next-view", nextViewAction);
        collection->addAction("previous-view", previousViewAction);
        collection->addAction("last-tab", lastViewAction);
        collection->addAction("next-container", nextContainerAction);
        collection->addAction("move-view-left", moveViewLeftAction);
        collection->addAction("move-view-right", moveViewRightAction);

        // Switch to tab N shortcuts
        const int SWITCH_TO_TAB_COUNT = 19;
        QSignalMapper* switchToTabMapper = new QSignalMapper(this);
        connect(switchToTabMapper, SIGNAL(mapped(int)), this, SLOT(switchToView(int)));
        for (int i = 0; i < SWITCH_TO_TAB_COUNT; i++) {
            KAction* switchToTabAction = new KAction(i18nc("@action Shortcut entry", "Switch to Tab %1", i + 1), this);
            switchToTabMapper->setMapping(switchToTabAction, i);
            connect(switchToTabAction, SIGNAL(triggered()), switchToTabMapper,
                    SLOT(map()));
            collection->addAction(QString("switch-to-tab-%1").arg(i), switchToTabAction);
        }
    }

    foreach(QAction* action, multiViewOnlyActions) {
        connect(this , SIGNAL(splitViewToggle(bool)) , action , SLOT(setEnabled(bool)));
    }

    // keyboard shortcut only actions
    nextViewAction->setShortcut(QKeySequence(Qt::SHIFT + Qt::Key_Right));
    connect(nextViewAction, SIGNAL(triggered()) , this , SLOT(nextView()));
    _viewSplitter->addAction(nextViewAction);

    previousViewAction->setShortcut(QKeySequence(Qt::SHIFT + Qt::Key_Left));
    connect(previousViewAction, SIGNAL(triggered()) , this , SLOT(previousView()));
    _viewSplitter->addAction(previousViewAction);

    nextContainerAction->setShortcut(QKeySequence(Qt::SHIFT + Qt::Key_Tab));
    connect(nextContainerAction , SIGNAL(triggered()) , this , SLOT(nextContainer()));
    _viewSplitter->addAction(nextContainerAction);

    moveViewLeftAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_Left));
    connect(moveViewLeftAction , SIGNAL(triggered()) , this , SLOT(moveActiveViewLeft()));
    _viewSplitter->addAction(moveViewLeftAction);

    moveViewRightAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_Right));
    connect(moveViewRightAction , SIGNAL(triggered()) , this , SLOT(moveActiveViewRight()));
    _viewSplitter->addAction(moveViewRightAction);

    connect(lastViewAction, SIGNAL(triggered()) , this , SLOT(lastView()));
    _viewSplitter->addAction(lastViewAction);
}
void ViewManager::switchToView(int index)
{
    Q_ASSERT(index >= 0);
    ViewContainer* container = _viewSplitter->activeContainer();
    Q_ASSERT(container);
    QList<QWidget*> containerViews = container->views();
    if (index >= containerViews.count())
        return;
    container->setActiveView(containerViews.at(index));
}
void ViewManager::updateDetachViewState()
{
    if (!_actionCollection)
        return;

    const bool splitView = _viewSplitter->containers().count() >= 2;
    const bool shouldEnable = splitView ||
                              _viewSplitter->activeContainer()->views().count() >= 2;

    QAction* detachAction = _actionCollection->action("detach-view");

    if (detachAction && shouldEnable != detachAction->isEnabled())
        detachAction->setEnabled(shouldEnable);
}
void ViewManager::moveActiveViewLeft()
{
    ViewContainer* container = _viewSplitter->activeContainer();
    Q_ASSERT(container);
    container->moveActiveView(ViewContainer::MoveViewLeft);
}
void ViewManager::moveActiveViewRight()
{
    ViewContainer* container = _viewSplitter->activeContainer();
    Q_ASSERT(container);
    container->moveActiveView(ViewContainer::MoveViewRight);
}
void ViewManager::nextContainer()
{
    _viewSplitter->activateNextContainer();
}

void ViewManager::nextView()
{
    ViewContainer* container = _viewSplitter->activeContainer();

    Q_ASSERT(container);

    container->activateNextView();
}

void ViewManager::previousView()
{
    ViewContainer* container = _viewSplitter->activeContainer();

    Q_ASSERT(container);

    container->activatePreviousView();
}

void ViewManager::lastView()
{
    ViewContainer* container = _viewSplitter->activeContainer();

    Q_ASSERT(container);

    container->activateLastView();
}

void ViewManager::detachActiveView()
{
    // find the currently active view and remove it from its container
    ViewContainer* container = _viewSplitter->activeContainer();

    detachView(container, container->activeView());
}

void ViewManager::detachView(ViewContainer* container, QWidget* widgetView)
{
    TerminalDisplay * viewToDetach = qobject_cast<TerminalDisplay*>(widgetView);

    if (!viewToDetach)
        return;

    emit viewDetached(_sessionMap[viewToDetach]);

    _sessionMap.remove(viewToDetach);

    // remove the view from this window
    container->removeView(viewToDetach);
    viewToDetach->deleteLater();

    // if the container from which the view was removed is now empty then it can be deleted,
    // unless it is the only container in the window, in which case it is left empty
    // so that there is always an active container
    if (_viewSplitter->containers().count() > 1 &&
            container->views().count() == 0) {
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
    Q_ASSERT(session);

    // close attached views
    QList<TerminalDisplay*> children = _viewSplitter->findChildren<TerminalDisplay*>();

    foreach(TerminalDisplay* view , children) {
        if (_sessionMap[view] == session) {
            _sessionMap.remove(view);
            view->deleteLater();
        }
    }

    // This is needed to remove this controller from factory() in
    // order to prevent BUG: 185466 - disappearing menu popup
    if (_pluggedController)
        emit unplugController(_pluggedController);
}

void ViewManager::focusActiveView()
{
    // give the active view in a container the focus.  this ensures
    // that controller associated with that view is activated and the session-specific
    // menu items are replaced with the ones for the newly focused view

    // see the viewFocused() method

    ViewContainer* container = _viewSplitter->activeContainer();
    if (container) {
        QWidget* activeView = container->activeView();
        if (activeView) {
            activeView->setFocus(Qt::MouseFocusReason);
        }
    }
}

void ViewManager::viewActivated(QWidget* view)
{
    Q_ASSERT(view != 0);

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
    ViewContainer* container = createContainer();

    // iterate over each session which has a view in the current active
    // container and create a new view for that session in a new container
    foreach(QWidget* view,  _viewSplitter->activeContainer()->views()) {
        Session* session = _sessionMap[qobject_cast<TerminalDisplay*>(view)];
        TerminalDisplay* display = createTerminalDisplay(session);
        const Profile::Ptr profile = SessionManager::instance()->sessionProfile(session);
        applyProfileToView(display, profile);
        ViewProperties* properties = createController(session, display);

        _sessionMap[display] = session;

        container->addView(display, properties);
        session->addView(display);
    }

    _viewSplitter->addContainer(container, orientation);
    emit splitViewToggle(_viewSplitter->containers().count() > 0);

    // focus the new container
    container->containerWidget()->setFocus();

    // ensure that the active view is focused after the split / unsplit
    ViewContainer* activeContainer = _viewSplitter->activeContainer();
    QWidget* activeView = activeContainer ? activeContainer->activeView() : 0;

    if (activeView)
        activeView->setFocus(Qt::OtherFocusReason);
}
void ViewManager::removeContainer(ViewContainer* container)
{
    // remove session map entries for views in this container
    foreach(QWidget* view , container->views()) {
        TerminalDisplay* display = qobject_cast<TerminalDisplay*>(view);
        Q_ASSERT(display);
        _sessionMap.remove(display);
    }

    _viewSplitter->removeContainer(container);
    container->deleteLater();

    emit splitViewToggle(_viewSplitter->containers().count() > 1);
}
void ViewManager::expandActiveContainer()
{
    _viewSplitter->adjustContainerSize(_viewSplitter->activeContainer(), 10);
}
void ViewManager::shrinkActiveContainer()
{
    _viewSplitter->adjustContainerSize(_viewSplitter->activeContainer(), -10);
}
void ViewManager::closeActiveContainer()
{
    // only do something if there is more than one container active
    if (_viewSplitter->containers().count() > 1) {
        ViewContainer* container = _viewSplitter->activeContainer();

        removeContainer(container);

        // focus next container so that user can continue typing
        // without having to manually focus it themselves
        nextContainer();
    }
}
void ViewManager::closeOtherContainers()
{
    ViewContainer* active = _viewSplitter->activeContainer();

    foreach(ViewContainer* container, _viewSplitter->containers()) {
        if (container != active)
            removeContainer(container);
    }
}

SessionController* ViewManager::createController(Session* session , TerminalDisplay* view)
{
    // create a new controller for the session, and ensure that this view manager
    // is notified when the view gains the focus
    SessionController* controller = new SessionController(session, view, this);
    connect(controller , SIGNAL(focused(SessionController*)) , this , SLOT(controllerChanged(SessionController*)));
    connect(session , SIGNAL(destroyed()) , controller , SLOT(deleteLater()));
    connect(session , SIGNAL(primaryScreenInUse(bool)) ,
            controller , SLOT(setupPrimaryScreenSpecificActions(bool)));
    connect(session , SIGNAL(selectionChanged(QString)) ,
            controller , SLOT(selectionChanged(QString)));
    connect(view , SIGNAL(destroyed()) , controller , SLOT(deleteLater()));

    // if this is the first controller created then set it as the active controller
    if (!_pluggedController)
        controllerChanged(controller);

    return controller;
}

void ViewManager::controllerChanged(SessionController* controller)
{
    if (controller == _pluggedController)
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
    // Use Qt::UniqueConnection to avoid duplicate connection
    connect(session, SIGNAL(finished()), this, SLOT(sessionFinished()), Qt::UniqueConnection);

    TerminalDisplay* display = createTerminalDisplay(session);
    const Profile::Ptr profile = SessionManager::instance()->sessionProfile(session);
    applyProfileToView(display, profile);

    // set initial size
    const QSize& preferredSize = session->preferredSize();
    // FIXME: +1 is needed here for getting the expected rows
    display->setSize(preferredSize.width(), preferredSize.height() + 1);

    ViewProperties* properties = createController(session, display);

    _sessionMap[display] = session;
    container->addView(display, properties, index);
    session->addView(display);

    // tell the session whether it has a light or dark background
    session->setDarkBackground(colorSchemeForProfile(profile)->hasDarkBackground());

    if (container == _viewSplitter->activeContainer()) {
        container->setActiveView(display);
        display->setFocus(Qt::OtherFocusReason);
    }

    updateDetachViewState();
}

void ViewManager::createView(Session* session)
{
    // create the default container
    if (_viewSplitter->containers().count() == 0) {
        ViewContainer* container = createContainer();
        _viewSplitter->addContainer(container, Qt::Vertical);
        emit splitViewToggle(false);
    }

    // new tab will be put at the end by default.
    int index = -1;

    if (_newTabBehavior == PutNewTabAfterCurrentTab) {
        QWidget* view = activeView();
        if (view) {
            QList<QWidget*> views =  _viewSplitter->activeContainer()->views();
            index = views.indexOf(view) + 1;
        }
    }

    // iterate over the view containers owned by this view manager
    // and create a new terminal display for the session in each of them, along with
    // a controller for the session/display pair
    foreach(ViewContainer* container,  _viewSplitter->containers()) {
        createView(session, container, index);
    }
}

ViewContainer* ViewManager::createContainer()
{
    ViewContainer* container = 0;

    switch (_navigationMethod) {
    case TabbedNavigation: {
        container = new TabbedViewContainer(_navigationPosition, _viewSplitter);

        connect(container, SIGNAL(detachTab(ViewContainer*,QWidget*)),
                this, SLOT(detachView(ViewContainer*,QWidget*))
               );
        connect(container, SIGNAL(closeTab(ViewContainer*,QWidget*)),
                this, SLOT(closeTabFromContainer(ViewContainer*,QWidget*)));
    }
    break;
    case NoNavigation:
    default:
        container = new StackedViewContainer(_viewSplitter);
    }

    // FIXME: these code feels duplicated
    container->setNavigationVisibility(_navigationVisibility);
    container->setNavigationPosition(_navigationPosition);
    container->setStyleSheet(_navigationStyleSheet);
    if (_showQuickButtons) {
        container->setFeatures(container->features()
                               | ViewContainer::QuickNewView
                               | ViewContainer::QuickCloseView);
    } else {
        container->setFeatures(container->features()
                               & ~ViewContainer::QuickNewView
                               & ~ViewContainer::QuickCloseView);
    }

    // connect signals and slots
    connect(container , SIGNAL(viewAdded(QWidget*,ViewProperties*)) , _containerSignalMapper ,
            SLOT(map()));
    connect(container , SIGNAL(viewRemoved(QWidget*)) , _containerSignalMapper ,
            SLOT(map()));
    _containerSignalMapper->setMapping(container, container);

    connect(container, SIGNAL(newViewRequest()), this, SIGNAL(newViewRequest()));
    connect(container, SIGNAL(newViewRequest(Profile::Ptr)), this, SIGNAL(newViewRequest(Profile::Ptr)));
    connect(container, SIGNAL(moveViewRequest(int,int,bool&)),
            this , SLOT(containerMoveViewRequest(int,int,bool&)));
    connect(container , SIGNAL(viewRemoved(QWidget*)) , this , SLOT(viewDestroyed(QWidget*)));
    connect(container , SIGNAL(activeViewChanged(QWidget*)) , this , SLOT(viewActivated(QWidget*)));

    return container;
}
void ViewManager::containerMoveViewRequest(int index, int id, bool& moved)
{
    ViewContainer* container = qobject_cast<ViewContainer*>(sender());
    SessionController* controller = qobject_cast<SessionController*>(ViewProperties::propertiesById(id));

    if (!controller)
        return;

    createView(controller->session(), container, index);
    controller->session()->refresh();
    moved = true;
}
void ViewManager::setNavigationMethod(NavigationMethod method)
{
    _navigationMethod = method;

    KActionCollection* collection = _actionCollection;

    if (collection) {
        // FIXME: The following disables certain actions for the KPart that it
        // doesn't actually have a use for, to avoid polluting the action/shortcut
        // namespace of an application using the KPart (otherwise, a shortcut may
        // be in use twice, and the user gets to see an "ambiguous shortcut over-
        // load" error dialog). However, this approach sucks - it's the inverse of
        // what it should be. Rather than disabling actions not used by the KPart,
        // a method should be devised to only enable those that are used, perhaps
        // by using a separate action collection.

        const bool enable = (_navigationMethod != NoNavigation);
        QAction* action;

        action = collection->action("next-view");
        if (action) action->setEnabled(enable);

        action = collection->action("previous-view");
        if (action) action->setEnabled(enable);

        action = collection->action("last-tab");
        if (action) action->setEnabled(enable);

        action = collection->action("split-view-left-right");
        if (action) action->setEnabled(enable);

        action = collection->action("split-view-top-bottom");
        if (action) action->setEnabled(enable);

        action = collection->action("rename-session");
        if (action) action->setEnabled(enable);

        action = collection->action("move-view-left");
        if (action) action->setEnabled(enable);

        action = collection->action("move-view-right");
        if (action) action->setEnabled(enable);
    }
}

ViewManager::NavigationMethod ViewManager::navigationMethod() const
{
    return _navigationMethod;
}

void ViewManager::containerViewsChanged(QObject* container)
{
    if (_viewSplitter && container == _viewSplitter->activeContainer()) {
        emit viewPropertiesChanged(viewProperties());
    }
}

void ViewManager::viewDestroyed(QWidget* view)
{
    // Note: the received QWidget has already been destroyed, so
    // using dynamic_cast<> or qobject_cast<> does not work here
    TerminalDisplay* display = static_cast<TerminalDisplay*>(view);
    Q_ASSERT(display);

    // 1. detach view from session
    // 2. if the session has no views left, close it
    Session* session = _sessionMap[ display ];
    _sessionMap.remove(display);
    if (session) {
        display->deleteLater();

        if (session->views().count() == 0)
            session->close();
    }
    //we only update the focus if the splitter is still alive
    if (_viewSplitter) {
        focusActiveView();
        updateDetachViewState();
    }
    // The below causes the menus  to be messed up
    // Only happenss when using the tab bar close button
//    if (_pluggedController)
//        emit unplugController(_pluggedController);
}

TerminalDisplay* ViewManager::createTerminalDisplay(Session* session)
{
    TerminalDisplay* display = new TerminalDisplay(0);
    display->setRandomSeed(session->sessionId() * 31);

    return display;
}

const ColorScheme* ViewManager::colorSchemeForProfile(const Profile::Ptr profile)
{
    const ColorScheme* colorScheme = ColorSchemeManager::instance()->
                                     findColorScheme(profile->colorScheme());
    if (!colorScheme)
        colorScheme = ColorSchemeManager::instance()->defaultColorScheme();
    Q_ASSERT(colorScheme);

    return colorScheme;
}

void ViewManager::applyProfileToView(TerminalDisplay* view , const Profile::Ptr profile)
{
    Q_ASSERT(profile);

    emit setSaveGeometryOnExitRequest(profile->saveGeometryOnExit());

    emit updateWindowIcon();

    // load color scheme
    ColorEntry table[TABLE_COLORS];
    const ColorScheme* colorScheme = colorSchemeForProfile(profile);
    colorScheme->getColorTable(table , view->randomSeed());
    view->setColorTable(table);
    view->setOpacity(colorScheme->opacity());
    view->setWallpaper(colorScheme->wallpaper());

    // load font
    view->setAntialias(profile->antiAliasFonts());
    view->setBoldIntense(profile->boldIntense());
    view->setVTFont(profile->font());

    // set scroll-bar position
    int scrollBarPosition = profile->property<int>(Profile::ScrollBarPosition);

    if (scrollBarPosition == Enum::ScrollBarLeft)
        view->setScrollBarPosition(Enum::ScrollBarLeft);
    else if (scrollBarPosition == Enum::ScrollBarRight)
        view->setScrollBarPosition(Enum::ScrollBarRight);
    else if (scrollBarPosition == Enum::ScrollBarHidden)
        view->setScrollBarPosition(Enum::ScrollBarHidden);

    // show hint about termianl size after resizing
    view->setShowTerminalSizeHint(profile->showTerminalSizeHint());

    // terminal features
    view->setBlinkingCursorEnabled(profile->blinkingCursorEnabled());
    view->setBlinkingTextEnabled(profile->blinkingTextEnabled());

    int tripleClickMode = profile->property<int>(Profile::TripleClickMode);
    view->setTripleClickMode(Enum::TripleClickModeEnum(tripleClickMode));

    view->setAutoCopySelectedText(profile->autoCopySelectedText());
    view->setUnderlineLinks(profile->underlineLinksEnabled());
    view->setControlDrag(profile->property<bool>(Profile::CtrlRequiredForDrag));
    view->setBidiEnabled(profile->bidiRenderingEnabled());
    view->setLineSpacing(profile->lineSpacing());
    view->setTrimTrailingSpaces(profile->property<bool>(Profile::TrimTrailingSpacesInSelectedText));

    view->setOpenLinksByDirectClick(profile->property<bool>(Profile::OpenLinksByDirectClickEnabled));

    int middleClickPasteMode = profile->property<int>(Profile::MiddleClickPasteMode);
    if (middleClickPasteMode == Enum::PasteFromX11Selection)
        view->setMiddleClickPasteMode(Enum::PasteFromX11Selection);
    else if (middleClickPasteMode == Enum::PasteFromClipboard)
        view->setMiddleClickPasteMode(Enum::PasteFromClipboard);

    // cursor shape
    int cursorShape = profile->property<int>(Profile::CursorShape);

    if (cursorShape == Enum::BlockCursor)
        view->setKeyboardCursorShape(Enum::BlockCursor);
    else if (cursorShape == Enum::IBeamCursor)
        view->setKeyboardCursorShape(Enum::IBeamCursor);
    else if (cursorShape == Enum::UnderlineCursor)
        view->setKeyboardCursorShape(Enum::UnderlineCursor);

    // cursor color
    if (profile->useCustomCursorColor()) {
        const QColor& cursorColor = profile->customCursorColor();
        view->setKeyboardCursorColor(cursorColor);
    } else {
        // an invalid QColor is used to inform the view widget to
        // draw the cursor using the default color( matching the text)
        view->setKeyboardCursorColor(QColor());
    }

    // word characters
    view->setWordCharacters(profile->wordCharacters());

    // bell mode
    view->setBellMode(profile->property<int>(Profile::BellMode));
}

void ViewManager::updateViewsForSession(Session* session)
{
    const Profile::Ptr profile = SessionManager::instance()->sessionProfile(session);

    foreach(TerminalDisplay* view, _sessionMap.keys(session)) {
        applyProfileToView(view, profile);
    }
}

void ViewManager::profileChanged(Profile::Ptr profile)
{
    // update all views associated with this profile
    QHashIterator<TerminalDisplay*, Session*> iter(_sessionMap);
    while (iter.hasNext()) {
        iter.next();

        // if session uses this profile, update the display
        if (iter.key() != 0 &&
                iter.value() != 0 &&
                SessionManager::instance()->sessionProfile(iter.value()) == profile) {
            applyProfileToView(iter.key(), profile);
        }
    }
}

QList<ViewProperties*> ViewManager::viewProperties() const
{
    QList<ViewProperties*> list;

    ViewContainer* container = _viewSplitter->activeContainer();

    Q_ASSERT(container);

    foreach(QWidget* view, container->views()) {
        ViewProperties* properties = container->viewProperties(view);
        Q_ASSERT(properties);
        list << properties;
    }

    return list;
}

void ViewManager::saveSessions(KConfigGroup& group)
{
    // find all unique session restore IDs
    QList<int> ids;
    QHash<Session*, int> unique;

    // first: sessions in the active container, preserving the order
    ViewContainer* container = _viewSplitter->activeContainer();
    Q_ASSERT(container);
    TerminalDisplay* activeview = qobject_cast<TerminalDisplay*>(container->activeView());

    QListIterator<QWidget*> viewIter(container->views());
    int tab = 1;
    while (viewIter.hasNext()) {
        TerminalDisplay* view = qobject_cast<TerminalDisplay*>(viewIter.next());
        Q_ASSERT(view);
        Session* session = _sessionMap[view];
        ids << SessionManager::instance()->getRestoreId(session);
        if (view == activeview) group.writeEntry("Active", tab);
        unique.insert(session, 1);
        tab++;
    }

    // second: all other sessions, in random order
    // we don't want to have sessions restored that are not connected
    foreach(Session * session, _sessionMap) {
        if (!unique.contains(session)) {
            ids << SessionManager::instance()->getRestoreId(session);
            unique.insert(session, 1);
        }
    }

    group.writeEntry("Sessions", ids);
}

void ViewManager::restoreSessions(const KConfigGroup& group)
{
    QList<int> ids = group.readEntry("Sessions", QList<int>());
    int activeTab  = group.readEntry("Active", 0);
    TerminalDisplay* display = 0;

    int tab = 1;
    foreach(int id, ids) {
        Session* session = SessionManager::instance()->idToSession(id);
        createView(session);
        if (!session->isRunning())
            session->run();
        if (tab++ == activeTab)
            display = qobject_cast<TerminalDisplay*>(activeView());
    }

    if (display) {
        _viewSplitter->activeContainer()->setActiveView(display);
        display->setFocus(Qt::OtherFocusReason);
    }

    if (ids.isEmpty()) { // Session file is unusable, start default Profile
        Profile::Ptr profile = ProfileManager::instance()->defaultProfile();
        Session* session = SessionManager::instance()->createSession(profile);
        createView(session);
        if (!session->isRunning())
            session->run();
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
    QHash<TerminalDisplay*, Session*>::iterator i;
    for (i = this->_sessionMap.begin(); i != this->_sessionMap.end(); ++i)
        if (i.key()->isVisible())
            return i.value()->sessionId();
    return -1;
}

int ViewManager::newSession()
{
    Profile::Ptr profile = ProfileManager::instance()->defaultProfile();
    Session* session = SessionManager::instance()->createSession(profile);

    this->createView(session);
    session->run();

    return session->sessionId();
}

int ViewManager::newSession(QString profile, QString directory)
{
    const QList<Profile::Ptr> profilelist = ProfileManager::instance()->allProfiles();
    Profile::Ptr profileptr = ProfileManager::instance()->defaultProfile();

    for (int i = 0; i < profilelist.size(); ++i) {
        if (profilelist.at(i)->name() == profile) {
            profileptr = profilelist.at(i);
            break;
        }
    }

    Session* session = SessionManager::instance()->createSession(profileptr);
    session->setInitialWorkingDirectory(directory);

    this->createView(session);
    session->run();

    return session->sessionId();
}

QString ViewManager::defaultProfile()
{
    return ProfileManager::instance()->defaultProfile()->name();
}

QStringList ViewManager::profileList()
{
    return ProfileManager::instance()->availableProfileNames();
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

void ViewManager::setTabWidthToText(bool useTextWidth)
{
    ViewContainer* container = _viewSplitter->activeContainer();
    Q_ASSERT(container);
    container->setNavigationTextMode(useTextWidth);
}

void ViewManager::closeTabFromContainer(ViewContainer* container, QWidget* tab)
{
    SessionController* controller = qobject_cast<SessionController*>(container->viewProperties(tab));
    Q_ASSERT(controller);
    if (controller)
        controller->closeSession();
}

void ViewManager::setNavigationVisibility(int visibility)
{
    _navigationVisibility =
        static_cast<ViewContainer::NavigationVisibility>(visibility);

    foreach(ViewContainer* container, _viewSplitter->containers()) {
        container->setNavigationVisibility(_navigationVisibility);
    }
}

void ViewManager::setNavigationPosition(int position)
{
    _navigationPosition =
        static_cast<ViewContainer::NavigationPosition>(position);

    foreach(ViewContainer* container, _viewSplitter->containers()) {
        Q_ASSERT(container->supportedNavigationPositions().contains(_navigationPosition));
        container->setNavigationPosition(_navigationPosition);
    }
}

void ViewManager::setNavigationStyleSheet(const QString& styleSheet)
{
    _navigationStyleSheet = styleSheet;

    foreach(ViewContainer* container, _viewSplitter->containers()) {
        container->setStyleSheet(_navigationStyleSheet);
    }
}

void ViewManager::setShowQuickButtons(bool show)
{
    _showQuickButtons = show;

    foreach(ViewContainer* container, _viewSplitter->containers()) {
        if (_showQuickButtons) {
            container->setFeatures(container->features()
                                   | ViewContainer::QuickNewView
                                   | ViewContainer::QuickCloseView);
        } else {
            container->setFeatures(container->features()
                                   & ~ViewContainer::QuickNewView
                                   & ~ViewContainer::QuickCloseView);
        }
    }
}


void ViewManager::setNavigationBehavior(int behavior)
{
    _newTabBehavior = static_cast<NewTabBehavior>(behavior);
}

#include "ViewManager.moc"


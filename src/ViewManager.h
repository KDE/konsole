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

#ifndef VIEWMANAGER_H
#define VIEWMANAGER_H

// Qt
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPointer>

// Konsole
#include "Profile.h"
#include "ViewContainer.h"

class QSignalMapper;
class KActionCollection;
class KConfigGroup;

namespace Konsole
{
class ColorScheme;
class IncrementalSearchBar;
class Session;
class TerminalDisplay;

class SessionController;
class ViewProperties;
class ViewSplitter;

/**
 * Manages the terminal display widgets in a Konsole window or part.
 *
 * When a view manager is created, it constructs a splitter widget ( accessed via
 * widget() ) to hold one or more view containers.  Each view container holds
 * one or more terminal displays and a navigation widget ( eg. tabs or a list )
 * to allow the user to navigate between the displays in that container.
 *
 * The view manager provides menu actions ( defined in the 'konsoleui.rc' XML file )
 * to manipulate the views and view containers - for example, actions to split the view
 * left/right or top/bottom, detach a view from the current window and navigate between
 * views and containers.  These actions are added to the collection specified in the
 * ViewManager's constructor.
 *
 * The view manager provides facilities to construct display widgets for a terminal
 * session and also to construct the SessionController which provides the menus and other
 * user interface elements specific to each display/session pair.
 *
 */
class KONSOLEPRIVATE_EXPORT ViewManager : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.konsole.Window")

public:
    /**
     * Constructs a new view manager with the specified @p parent.
     * View-related actions defined in 'konsoleui.rc' are created
     * and added to the specified @p collection.
     */
    ViewManager(QObject* parent , KActionCollection* collection);
    ~ViewManager();

    /**
     * Creates a new view to display the output from and deliver input to @p session.
     * Constructs a new container to hold the views if no container has yet been created.
     */
    void createView(Session* session);

    /**
     * Applies the view-specific settings associated with specified @p profile
     * to the terminal display @p view.
     */
    void applyProfileToView(TerminalDisplay* view , const Profile::Ptr profile);

    /**
     * Return the main widget for the view manager which
     * holds all of the views managed by this ViewManager instance.
     */
    QWidget* widget() const;

    /**
     * Returns the view manager's active view.
     */
    QWidget* activeView() const;

    /**
     * Returns the list of view properties for views in the active container.
     * Each view widget is associated with a ViewProperties instance which
     * provides access to basic information about the session being
     * displayed in the view, such as title, current directory and
     * associated icon.
     */
    QList<ViewProperties*> viewProperties() const;

    /**
     * This enum describes the available types of navigation widget
     * which newly created containers can provide to allow navigation
     * between open sessions.
     */
    enum NavigationMethod {
        /**
         * Each container has a row of tabs (one per session) which the user
         * can click on to navigate between open sessions.
         */
        TabbedNavigation,
        /** The container has no navigation widget. */
        NoNavigation
    };

    /**
     * This enum describes where newly created tab should be placed.
     */
    enum NewTabBehavior {
        /** Put newly created tab at the end. */
        PutNewTabAtTheEnd = 0,
        /** Put newly created tab right after current tab. */
        PutNewTabAfterCurrentTab = 1
    };

    /**
     * Sets the type of widget provided to navigate between open sessions
     * in a container.  The changes will only apply to newly created containers.
     *
     * The default method is TabbedNavigation.  To disable navigation widgets, call
     * setNavigationMethod(ViewManager::NoNavigation) before creating any sessions.
     */
    void setNavigationMethod(NavigationMethod method);

    /**
     * Returns the type of navigation widget created in new containers.
     * See setNavigationMethod()
     */
    NavigationMethod navigationMethod() const;

    /**
     * Returns the controller for the active view.  activeViewChanged() is
     * emitted when this changes.
     */
    SessionController* activeViewController() const;

    /**
     * Returns the search bar.
     */
    IncrementalSearchBar* searchBar() const;

    /**
     * Session management
     */
    void saveSessions(KConfigGroup& group);
    void restoreSessions(const KConfigGroup& group);

    void setNavigationVisibility(int visibility);
    void setNavigationPosition(int position);
    void setNavigationBehavior(int behavior);
    void setNavigationStyleSheet(const QString& styleSheet);
    void setShowQuickButtons(bool show);

    int managerId() const;

    /** Returns a list of sessions in this ViewManager */
    QList<Session*> sessions() { return _sessionMap.values(); }

signals:
    /** Emitted when the last view is removed from the view manager */
    void empty();

    /** Emitted when a session is detached from a view owned by this ViewManager */
    void viewDetached(Session* session);

    /**
     * Emitted when the active view changes.
     * @param controller The controller associated with the active view
     */
    void activeViewChanged(SessionController* controller);

    /**
     * Emitted when the current session needs unplugged from factory().
     * @param controller The controller associated with the active view
     */
    void unplugController(SessionController* controller);

    /**
     * Emitted when the list of view properties ( as returned by viewProperties() ) changes.
     * This occurs when views are added to or removed from the active container, or
     * if the active container is changed.
     */
    void viewPropertiesChanged(const QList<ViewProperties*>& propertiesList);

    /**
     * Emitted when the number of views containers changes.  This is used to disable or
     * enable menu items which can only be used when there are one or multiple containers
     * visible.
     *
     * @param multipleViews True if there are multiple view containers open or false if there is
     * just a single view.
     */
    void splitViewToggle(bool multipleViews);

    /**
     * Emitted when menu bar visibility changes because a profile that requires so is
     * activated.
     */
    void setMenuBarVisibleRequest(bool);
    void updateWindowIcon();

    /** Requests creation of a new view with the default profile. */
    void newViewRequest();
    /** Requests creation of a new view, with the selected profile. */
    void newViewRequest(Profile::Ptr);

public slots:
    /** DBus slot that returns the number of sessions in the current view. */
    Q_SCRIPTABLE int sessionCount();

    /** DBus slot that returns the current (active) session window */
    Q_SCRIPTABLE int currentSession();

    /** DBus slot that creates a new session in the current view.
     * @param profile the name of the profile to be used
     * @param directory the working directory where the session is
     * started.
     */
    Q_SCRIPTABLE int newSession(QString profile, QString directory);

    // TODO: its semantic is application-wide. Move it to more appropriate place
    // DBus slot that returns the name of default profile
    Q_SCRIPTABLE QString defaultProfile();

    // TODO: its semantic is application-wide. Move it to more appropriate place
    // DBus slot that returns a string list of defined (known) profiles
    Q_SCRIPTABLE QStringList profileList();

    /** DBus slot that creates a new session in the current view with the associated
      * default profile and the default working directory
      */
    Q_SCRIPTABLE int newSession();

    /** DBus slot that changes the view port to the next session */
    Q_SCRIPTABLE void nextSession();

    /** DBus slot that changes the view port to the previous session */
    Q_SCRIPTABLE void prevSession();

    /** DBus slot that switches the current session (as returned by
      * currentSession()) with the left (or previous) one in the
      * navigation tab.
      */
    Q_SCRIPTABLE void moveSessionLeft();

    /** DBus slot that Switches the current session (as returned by
      * currentSession()) with the right (or next) one in the navigation
      * tab.
      */
    Q_SCRIPTABLE void moveSessionRight();

    /** DBus slot that sets ALL tabs' width to match their text */
    Q_SCRIPTABLE void setTabWidthToText(bool);

private slots:
    // called when the "Split View Left/Right" menu item is selected
    void splitLeftRight();
    void splitTopBottom();
    void closeActiveContainer();
    void closeOtherContainers();
    void expandActiveContainer();
    void shrinkActiveContainer();

    // called when the "Detach View" menu item is selected
    void detachActiveView();
    void updateDetachViewState();

    // called when a session terminates - the view manager will delete any
    // views associated with the session
    void sessionFinished();
    // called when one view has been destroyed
    void viewDestroyed(QWidget* widget);

    // controller detects when an associated view is given the focus
    // and emits a signal.  ViewManager listens for that signal
    // and then plugs the action into the UI
    //void viewFocused( SessionController* controller );

    // called when the active view in a ViewContainer changes, so
    // that we can plug the appropriate actions into the UI
    void viewActivated(QWidget* view);

    // called when "Next View" shortcut is activated
    void nextView();

    // called when "Previous View" shortcut is activated
    void previousView();

    // called when "Switch to last tab" shortcut is activated
    void lastView();

    // called when "Next View Container" shortcut is activated
    void nextContainer();

    // called when the views in a container owned by this view manager
    // changes
    void containerViewsChanged(QObject* container);

    // called when a profile changes
    void profileChanged(Profile::Ptr profile);

    void updateViewsForSession(Session* session);

    // moves active view to the left
    void moveActiveViewLeft();
    // moves active view to the right
    void moveActiveViewRight();
    // switches to the view at visual position 'index'
    // in the current container
    void switchToView(int index);

    // called when a SessionController gains focus
    void controllerChanged(SessionController* controller);

    // called when a ViewContainer requests a view be
    // moved
    void containerMoveViewRequest(int index, int id, bool& success, TabbedViewContainer* sourceTabbedContainer);

    void detachView(ViewContainer* container, QWidget* view);

    void closeTabFromContainer(ViewContainer* container, QWidget* view);

private:
    void createView(Session* session, ViewContainer* container, int index);
    static const ColorScheme* colorSchemeForProfile(const Profile::Ptr profile);

    void setupActions();
    void focusActiveView();

    // takes a view from a view container owned by a different manager and places it in
    // newContainer owned by this manager
    void takeView(ViewManager* otherManager , ViewContainer* otherContainer, ViewContainer* newContainer, TerminalDisplay* view);
    void splitView(Qt::Orientation orientation);

    // creates a new container which can hold terminal displays
    ViewContainer* createContainer();
    // removes a container and emits appropriate signals
    void removeContainer(ViewContainer* container);

    // creates a new terminal display
    // the 'session' is used so that the terminal display's random seed
    // can be set to something which depends uniquely on that session
    TerminalDisplay* createTerminalDisplay(Session* session = 0);

    // creates a new controller for a session/display pair which provides the menu
    // actions associated with that view, and exposes basic information
    // about the session ( such as title and associated icon ) to the display.
    SessionController* createController(Session* session , TerminalDisplay* display);

private:
    QPointer<ViewSplitter>          _viewSplitter;
    QPointer<SessionController>     _pluggedController;

    QHash<TerminalDisplay*, Session*> _sessionMap;

    KActionCollection*                  _actionCollection;
    QSignalMapper*                      _containerSignalMapper;

    NavigationMethod  _navigationMethod;

    ViewContainer::NavigationVisibility _navigationVisibility;
    ViewContainer::NavigationPosition _navigationPosition;
    bool _showQuickButtons;
    NewTabBehavior _newTabBehavior;
    QString _navigationStyleSheet;

    int _managerId;
    static int lastManagerId;
};
}

#endif

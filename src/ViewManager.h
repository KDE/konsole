/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VIEWMANAGER_H
#define VIEWMANAGER_H

// Qt
#include <QAction>
#include <QHash>
#include <QObject>
#include <QPointer>

#include "konsoleprivate_export.h"
// Konsole

class KActionCollection;
class KConfigGroup;

namespace Konsole
{
class ColorScheme;
class Profile;
class Session;
class SessionController;
class TabbedViewContainer;
class TabbedViewContainer;
class TerminalDisplay;
class ViewProperties;
class ViewSplitter;

/**
 * Manages the terminal display widgets in a Konsole window or part.
 *
 * When a view manager is created, it constructs a tab widget ( accessed via
 * widget() ) to hold one or more view splitters.  Each view splitter holds
 * one or more terminal displays  and splitters.
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
    ViewManager(QObject *parent, KActionCollection *collection);
    ~ViewManager() override;

    /**
     * Creates a new view to display the output from and deliver input to @p session.
     * Constructs a new container to hold the views if no container has yet been created.
     */
    void createView(TabbedViewContainer *tabWidget, Session *session);

    /*
     * Applies the view-specific settings associated with specified @p profile
     * to the terminal display @p view.
     */
    void applyProfileToView(TerminalDisplay *view, const QExplicitlySharedDataPointer<Profile> &profile);

    void toggleActionsBasedOnState();
    /**
     * Return the main widget for the view manager which
     * holds all of the views managed by this ViewManager instance.
     */
    QWidget *widget() const;

    /**
     * Returns the view manager's active view.
     */
    QWidget *activeView() const;

    /**
     * Returns the list of view properties for views in the active container.
     * Each view widget is associated with a ViewProperties instance which
     * provides access to basic information about the session being
     * displayed in the view, such as title, current directory and
     * associated icon.
     */
    QList<ViewProperties *> viewProperties() const;

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
        NoNavigation,
    };

    /**
     * Describes the options for showing or hiding the container's navigation widget.
     */
    enum NavigationVisibility {
        NavigationNotSet, // Don't rely on this information, Only use the settings.
        AlwaysShowNavigation,
        ShowNavigationAsNeeded,
        AlwaysHideNavigation,
    };

    /**
     * Sets the visibility of the view container's navigation widget.
     * The ViewContainer subclass is responsible for ensuring that this
     * setting is respected as views are dded or removed from the container
     */
    void setNavigationVisibility(NavigationVisibility navigationVisibility);

    /** Returns the current mode for controlling the visibility of the
     * view container's navigation widget.
     */
    NavigationVisibility navigationVisibility() const;

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
    SessionController *activeViewController() const;

    /**
     * Session management
     */
    void saveSessions(KConfigGroup &group);
    void restoreSessions(const KConfigGroup &group);

    int managerId() const;

    /** Returns a list of sessions in this ViewManager */
    QList<Session *> sessions()
    {
        return _sessionMap.values();
    }

    /**
     * Returns whether the @p profile has the blur setting enabled
     */
    static bool profileHasBlurEnabled(const QExplicitlySharedDataPointer<Profile> &profile);

    /** returns the active tab from the view
     */
    TabbedViewContainer *activeContainer();
    TerminalDisplay *createView(Session *session);
    void attachView(TerminalDisplay *terminal, Session *session);

    static const ColorScheme *colorSchemeForProfile(const QExplicitlySharedDataPointer<Profile> &profile);
    /** Reorder the terminal display history list */
    void updateTerminalDisplayHistory(TerminalDisplay *terminalDisplay = nullptr, bool remove = false);

    QHash<TerminalDisplay *, Session *> forgetAll(ViewSplitter *splitter);
    Session *forgetTerminal(TerminalDisplay *terminal);

    /**
     * Creates and returns new session
     *
     * The session has specified @p profile, working @p directory
     * and configured environment.
     */
    Session *createSession(const QExplicitlySharedDataPointer<Profile> &profile, const QString &directory = QString());

Q_SIGNALS:
    /** Emitted when the last view is removed from the view manager */
    void empty();

    /** Emitted when a session is detached from a view owned by this ViewManager */
    void terminalsDetached(ViewSplitter *splitter, QHash<TerminalDisplay *, Session *> sessionsMap);

    /**
     * Emitted when the active view changes.
     * @param controller The controller associated with the active view
     */
    void activeViewChanged(SessionController *controller);

    /**
     * Emitted when the current session needs unplugged from factory().
     * @param controller The controller associated with the active view
     */
    void unplugController(SessionController *controller);

    /**
     * Emitted when the list of view properties ( as returned by viewProperties() ) changes.
     * This occurs when views are added to or removed from the active container, or
     * if the active container is changed.
     */
    void viewPropertiesChanged(const QList<ViewProperties *> &propertiesList);

    /**
     * Emitted when menu bar visibility changes because a profile that requires so is
     * activated.
     */
    void setMenuBarVisibleRequest(bool);
    void updateWindowIcon();

    void blurSettingChanged(bool);

    /** Requests creation of a new view with the default profile. */
    void newViewRequest();
    /** Requests creation of a new view, with the selected profile. */
    void newViewWithProfileRequest(const QExplicitlySharedDataPointer<Profile> &profile);

public Q_SLOTS:
    /** DBus slot that returns the number of sessions in the current view. */
    Q_SCRIPTABLE int sessionCount();

    /**
     * DBus slot that returns the unique ids of the sessions in the
     * current view.  The returned list is ordered by tab.
     * QList<int> is not printable by qdbus so we use QStringList
     * Example:
     *  A) create tab, create tab 2, create tab 3, go to tab 2, split view
     *     sessionList() returns 1 4 2 3
     *  B) create tab, create tab 2, split view, create tab 3
     *     sessionList() returns 1 3 2 4
     */
    Q_SCRIPTABLE QStringList sessionList();

    /** DBus slot that returns the current (active) session window */
    Q_SCRIPTABLE int currentSession();

    /** DBus slot that sets the current (active) session window */
    Q_SCRIPTABLE void setCurrentSession(int sessionId);

    /** DBus slot that creates a new session in the current view with the associated
     * default profile and the default working directory
     */
    Q_SCRIPTABLE int newSession();

    /** DBus slot that creates a new session in the current view.
     * @param profile the name of the profile to be used
     * started.
     */
    Q_SCRIPTABLE int newSession(const QString &profile);

    /** DBus slot that creates a new session in the current view.
     * @param profile the name of the profile to be used
     * @param directory the working directory where the session is
     * started.
     */
    Q_SCRIPTABLE int newSession(const QString &profile, const QString &directory);

    // TODO: its semantic is application-wide. Move it to more appropriate place
    // DBus slot that returns the name of default profile
    Q_SCRIPTABLE QString defaultProfile();

    // TODO: its semantic is application-wide. Move it to more appropriate place
    // DBus slot that sets the default profile
    Q_SCRIPTABLE void setDefaultProfile(const QString &profile);

    // TODO: its semantic is application-wide. Move it to more appropriate place
    // DBus slot that returns a string list of defined (known) profiles
    Q_SCRIPTABLE QStringList profileList();

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

    // Creates json file with split config
    Q_SCRIPTABLE void saveLayoutFile();
    Q_SCRIPTABLE void loadLayoutFile();
    Q_SCRIPTABLE void loadLayout(QString File);

private Q_SLOTS:
    // called when the "Split View Left/Right" menu item is selected
    void splitLeftRight();
    void splitTopBottom();
    void expandActiveContainer();
    void shrinkActiveContainer();

    // called when the "Detach View" menu item is selected
    void detachActiveView();
    void detachActiveTab();

    // called when a session terminates - the view manager will delete any
    // views associated with the session
    void sessionFinished();
    // called when one view has been destroyed
    void viewDestroyed(QWidget *view);

    // controller detects when an associated view is given the focus
    // and emits a signal.  ViewManager listens for that signal
    // and then plugs the action into the UI
    // void viewFocused( SessionController* controller );

    // called when the active view in a ViewContainer changes, so
    // that we can plug the appropriate actions into the UI
    void activateView(TerminalDisplay *view);

    void focusUp();
    void focusDown();
    void focusLeft();
    void focusRight();

    // called when "Next View" shortcut is activated
    void nextView();

    // called when "Previous View" shortcut is activated
    void previousView();

    // called when "Switch to last tab" shortcut is activated
    void lastView();

    // called when "Switch to last used tab" shortcut is activated
    void lastUsedView();

    // called when "Switch to last used tab (reverse)" shortcut is activated
    void lastUsedViewReverse();

    // called when "Next View Container" shortcut is activated
    void nextContainer();

    // called when "Toggle Two tabs" shortcut is activated
    void toggleTwoViews();

    // called when the views in a container owned by this view manager
    // changes
    void containerViewsChanged(TabbedViewContainer *container);

    // called when a profile changes
    void profileChanged(const QExplicitlySharedDataPointer<Profile> &profile);

    void updateViewsForSession(Session *session);

    // moves active view to the left
    void moveActiveViewLeft();
    // moves active view to the right
    void moveActiveViewRight();
    // switches to the view at visual position 'index'
    // in the current container
    void switchToView(int index);
    // gives focus and switches the terminal display, changing tab if needed
    void switchToTerminalDisplay(TerminalDisplay *terminalDisplay);

    // called when a SessionController gains focus
    void controllerChanged(SessionController *controller);

    /* Detaches the tab at index tabIdx */
    void detachTab(int tabIdx);

private:
    Q_DISABLE_COPY(ViewManager)

    void createView(Session *session, TabbedViewContainer *container, int index);

    void setupActions();

    // takes a view from a view container owned by a different manager and places it in
    // newContainer owned by this manager
    void takeView(ViewManager *otherManager, TabbedViewContainer *otherContainer, TabbedViewContainer *newContainer, TerminalDisplay *view);
    void splitView(Qt::Orientation orientation);

    // creates a new container which can hold terminal displays
    TabbedViewContainer *createContainer();

    // creates a new terminal display
    // the 'session' is used so that the terminal display's random seed
    // can be set to something which depends uniquely on that session
    TerminalDisplay *createTerminalDisplay(Session *session = nullptr);

    // creates a new controller for a session/display pair which provides the menu
    // actions associated with that view, and exposes basic information
    // about the session ( such as title and associated icon ) to the display.
    SessionController *createController(Session *session, TerminalDisplay *view);
    void removeController(SessionController *controller);

    // Activates a different terminal when the TerminalDisplay
    // closes or is detached and another one should be focused.
    // It will activate the last used terminal within the same splitView
    // if possible otherwise it will focus the last used tab
    void focusAnotherTerminal(ViewSplitter *toplevelSplitter);

    void activateLastUsedView(bool reverse);

    void registerTerminal(TerminalDisplay *terminal);
    void unregisterTerminal(TerminalDisplay *terminal);

private:
    QPointer<TabbedViewContainer> _viewContainer;
    QPointer<SessionController> _pluggedController;

    QHash<TerminalDisplay *, Session *> _sessionMap;

    KActionCollection *_actionCollection;

    NavigationMethod _navigationMethod;
    NavigationVisibility _navigationVisibility;
    int _managerId;
    static int lastManagerId;
    QList<TerminalDisplay *> _terminalDisplayHistory;
    int _terminalDisplayHistoryIndex;

    // List of actions that should only be enabled when there are multiple view
    // containers open
    QList<QAction *> _multiTabOnlyActions;
    QList<QAction *> _multiSplitterOnlyActions;
};
}

#endif

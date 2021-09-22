/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VIEWCONTAINER_H
#define VIEWCONTAINER_H

// Qt
#include <QObject>
#include <QTabWidget>

// Konsole
#include "ViewManager.h"
#include "session/Session.h"

// Qt
class QPoint;
class QToolButton;
class QMenu;

namespace Konsole
{
class ViewProperties;
class ViewManager;
class TabbedViewContainer;

/**
 * An interface for container widgets which can hold one or more views.
 *
 * The container widget typically displays a list of the views which
 * it has and provides a means of switching between them.
 *
 * Subclasses should reimplement the addViewWidget() and removeViewWidget() functions
 * to actually add or remove view widgets from the container widget, as well
 * as updating any navigation aids.
 */
class KONSOLEPRIVATE_EXPORT TabbedViewContainer : public QTabWidget
{
    Q_OBJECT

public:
    /**
     * Constructs a new view container with the specified parent.
     *
     * @param connectedViewManager Connect the new view to this manager
     * @param parent The parent object of the container
     */
    TabbedViewContainer(ViewManager *connectedViewManager, QWidget *parent);

    /**
     * Called when the ViewContainer is destroyed.  When reimplementing this in
     * subclasses, use object->deleteLater() to delete any widgets or other objects
     * instead of 'delete object'.
     */
    ~TabbedViewContainer() override;

    /** Adds a new view to the container widget */
    void addView(TerminalDisplay *view);
    void addSplitter(ViewSplitter *viewSplitter, int index = -1);

    /** splits the currently focused Splitter */
    void splitView(TerminalDisplay *view, Qt::Orientation orientation);

    void setTabActivity(int index, bool activity);

    /** Sets tab title to item title if the view is active */
    void updateTitle(ViewProperties *item);
    /** Sets tab color to item color if the view is active */
    void updateColor(ViewProperties *item);
    /** Sets tab icon to item icon if the view is active */
    void updateIcon(ViewProperties *item);
    /** Sets tab activity status if the tab is not active */
    void updateActivity(ViewProperties *item);
    /** Sets tab notification */
    void updateNotification(ViewProperties *item, Konsole::Session::Notification notification, bool enabled);
    /** Sets tab special state (copy input or read-only) */
    void updateSpecialState(ViewProperties *item);

    /** Changes the active view to the next view */
    void activateNextView();

    /** Changes the active view to the previous view */
    void activatePreviousView();

    /** Changes the active view to the last view */
    void activateLastView();

    void setCssFromFile(const QUrl &url);

    ViewSplitter *activeViewSplitter();
    /**
     * This enum describes the directions
     * in which views can be re-arranged within the container
     * using the moveActiveView() method.
     */
    enum MoveDirection {
        /** Moves the view to the left. */
        MoveViewLeft,
        /** Moves the view to the right. */
        MoveViewRight,
    };

    /**
     * Moves the active view within the container and
     * updates the order in which the views are shown
     * in the container's navigation widget.
     *
     * The default implementation does nothing.
     */
    void moveActiveView(MoveDirection direction);

    /** Sets the menu to be shown when the new view button is clicked.
     * Only valid if the QuickNewView feature is enabled.
     * The default implementation does nothing. */
    // TODO: Reenable this later.
    //    void setNewViewMenu(QMenu *menu);
    void renameTab(int index);
    ViewManager *connectedViewManager();
    void currentTabChanged(int index);
    void closeCurrentTab();
    void wheelScrolled(int delta);
    void currentSessionControllerChanged(SessionController *controller);
    void tabDoubleClicked(int index);
    void openTabContextMenu(const QPoint &point);
    void setNavigationVisibility(ViewManager::NavigationVisibility navigationVisibility);
    void moveTabToWindow(int index, QWidget *window);

    void toggleMaximizeCurrentTerminal();
    /* return the widget(int index) casted to TerminalDisplay*
     *
     * The only thing that this class holds are TerminalDisplays, so
     * this is the only thing that should be used to retrieve widgets.
     */
    ViewSplitter *viewSplitterAt(int index);

    /**
     * Returns the number of split views (i.e. TerminalDisplay widgets)
     * in this tab; if there are no split views, 1 is returned.
     */
    int currentTabViewCount();

    void connectTerminalDisplay(TerminalDisplay *display);
    void disconnectTerminalDisplay(TerminalDisplay *display);
    void moveTabLeft();
    void moveTabRight();

    /**
     * This enum describes where newly created tab should be placed.
     */
    enum NewTabBehavior {
        /** Put newly created tab at the end. */
        PutNewTabAtTheEnd = 0,
        /** Put newly created tab right after current tab. */
        PutNewTabAfterCurrentTab = 1,
    };

    void setNavigationBehavior(int behavior);
    void terminalDisplayDropped(TerminalDisplay *terminalDisplay);

    void moveToNewTab(TerminalDisplay *display);

    QSize sizeHint() const override;

Q_SIGNALS:
    /** Emitted when the container has no more children */
    void empty(TabbedViewContainer *container);

    /** Emitted when the user requests to open a new view */
    void newViewRequest();

    /** Requests creation of a new view, with the selected profile. */
    void newViewWithProfileRequest(const QExplicitlySharedDataPointer<Profile> &profile);

    /** a terminalDisplay was dropped in a child Splitter */

    /**
     * Emitted when the user requests to move a view from another container
     * into this container.  If 'success' is set to true by a connected slot
     * then the original view will be removed.
     *
     * @param index Index at which to insert the new view in the container
     * or -1 to append it.  This index should be passed to addView() when
     * the new view has been created.
     * @param sessionControllerId The identifier of the view.
     */
    void moveViewRequest(int index, int sessionControllerId);

    /** Emitted when the active view changes */
    void activeViewChanged(TerminalDisplay *view);

    /** Emitted when a view is added to the container. */
    void viewAdded(TerminalDisplay *view);

    /** Emitted when a view is removed from container. */
    void viewRemoved();

    /** detach the specific tab */
    void detachTab(int tabIdx);

    /** set the color tab */
    void setColor(int index, const QColor &color);

    /** remve the color tab */
    void removeColor(int idx);

protected:
    // close tabs and unregister
    void closeTerminalTab(int idx);

    void keyReleaseEvent(QKeyEvent *event) override;
private Q_SLOTS:
    void viewDestroyed(QObject *view);
    void konsoleConfigChanged();

private:
    void forgetView();

    struct TabIconState {
        TabIconState()
            : readOnly(false)
            , broadcast(false)
            , notification(Session::NoNotification)
        {
        }

        bool readOnly;
        bool broadcast;
        Session::Notification notification;

        bool isAnyStateActive() const
        {
            return readOnly || broadcast || (notification != Session::NoNotification);
        }
    };

    bool _stylesheetSet = false;

    QHash<const QWidget *, TabIconState> _tabIconState;
    ViewManager *_connectedViewManager;
    QMenu *_contextPopupMenu;
    QToolButton *_newTabButton;
    QToolButton *_closeTabButton;
    int _contextMenuTabIndex;
    ViewManager::NavigationVisibility _navigationVisibility;
    NewTabBehavior _newTabBehavior;
};

}
#endif // VIEWCONTAINER_H

/*
    This file is part of the Konsole Terminal.

    Copyright 2006-2008 Robert Knight <robertknight@gmail.com>

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

#ifndef VIEWCONTAINER_H
#define VIEWCONTAINER_H

// Qt
#include <QObject>
#include <QTabWidget>

// Konsole
#include "Profile.h"
#include "ViewManager.h"

// Qt
class QPoint;
class QToolButton;
class QMenu;

namespace Konsole {
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
    ~TabbedViewContainer() Q_DECL_OVERRIDE;

    /** Adds a new view to the container widget */
    void addView(TerminalDisplay *view);
    void addSplitter(ViewSplitter *splitter, int index = -1);

    /** splits the currently focused Splitter */
    void splitView(TerminalDisplay *view, Qt::Orientation orientation);

    void setTabActivity(int index, bool activity);

    void updateTitle(ViewProperties *item);
    void updateIcon(ViewProperties *item);
    void updateActivity(ViewProperties *item);

    /** Changes the active view to the next view */
    void activateNextView();

    /** Changes the active view to the previous view */
    void activatePreviousView();

    /** Changes the active view to the last view */
    void activateLastView();

    void setCss(const QString& styleSheet = QString());
    void setCssFromFile(const QUrl& url);

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
        MoveViewRight
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

    void connectTerminalDisplay(TerminalDisplay *view);
    void disconnectTerminalDisplay(TerminalDisplay *view);
    void moveTabLeft();
    void moveTabRight();

    /**
     * This enum describes where newly created tab should be placed.
     */
    enum NewTabBehavior {
        /** Put newly created tab at the end. */
        PutNewTabAtTheEnd = 0,
        /** Put newly created tab right after current tab. */
        PutNewTabAfterCurrentTab = 1
    };

    void setNavigationBehavior(int behavior);
    void terminalDisplayDropped(TerminalDisplay* terminalDisplay);

    QSize sizeHint() const override;

Q_SIGNALS:
    /** Emitted when the container has no more children */
    void empty(TabbedViewContainer *container);

    /** Emitted when the user requests to open a new view */
    void newViewRequest();

    /** Requests creation of a new view, with the selected profile. */
    void newViewWithProfileRequest(const Profile::Ptr&);

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

    /** Emitted when a view is removed from the container. */
    void viewRemoved(TerminalDisplay *view);

    /** detach the specific tab */
    void detachTab(int tabIdx);

protected:
    // close tabs and unregister
    void closeTerminalTab(int idx);

    void keyReleaseEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
private Q_SLOTS:
    void viewDestroyed(QObject *view);
    void konsoleConfigChanged();

private:
    void forgetView(ViewSplitter *view);

    ViewManager *_connectedViewManager;
    QMenu *_contextPopupMenu;
    QToolButton *_newTabButton;
    QToolButton *_closeTabButton;
    int _contextMenuTabIndex;
    ViewManager::NavigationVisibility _navigationVisibility;
    NewTabBehavior _newTabBehavior;
};


}
#endif //VIEWCONTAINER_H

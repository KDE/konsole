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
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QHash>
#include <QtCore/QList>

// Konsole
#include "Profile.h"

class QStackedWidget;
class QWidget;
class QHBoxLayout;
class QVBoxLayout;

// TabbedViewContainer
// Qt
class QPoint;
class QToolButton;
class QMenu;
class QDropEvent;

// KDE
class KMenu;

namespace Konsole
{
class IncrementalSearchBar;
class ViewProperties;
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
class ViewContainer : public QObject
{
    Q_OBJECT

public:
    /**
     * This enum describes the options for positioning the
     * container's navigation widget.
     */
    enum NavigationPosition {
        /** Position the navigation widget above the views. */
        NavigationPositionTop,
        /** Position the navigation widget below the views. */
        NavigationPositionBottom,
        /** Position the navigation widget to the left of the views. */
        NavigationPositionLeft,
        /** Position the navigation widget to the right of the views. */
        NavigationPositionRight
    };

    /**
     * Constructs a new view container with the specified parent.
     *
     * @param position The initial position of the navigation widget
     * @param parent The parent object of the container
     */
    ViewContainer(NavigationPosition position , QObject* parent);

    /**
     * Called when the ViewContainer is destroyed.  When reimplementing this in
     * subclasses, use object->deleteLater() to delete any widgets or other objects
     * instead of 'delete object'.
     */
    virtual ~ViewContainer();

    /** Returns the widget which contains the view widgets */
    virtual QWidget* containerWidget() const = 0;

    /**
     * This enum describes the options for showing or hiding the
     * container's navigation widget.
     */
    enum NavigationVisibility {
        /** Always show the navigation widget. */
        AlwaysShowNavigation,
        /** Show the navigation widget only when the container has more than one view. */
        ShowNavigationAsNeeded,
        /** Always hide the navigation widget. */
        AlwaysHideNavigation
    };
    /*
     * Sets the visibility of the view container's navigation widget.
     *
     * The ViewContainer sub-class is responsible for ensuring that this
     * setting is respected as views are added or removed from the
     * container.
     *
     * ViewContainer sub-classes should reimplement the
     * navigationVisibilityChanged() method to respond to changes
     * of this property.
     */
    void setNavigationVisibility(NavigationVisibility mode);
    /**
     * Returns the current mode for controlling the visibility of the
     * the view container's navigation widget.
     */
    NavigationVisibility navigationVisibility() const;

    /**
     * Sets the position of the navigation widget with
     * respect to the main content area.
     *
     * Depending on the ViewContainer subclass, not all
     * positions from the NavigationPosition enum may be
     * supported.  A list of supported positions can be
     * obtained by calling supportedNavigationPositions()
     *
     * ViewContainer sub-classes should re-implement the
     * navigationPositionChanged() method to respond
     * to changes of this property.
     */
    void setNavigationPosition(NavigationPosition position);

    /**
     * Returns the position of the navigation widget with
     * respect to the main content area.
     */
    NavigationPosition navigationPosition() const;

    /**
     * Returns the list of supported navigation positions.
     * The supported positions will depend upon the type of the
     * navigation widget used by the ViewContainer subclass.
     *
     * The base implementation returns one item, NavigationPositionTop
     */
    virtual QList<NavigationPosition> supportedNavigationPositions() const;

    /** Sets the navigation text mode
     *  If mode is true, use the width of the title; otherwise use the
     *  default width calculations.
     */
    void setNavigationTextMode(bool mode);

    /** Sets the stylesheet for visual appearance
     *
     * The default implementation does nothing.
     */
    virtual void setStyleSheet(const QString& styleSheet) {
        Q_UNUSED(styleSheet);
    }

    /** Adds a new view to the container widget */
    void addView(QWidget* view , ViewProperties* navigationItem, int index = -1);

    /** Removes a view from the container */
    void removeView(QWidget* view);

    /** Returns the ViewProperties instance associated with a particular view in the container */
    ViewProperties* viewProperties(QWidget* view) const;

    /** Returns a list of the contained views */
    const QList<QWidget*> views() const;

    /**
     * Returns the view which currently has the focus or 0 if none
     * of the child views have the focus.
     */
    virtual QWidget* activeView() const = 0;

    /**
     * Changes the focus to the specified view and updates
     * navigation aids to reflect the change.
     */
    virtual void setActiveView(QWidget* widget) = 0;

    /**
     * @return the search widget for this view
     */
    IncrementalSearchBar* searchBar();

    /** Changes the active view to the next view */
    void activateNextView();

    /** Changes the active view to the previous view */
    void activatePreviousView();

    /** Changes the active view to the last view */
    void activateLastView();

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

    /** Enum describing extra UI features which can be
     * provided by the container. */
    enum Feature {
        /** Provides a button which can be clicked to create new views quickly.
         * When the button is clicked, a newViewRequest() signal is emitted. */
        QuickNewView = 1,
        /** Provides a button which can be clicked to close views quickly. */
        QuickCloseView = 2
    };
    Q_DECLARE_FLAGS(Features, Feature)
    /**
     * Sets which additional features are enabled in this container.
     * The default implementation does thing.  Sub-classes should re-implement this
     * to hide or show the relevant parts of their UI
     */
    virtual void setFeatures(Features features);
    /** Returns a bitwise-OR of enabled extra UI features.  See setFeatures() */
    Features features() const;
    /** Returns a bitwise-OR of supported extra UI features.  The default
     * implementation returns 0 (no extra features) */
    virtual Features supportedFeatures() const {
        return 0;
    }
    /** Sets the menu to be shown when the new view button is clicked.
     * Only valid if the QuickNewView feature is enabled.
     * The default implementation does nothing. */
    virtual void setNewViewMenu(QMenu* menu) {
        Q_UNUSED(menu);
    }

signals:
    /** Emitted when the container is deleted */
    void destroyed(ViewContainer* container);

    /** Emitted when the container has no more children */
    void empty(ViewContainer* container);

    /** Emitted when the user requests to open a new view */
    void newViewRequest();

    /** Requests creation of a new view, with the selected profile. */
    void newViewRequest(Profile::Ptr);

    /**
     * Emitted when the user requests to move a view from another container
     * into this container.  If 'success' is set to true by a connected slot
     * then the original view will be removed.
     *
     * @param index Index at which to insert the new view in the container or -1
     * to append it.  This index should be passed to addView() when the new view
     * has been created.
     * @param id The identifier of the view.
     * @param success The slot handling this signal should set this to true if the
     * new view was successfully created.
     */
    void moveViewRequest(int index, int id, bool& success);

    /** Emitted when the active view changes */
    void activeViewChanged(QWidget* view);

    /** Emitted when a view is added to the container. */
    void viewAdded(QWidget* view , ViewProperties* properties);

    /** Emitted when a view is removed from the container. */
    void viewRemoved(QWidget* view);

protected:
    /**
     * Performs the task of adding the view widget
     * to the container widget.
     */
    virtual void addViewWidget(QWidget* view, int index) = 0;
    /**
     * Performs the task of removing the view widget
     * from the container widget.
     */
    virtual void removeViewWidget(QWidget* view) = 0;

    /**
     * Called when the navigation display mode changes.
     * See setNavigationVisibility
     */
    virtual void navigationVisibilityChanged(NavigationVisibility) {}

    /**
     * Called when the navigation position changes to re-layout
     * the container and place the navigation widget in the
     * specified position.
     */
    virtual void navigationPositionChanged(NavigationPosition) {}

    virtual void navigationTextModeChanged(bool) {}

    /** Returns the widgets which are associated with a particular navigation item */
    QList<QWidget*> widgetsForItem(ViewProperties* item) const;

    /**
     * Rearranges the order of widgets in the container.
     *
     * @param fromIndex Current index of the widget to move
     * @param toIndex New index for the widget
     */
    virtual void moveViewWidget(int fromIndex , int toIndex);

private slots:
    void viewDestroyed(QObject* view);
    void searchBarDestroyed();

private:
    NavigationVisibility _navigationVisibility;
    NavigationPosition _navigationPosition;
    QList<QWidget*> _views;
    QHash<QWidget*, ViewProperties*> _navigation;
    Features _features;
    IncrementalSearchBar* _searchBar;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(ViewContainer::Features)

class ViewContainerTabBar;

/**
 * An alternative tabbed view container which uses a QTabBar and QStackedWidget
 * combination for navigation instead of QTabWidget
 */
class TabbedViewContainer : public ViewContainer
{
    Q_OBJECT

public:
    /**
     * Constructs a new tabbed view container.  Supported positions
     * are NavigationPositionTop and NavigationPositionBottom.
     */
    TabbedViewContainer(NavigationPosition position , QObject* parent);
    virtual ~TabbedViewContainer();

    virtual QWidget* containerWidget() const;
    virtual QWidget* activeView() const;
    virtual void setActiveView(QWidget* view);
    virtual QList<NavigationPosition> supportedNavigationPositions() const;
    virtual void setFeatures(Features features);
    virtual Features supportedFeatures() const;
    virtual void setNewViewMenu(QMenu* menu);
    virtual void setStyleSheet(const QString& styleSheet);

protected:
    virtual void addViewWidget(QWidget* view , int index);
    virtual void removeViewWidget(QWidget* view);
    virtual void navigationVisibilityChanged(NavigationVisibility mode);
    virtual void navigationPositionChanged(NavigationPosition position);
    virtual void navigationTextModeChanged(bool mode);
    virtual void moveViewWidget(int fromIndex , int toIndex);

private slots:
    void updateTitle(ViewProperties* item);
    void updateIcon(ViewProperties* item);
    void updateActivity(ViewProperties* item);
    void currentTabChanged(int index);
    void closeCurrentTab();
    void wheelScrolled(int delta);

    void tabDoubleClicked(int index);
    void openTabContextMenu(int index, const QPoint& point);
    void tabContextMenuCloseTab();
    void tabContextMenuRenameTab();
    void tabContextMenuDetachTab();
    void startTabDrag(int index);
    void querySourceIndex(const QDropEvent* event, int& sourceIndex);
    void onMoveViewRequest(int index, const QDropEvent* event, bool& success);

signals:
    void detachTab(ViewContainer * self, QWidget * activeView);
    void closeTab(ViewContainer * self, QWidget * activeView);

private:
    void dynamicTabBarVisibility();
    void setTabBarVisible(bool visible);
    void setTabActivity(int index, bool activity);
    void renameTab(int index);
    void updateVisibilityOfQuickButtons();

    ViewContainerTabBar* _tabBar;
    QPointer<QStackedWidget> _stackWidget;
    QPointer<QWidget> _containerWidget;
    QVBoxLayout* _layout;
    QHBoxLayout* _tabBarLayout;
    QToolButton* _newTabButton;
    QToolButton* _closeTabButton;
    int _contextMenuTabIndex;
    KMenu* _contextPopupMenu;
};

/** A plain view container with no navigation display */
class StackedViewContainer : public ViewContainer
{
public:
    explicit StackedViewContainer(QObject* parent);
    virtual ~StackedViewContainer();

    virtual QWidget* containerWidget() const;
    virtual QWidget* activeView() const;
    virtual void setActiveView(QWidget* view);

protected:
    virtual void addViewWidget(QWidget* view , int index);
    virtual void removeViewWidget(QWidget* view);

private:
    QPointer<QWidget> _containerWidget;
    QPointer<QStackedWidget> _stackWidget;
};
}
#endif //VIEWCONTAINER_H

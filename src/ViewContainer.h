/*
    This file is part of the Konsole Terminal.
    
    Copyright (C) 2006 Robert Knight <robertknight@gmail.com>

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
#include <QHash>
#include <QList>
#include <QTabBar>

class QSpacerItem;
class QStackedWidget;
class QWidget;


// TabbedViewContainer
    // Qt
    class QAction;
    class QPoint;
    class QWidgetAction;
    class QToolButton;
    class QMenu;

    // KDE
    class KTabWidget;
    class KColorCells;
    class KMenu;

// ListViewContainer
    class QSplitter;
    class QListWidget;

namespace Konsole
{
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
 * 
 */
class ViewContainer : public QObject
{
Q_OBJECT
    
public:
    /** Constructs a new view container with the specified parent. */
    ViewContainer(QObject* parent);

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
    enum NavigationDisplayMode
    {
        AlwaysShowNavigation,
        AlwaysHideNavigation,
        ShowNavigationAsNeeded
    };

    /** TODO: Document me. */
    void setNavigationDisplayMode(NavigationDisplayMode mode);
    /** TODO: Document me. */
    NavigationDisplayMode navigationDisplayMode() const;

    /** Adds a new view to the container widget */
    void addView(QWidget* view , ViewProperties* navigationItem);
 
    /** Removes a view from the container */
    void removeView(QWidget* view);

    /** Returns the ViewProperties instance associated with a particular view in the container */
    ViewProperties* viewProperties(  QWidget* view );   
    
    /** Returns a list of the contained views */
    const QList<QWidget*> views();
   
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
     * Changes the active view to the next view 
     */
    void activateNextView();

    /**
     * Changes the active view to the previous view
     */
    void activatePreviousView();

signals:
    /** Emitted when the container is deleted */
    void destroyed(ViewContainer* container);

    /** Emitted when the container has no more children */
    void empty(ViewContainer* container);

    /** Emitted when the user requests to close the a view */
    void closeRequest(QWidget* activeView);

    /** Emitted when the active view changes */
    void activeViewChanged( QWidget* view );

    /** Emitted when a view is added to the container. */
    void viewAdded(QWidget* view , ViewProperties* properties);

    /** Emitted when a view is removed from the container. */
    void viewRemoved(QWidget* view);

protected:
    /** 
     * Performs the task of adding the view widget
     * to the container widget.
     */
    virtual void addViewWidget(QWidget* view) = 0;
    /**
     * Performs the task of removing the view widget
     * from the container widget.
     */
    virtual void removeViewWidget(QWidget* view) = 0;
   
    /** 
     * Called when the navigation display mode changes.
     * See setNavigationDisplayMode
     */
    virtual void navigationDisplayModeChanged(NavigationDisplayMode) {};

    /** Returns the widgets which are associated with a particular navigation item */
    QList<QWidget*> widgetsForItem( ViewProperties* item ) const;

private slots:
    void viewDestroyed(QObject* view);

private:
    NavigationDisplayMode _navigationDisplayMode;
    QList<QWidget*> _views;
    QHash<QWidget*,ViewProperties*> _navigation;
};

/** 
 * A view container which uses a QTabWidget to display the views and 
 * allow the user to switch between them.
 */
class TabbedViewContainer : public ViewContainer  
{
    Q_OBJECT

public:
    TabbedViewContainer(QObject* parent);
    virtual ~TabbedViewContainer();
    
    virtual QWidget* containerWidget() const;
    virtual QWidget* activeView() const;
    virtual void setActiveView(QWidget* view);

    void setNewSessionMenu(QMenu* menu);
     
protected:
    virtual void addViewWidget( QWidget* view );
    virtual void removeViewWidget( QWidget* view ); 

private slots:
    void updateTitle(ViewProperties* item);
    void updateIcon(ViewProperties* item);
    void closeTabClicked();
    void selectTabColor();
    void prepareColorCells();
    void showContextMenu(QWidget* widget , const QPoint& position);
    void currentTabChanged(int index);


private:
    KTabWidget* _tabWidget; 
    QList<QAction*> _viewActions;

    QToolButton*    _newSessionButton;
    QMenu*          _newSessionMenu;

    KMenu*          _tabContextMenu;
    KMenu*          _tabSelectColorMenu;
    QWidgetAction*  _tabColorSelector; 
    KColorCells*    _tabColorCells;

    int _contextMenuTab;
};

// internal class,
// to allow for tweaks to the tab bar required by TabbedViewContainerV2.
// does not actually do anything currently
class ViewContainerTabBar : public QTabBar
{
public:
    ViewContainerTabBar(QWidget* parent = 0);

protected:
    virtual QSize tabSizeHint(int index) const;
};

/** 
 * An alternative tabbed view container which uses a QTabBar and QStackedWidget
 * combination for navigation instead of QTabWidget
 */
class TabbedViewContainerV2 : public ViewContainer
{
    Q_OBJECT

public:
    TabbedViewContainerV2(QObject* parent);
    virtual ~TabbedViewContainerV2();

    virtual QWidget* containerWidget() const;
    virtual QWidget* activeView() const;
    virtual void setActiveView(QWidget* view);

protected:
    virtual void addViewWidget(QWidget* view);
    virtual void removeViewWidget(QWidget* view);
    virtual void navigationDisplayModeChanged(NavigationDisplayMode mode);

private slots:
    void updateTitle(ViewProperties* item);
    void updateIcon(ViewProperties* item);
    void currentTabChanged(int index);

private:
    void dynamicTabBarVisibility();
    void setTabBarVisible(bool visible);

    ViewContainerTabBar* _tabBar;
    QStackedWidget* _stackWidget;
    QWidget* _containerWidget;
    QSpacerItem* _tabBarSpacer;

    static const int TabBarSpace = 2;
};

/**
 * A plain view container with no navigation display
 */
class StackedViewContainer : public ViewContainer
{
public:
    StackedViewContainer(QObject* parent);
    virtual ~StackedViewContainer();

    virtual QWidget* containerWidget() const;
    virtual QWidget* activeView() const;
    virtual void setActiveView(QWidget* view);

protected:
    virtual void addViewWidget( QWidget* view );
    virtual void removeViewWidget( QWidget* view );

private:
    QStackedWidget* _stackWidget;
};

/**
 * A view container which uses a list instead of tabs to provide navigation
 * between sessions.
 */
class ListViewContainer : public ViewContainer
{
Q_OBJECT

public:
    ListViewContainer(QObject* parent);
    virtual ~ListViewContainer();

    virtual QWidget* containerWidget() const;
    virtual QWidget* activeView() const;
    virtual void setActiveView(QWidget* view);

protected:
    virtual void addViewWidget( QWidget* view );
    virtual void removeViewWidget( QWidget* view );

private slots:
    void rowChanged( int row );
    void updateTitle( ViewProperties* );
    void updateIcon( ViewProperties* );

private:
    QBrush randomItemBackground(int randomIndex);

    QStackedWidget* _stackWidget;
    QSplitter* _splitter;
    QListWidget* _listWidget;
};

}
#endif //VIEWCONTAINER_H

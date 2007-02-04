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

#ifndef VIEWMANAGER_H
#define VIEWMANAGER_H

// Qt
#include <QHash>
#include <QObject>
#include <QPointer>

class KToggleAction;

class KonsoleMainWindow;
class TESession;
class TEWidget;

class SessionController;
class ViewProperties;
class ViewContainer;
class ViewSplitter;

/** 
 * Manages the views and view container widgets in a Konsole window.  Each Konsole window
 * has one ViewManager.
 * The view manager is responsible for creating new terminal displays for sessions and the 
 * controllers which connect the view and session to provide the menu actions associated with
 * the view, as well as exposing basic information ( such as associated title and icon ) to
 * the view. 
 *
 * Each Konsole window contains a number of view containers, which are instances of ViewContainer.
 * Each view container may contain one or more views, along with a navigation widget
 * (eg. tabs or a list) which allows the user to choose between the views in that container.  
 * 
 * When a ViewManager is instantiated, it creates a new view container and adds it to the 
 * main window associated with the ViewManager which is specified in the constructor.
 *
 * To create new terminal displays inside the container widget, use the createView() method.
 *
 * ViewContainers can be merged together, that is, the views contained in one container can be moved
 * into another using the merge() method.
 */
class ViewManager : public QObject
{
Q_OBJECT

public:
    /** 
     * Constructs a new view manager associated with @p mainWindow, and
     * adds a view container widget to the main window
     */
    ViewManager(KonsoleMainWindow* mainWindow);
    ~ViewManager();

    /**
     * Creates a new view to display the outout from and deliver input to @p session.
     * Constructs a new container to hold the views if no container has yet been created.
     */
    void createView(TESession* session);

    /**
     * Merges views from another view manager into this manager.
     * Only views from the active container in the other manager are merged.
     */
    void merge(ViewManager* manager);

signals:
    /** Emitted when the last view is removed from the view manager */
    void empty();

    /** Emitted when a session is detached from a view owned by this ViewManager */
    void viewDetached(TESession* session);

private slots:
    // called when the "Split View" menu item is selected
    void splitView(bool splitView);
    // called when the "Detach View" menu item is selected
    void detachActiveView();
    // called when a session terminates - the view manager will delete any
    // views associated with the session
    void sessionFinished( TESession* session );
    // called when the container requests to close a particular view
    void viewCloseRequest(QWidget* widget);

    // controller detects when an associated view is given the focus
    // and emits a signal.  ViewManager listens for that signal
    // and then plugs the action into the UI
    void viewFocused( SessionController* controller );

    // called when the active view in a ViewContainer changes, so
    // that we can plug the appropriate actions into the UI
    void viewActivated( QWidget* view );

    // called when the title of the active view changes
    void activeViewTitleChanged( ViewProperties* );
private:
    void setupActions();
    void focusActiveView();
    void registerView(TEWidget* view);
    void unregisterView(TEWidget* view);
    // takes a view from a view container owned by a different manager and places it in 
    // newContainer owned by this manager
    void takeView(ViewManager* otherManager , ViewContainer* otherContainer, ViewContainer* newContainer, TEWidget* view); 

    // creates a new container which can hold terminal displays
    ViewContainer* createContainer();
    // creates a new terminal display
    TEWidget* createTerminalDisplay();
    // applies the view-specific settings such as colour scheme associated
    // with 'session' to 'view'
    void loadViewSettings(TEWidget* view , TESession* session);

    // creates a new controller for a session/display pair which provides the menu
    // actions associated with that view, and exposes basic information
    // about the session ( such as title and associated icon ) to the display.
    SessionController* createController(TESession* session , TEWidget* display);

private:
    KonsoleMainWindow*          _mainWindow;
    KToggleAction*              _splitViewAction;
    ViewSplitter*               _viewSplitter;
    QPointer<SessionController> _pluggedController;
    QHash<TEWidget*,TESession*> _sessionMap;
    
};

#endif

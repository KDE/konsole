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
class ViewContainer;
class ViewSplitter;

/** 
 * Manages the views and view container widgets in a KonsoleMainWindow.
 * 
 * When a ViewManager is instantiated, it adds a container widget for terminal displays to the 
 * KonsoleMainWindow widget passed as an argument to the constructor.
 *
 * To create new terminal displays inside the container widget, use the createView() method.
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
    void splitView(bool splitView);
    void detachActiveView();
    // called when a session terminates - the view manager will delete any
    // views associated with the session
    void sessionFinished( TESession* session );
    // called when the container requests to close a particular view
    void viewCloseRequest(QWidget* widget);

    void viewFocused( SessionController* controller );
    void viewActivated( QWidget* view );

private:
    void setupActions();
    void focusActiveView();
    void registerView(TEWidget* view);
    void unregisterView(TEWidget* view);

    ViewContainer* createContainer();
    // creates a new terminal display
    TEWidget* createTerminalDisplay();
    // applies the view-specific settings such as colour scheme associated
    // with 'session' to 'view'
    void loadViewSettings(TEWidget* view , TESession* session);
    SessionController* createController(TESession* session , TEWidget* display);

private:
    KonsoleMainWindow*          _mainWindow;
    KToggleAction*              _splitViewAction;
    ViewSplitter*               _viewSplitter;
    QPointer<SessionController> _pluggedController;
    QHash<TEWidget*,TESession*> _sessionMap;
    
};

#endif

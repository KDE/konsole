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

#ifndef KONSOLEAPP_H
#define KONSOLEAPP_H

// KDE 
#include <KUniqueApplication>

class KCmdLineArgs;

namespace Konsole
{
class ColorSchemeManager;
class ProfileList;
class SessionManager;
class ViewManager;
class MainWindow;
class Session;

/**
 * The Konsole Application.
 *
 * The application consists of one or more main windows and a set of factories to create 
 * new sessions and views.
 *
 * To create a new main window with a default terminal session, call the newInstance() method.
 * Empty main windows can be created using newMainWindow().
 *
 * The factory used to create new terminal sessions can be retrieved using the sessionManager() accessor.
 */
class Application : public KUniqueApplication
{
Q_OBJECT

public:
    /** Constructs a new Konsole application. */
    Application();

    virtual ~Application();

    /** Creates a new main window and opens a default terminal session */
    virtual int newInstance();

    /** 
     * Creates a new, empty main window and returns a pointer to the created window 
     * 
     * DESIGN ISSUE:  This is the only way that new main windows should be created because
     *                Application needs to connect up certain signals from the window to itself
     *                perhaps it would be better if MainWindow hooked itself up to Application
     *                rather than the other way round?  
     */
    MainWindow* newMainWindow();

    /** Returns the Application instance */
    static Application* self();

private slots:
    void createSession(const QString& key, ViewManager* view);
    void detachView(Session* session);

    void toggleBackgroundInstance();

private:
    KCmdLineArgs*   _arguments;
    ProfileList*    _sessionList;
    
    MainWindow* _backgroundInstance;
};

}
#endif //KONSOLEAPP_H

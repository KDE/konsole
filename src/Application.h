/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>

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

#ifndef APPLICATION_H
#define APPLICATION_H

// KDE
#include <KUniqueApplication>

// Konsole
#include "Profile.h"

class KCmdLineArgs;

namespace Konsole
{
class MainWindow;
class Session;

/**
 * The Konsole Application.
 *
 * The application consists of one or more main windows and a set of
 * factories to create new sessions and views.
 *
 * To create a new main window with a default terminal session, call
 * the newInstance(). Empty main windows can be created using newMainWindow().
 *
 * The factory used to create new terminal sessions can be retrieved using
 * the sessionManager() accessor.
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
     * Creates a new, empty main window and connects to its newSessionRequest()
     * and newWindowRequest() signals to trigger creation of new sessions or
     * windows when then they are emitted.
     */
    MainWindow* newMainWindow();

private slots:
    void createWindow(Profile::Ptr profile , const QString& directory);
    void detachView(Session* session);

    void toggleBackgroundInstance();

private:
    void init();
    void listAvailableProfiles();
    void listProfilePropertyInfo();
    void startBackgroundMode(MainWindow* window);
    bool processHelpArgs(KCmdLineArgs* args);
    MainWindow* processWindowArgs(KCmdLineArgs* args);
    Profile::Ptr processProfileSelectArgs(KCmdLineArgs* args);
    Profile::Ptr processProfileChangeArgs(KCmdLineArgs* args, Profile::Ptr baseProfile);
    void processTabsFromFileArgs(KCmdLineArgs* args, MainWindow* window);
    void createTabFromArgs(KCmdLineArgs* args, MainWindow* window,
                           const QHash<QString, QString>&);

    MainWindow* _backgroundInstance;
};
}
#endif  // APPLICATION_H

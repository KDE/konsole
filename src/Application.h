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

// Qt
#include <QCommandLineParser>

// Konsole
#include "Profile.h"
#include "ViewSplitter.h"

namespace Konsole {
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
class Application : public QObject
{
    Q_OBJECT

public:
    /** Constructs a new Konsole application. */
    explicit Application(QSharedPointer<QCommandLineParser> parser,
                         const QStringList &customCommand);

    static void populateCommandLineParser(QCommandLineParser *parser);
    static QStringList getCustomCommand(QStringList &args);

    ~Application() override;

    /** Creates a new main window and opens a default terminal session */
    int newInstance();

    /**
     * Creates a new, empty main window and connects to its newSessionRequest()
     * and newWindowRequest() signals to trigger creation of new sessions or
     * windows when then they are emitted.
     */
    MainWindow *newMainWindow();

private Q_SLOTS:
    void createWindow(const Profile::Ptr &profile, const QString &directory);
    void detachTerminals(ViewSplitter *splitter, const QHash<TerminalDisplay*, Session*>& sessionsMap);

    void toggleBackgroundInstance();

public Q_SLOTS:
    void slotActivateRequested(QStringList args, const QString &workingDir);

private:
    Q_DISABLE_COPY(Application)

    void listAvailableProfiles();
    void listProfilePropertyInfo();
    void startBackgroundMode(MainWindow *window);
    bool processHelpArgs();
    MainWindow *processWindowArgs(bool &createdNewMainWindow);
    Profile::Ptr processProfileSelectArgs();
    Profile::Ptr processProfileChangeArgs(Profile::Ptr baseProfile);
    bool processTabsFromFileArgs(MainWindow *window);
    void createTabFromArgs(MainWindow *window, const QHash<QString, QString> &);
    void finalizeNewMainWindow(MainWindow *window);

    MainWindow *_backgroundInstance;
    QSharedPointer<QCommandLineParser> m_parser;
    QStringList m_customCommand;
};
}
#endif  // APPLICATION_H

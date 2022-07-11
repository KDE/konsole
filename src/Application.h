/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef APPLICATION_H
#define APPLICATION_H

// Qt
#include <QCommandLineParser>

// Konsole
#include "konsole_export.h"
#include "pluginsystem/PluginManager.h"
#include "profile/Profile.h"
#include "terminalDisplay/TerminalDisplay.h"
#include "widgets/ViewSplitter.h"

namespace Konsole
{
class MainWindow;
class Session;
class Profile;

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
class KONSOLE_EXPORT Application : public QObject
{
    Q_OBJECT

public:
    /** Constructs a new Konsole application. */
    explicit Application(QSharedPointer<QCommandLineParser> parser, const QStringList &customCommand);

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
    void createWindow(const QExplicitlySharedDataPointer<Profile> &profile, const QString &directory);
    void detachTerminals(MainWindow *currentWindow, ViewSplitter *splitter, const QHash<TerminalDisplay *, Session *> &sessionsMap);

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
    QExplicitlySharedDataPointer<Profile> processProfileSelectArgs();
    QExplicitlySharedDataPointer<Profile> processProfileChangeArgs(QExplicitlySharedDataPointer<Profile> baseProfile);
    bool processTabsFromFileArgs(MainWindow *window);
    void createTabFromArgs(MainWindow *window, const QHash<QString, QString> &);

    MainWindow *_backgroundInstance;
    QSharedPointer<QCommandLineParser> m_parser;
    QStringList m_customCommand;
    PluginManager m_pluginManager;
};
}
#endif // APPLICATION_H

/*
    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>

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

// Own
#include "Application.h"

// Qt
#include <QtCore/QHashIterator>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QCommandLineParser>
#include <QStandardPaths>
#include <QDebug>

// KDE
#include <KActionCollection>

// Konsole
#include "SessionManager.h"
#include "ProfileManager.h"
#include "MainWindow.h"
#include "Session.h"
#include "ShellCommand.h"
#include "KonsoleSettings.h"
#include "ViewManager.h"
#include "SessionController.h"

using namespace Konsole;

Application::Application(QCommandLineParser &parser) : m_parser(parser)
{
    _backgroundInstance = 0;
}

Application::~Application()
{
    SessionManager::instance()->closeAllSessions();
    ProfileManager::instance()->saveSettings();
}

MainWindow* Application::newMainWindow()
{
    MainWindow* window = new MainWindow();
    window->setTransparency(!m_parser.isSet("notransparency"));

    connect(window, &Konsole::MainWindow::newWindowRequest, this, &Konsole::Application::createWindow);
    connect(window, &Konsole::MainWindow::viewDetached, this, &Konsole::Application::detachView);

    return window;
}

void Application::createWindow(Profile::Ptr profile, const QString& directory)
{
    MainWindow* window = newMainWindow();
    window->createSession(profile, directory);
    finalizeNewMainWindow(window);
}

void Application::detachView(Session* session)
{
    MainWindow* window = newMainWindow();
    window->createView(session);

    // When detaching a view, the size of the new window should equal the
    // size of the source window
    Session* newsession = window->viewManager()->activeViewController()->session();
    newsession->setSize(session->size());
    window->adjustSize();
    // Since user is dragging and dropping, move dnd window to where
    // the user has the cursor (correct multiple monitor setups).
    window->move(QCursor::pos());
    window->show();
}

int Application::newInstance()
{
    // handle session management

    // returns from processWindowArgs(args, createdNewMainWindow)
    // if a new window was created
    bool createdNewMainWindow = false;

    // check for arguments to print help or other information to the
    // terminal, quit if such an argument was found
    if (processHelpArgs())
        return 0;

    // create a new window or use an existing one
    MainWindow* window = processWindowArgs(createdNewMainWindow);

    if (m_parser.isSet("tabs-from-file")) {
        // create new session(s) as described in file
        processTabsFromFileArgs(window);
    } else {
        // select profile to use
        Profile::Ptr baseProfile = processProfileSelectArgs();

        // process various command-line options which cause a property of the
        // selected profile to be changed
        Profile::Ptr newProfile = processProfileChangeArgs(baseProfile);

        // create new session
        Session* session = window->createSession(newProfile, QString());

        if (m_parser.isSet("noclose")) {
            session->setAutoClose(false);
        }
    }

    // if the background-mode argument is supplied, start the background
    // session ( or bring to the front if it already exists )
    if (m_parser.isSet("background-mode")) {
        startBackgroundMode(window);
    } else {
        // Qt constrains top-level windows which have not been manually
        // resized (via QWidget::resize()) to a maximum of 2/3rds of the
        //  screen size.
        //
        // This means that the terminal display might not get the width/
        // height it asks for.  To work around this, the widget must be
        // manually resized to its sizeHint().
        //
        // This problem only affects the first time the application is run.
        // run. After that KMainWindow will have manually resized the
        // window to its saved size at this point (so the Qt::WA_Resized
        // attribute will be set)

        // If not restoring size from last time or only adding new tab,
        // resize window to chosen profile size (see Bug:345403)
        if (createdNewMainWindow){
            finalizeNewMainWindow(window);
        } else{
            window->show();
        }
    }

    return 1;
}

/* Documentation for tab file:
 *
 * ;; is the token separator
 * # at the beginning of line results in line being ignored.
 * supported tokens are title, command and profile.
 *
 * Note that the title is static and the tab will close when the
 * command is complete (do not use --noclose).  You can start new tabs.
 *
 * Examples:
title: This is the title;; command: ssh jupiter
title: Top this!;; command: top
#this line is comment
command: ssh  earth
profile: Zsh
*/
void Application::processTabsFromFileArgs(MainWindow* window)
{
    // Open tab configuration file
    const QString tabsFileName(m_parser.value("tabs-from-file"));
    QFile tabsFile(tabsFileName);
    if (!tabsFile.open(QFile::ReadOnly)) {
        qWarning() << "ERROR: Cannot open tabs file "
                   << tabsFileName.toLocal8Bit().data();
        return;
    }

    unsigned int sessions = 0;
    while (!tabsFile.atEnd()) {
        QString lineString(tabsFile.readLine().trimmed());
        if ((lineString.isEmpty()) || (lineString[0] == '#'))
            continue;

        QHash<QString, QString> lineTokens;
        QStringList lineParts = lineString.split(QStringLiteral(";;"), QString::SkipEmptyParts);

        for (int i = 0; i < lineParts.size(); ++i) {
            QString key = lineParts.at(i).section(':', 0, 0).trimmed().toLower();
            QString value = lineParts.at(i).section(':', 1, -1).trimmed();
            lineTokens[key] = value;
        }
        // should contain at least one of 'command' and 'profile'
        if (lineTokens.contains(QLatin1String("command")) || lineTokens.contains(QStringLiteral("profile"))) {
            createTabFromArgs(window, lineTokens);
            sessions++;
        } else {
            qWarning() << "Each line should contain at least one of 'command' and 'profile'.";
        }
    }
    tabsFile.close();

    if (sessions < 1) {
        qWarning() << "No valid lines found in "
                   << tabsFileName.toLocal8Bit().data();
        return;
    }
}

void Application::createTabFromArgs(MainWindow* window,
                                    const QHash<QString, QString>& tokens)
{
    const QString& title = tokens["title"];
    const QString& command = tokens["command"];
    const QString& profile = tokens["profile"];
    const QString& workdir = tokens["workdir"];

    Profile::Ptr baseProfile;
    if (!profile.isEmpty()) {
        baseProfile = ProfileManager::instance()->loadProfile(profile);
    }
    if (!baseProfile) {
        // fallback to default profile
        baseProfile = ProfileManager::instance()->defaultProfile();
    }

    Profile::Ptr newProfile = Profile::Ptr(new Profile(baseProfile));
    newProfile->setHidden(true);

    // FIXME: the method of determining whether to use newProfile does not
    // scale well when we support more fields in the future
    bool shouldUseNewProfile = false;

    if (!command.isEmpty()) {
        newProfile->setProperty(Profile::Command,   command);
        newProfile->setProperty(Profile::Arguments, command.split(' '));
        shouldUseNewProfile = true;
    }

    if (!title.isEmpty()) {
        newProfile->setProperty(Profile::LocalTabTitleFormat, title);
        newProfile->setProperty(Profile::RemoteTabTitleFormat, title);
        shouldUseNewProfile = true;
    }

    if (m_parser.isSet("workdir")) {
        newProfile->setProperty(Profile::Directory, m_parser.value("workdir"));
        shouldUseNewProfile = true;
    }

    if (!workdir.isEmpty()) {
        newProfile->setProperty(Profile::Directory, workdir);
        shouldUseNewProfile = true;
    }

    // Create the new session
    Profile::Ptr theProfile = shouldUseNewProfile ? newProfile :  baseProfile;
    Session* session = window->createSession(theProfile, QString());

    if (m_parser.isSet("noclose")) {
        session->setAutoClose(false);
    }

    if (!window->testAttribute(Qt::WA_Resized)) {
        window->resize(window->sizeHint());
    }

    // FIXME: this ugly hack here is to make the session start running, so that
    // its tab title is displayed as expected.
    //
    // This is another side effect of the commit fixing BKO 176902.
    window->show();
    window->hide();
}

// Creates a new Konsole window.
// If --new-tab is given, use existing window.
MainWindow* Application::processWindowArgs(bool &createdNewMainWindow)
{
    MainWindow* window = 0;
    if (m_parser.isSet("new-tab")) {
        QListIterator<QWidget*> iter(QApplication::topLevelWidgets());
        iter.toBack();
        while (iter.hasPrevious()) {
            window = qobject_cast<MainWindow*>(iter.previous());
            if (window != 0)
                break;
        }
    }

    if (window == 0) {
        createdNewMainWindow = true;
        window = newMainWindow();

        // override default menubar visibility
        if (m_parser.isSet("show-menubar")) {
            window->setMenuBarInitialVisibility(true);
        }
        if (m_parser.isSet("hide-menubar")) {
            window->setMenuBarInitialVisibility(false);
        }
        if (m_parser.isSet("fullscreen")) {
            window->viewFullScreen(true);
        }

        // override default tabbbar visibility
        // FIXME: remove those magic number
        // see ViewContainer::NavigationVisibility
        if (m_parser.isSet("show-tabbar")) {
            // always show
            window->setNavigationVisibility(0);
        }
        if (m_parser.isSet("hide-tabbar")) {
            // never show
            window->setNavigationVisibility(2);
        }
    }
    return window;
}

// Loads a profile.
// If --profile <name> is given, loads profile <name>.
// If --fallback-profile is given, loads profile FALLBACK/.
// Else loads the default profile.
Profile::Ptr Application::processProfileSelectArgs()
{
    Profile::Ptr defaultProfile = ProfileManager::instance()->defaultProfile();

    if (m_parser.isSet("profile")) {
        Profile::Ptr profile = ProfileManager::instance()->loadProfile(
                                   m_parser.value("profile"));
        if (profile)
            return profile;
    } else if (m_parser.isSet("fallback-profile")) {
        Profile::Ptr profile = ProfileManager::instance()->loadProfile(QStringLiteral("FALLBACK/"));
        if (profile)
            return profile;
    }

    return defaultProfile;
}

bool Application::processHelpArgs()
{
    if (m_parser.isSet("list-profiles")) {
        listAvailableProfiles();
        return true;
    } else if (m_parser.isSet("list-profile-properties")) {
        listProfilePropertyInfo();
        return true;
    }
    return false;
}

void Application::listAvailableProfiles()
{
    QStringList paths = ProfileManager::instance()->availableProfilePaths();

    foreach(const QString& path, paths) {
        QFileInfo info(path);
        printf("%s\n", info.completeBaseName().toLocal8Bit().constData());
    }

    return;
}

void Application::listProfilePropertyInfo()
{
    Profile::Ptr tempProfile = ProfileManager::instance()->defaultProfile();
    const QStringList names = tempProfile->propertiesInfoList();

    foreach(const QString& name, names) {
        printf("%s\n", name.toLocal8Bit().constData());
    }

    return;
}

Profile::Ptr Application::processProfileChangeArgs(Profile::Ptr baseProfile)
{
    bool shouldUseNewProfile = false;

    Profile::Ptr newProfile = Profile::Ptr(new Profile(baseProfile));
    newProfile->setHidden(true);

    // change the initial working directory
    if (m_parser.isSet("workdir")) {
        newProfile->setProperty(Profile::Directory, m_parser.value("workdir"));
        shouldUseNewProfile = true;
    }

    // temporary changes to profile options specified on the command line
    foreach(const QString & value , m_parser.values("p")) {
        ProfileCommandParser parser;

        QHashIterator<Profile::Property, QVariant> iter(parser.parse(value));
        while (iter.hasNext()) {
            iter.next();
            newProfile->setProperty(iter.key(), iter.value());
        }

        shouldUseNewProfile = true;
    }

    // run a custom command
    if (m_parser.isSet("e")) {
        QString commandExec = m_parser.value("e");
        QStringList commandArguments;

        if (m_parser.positionalArguments().count() == 0 &&
            QStandardPaths::findExecutable(commandExec).isEmpty()) {
            // Example: konsole -e "man ls"
            ShellCommand shellCommand(m_parser.value("e"));
            commandExec = shellCommand.command();
            commandArguments = shellCommand.arguments();
        } else {
            // Example: konsole -e man ls
            commandArguments << commandExec;
            for ( int i = 0 ; i < m_parser.positionalArguments().count() ; i++ )
                commandArguments << m_parser.positionalArguments().at(i);
        }

        if (commandExec.startsWith(QLatin1String("./")))
            commandExec = QDir::currentPath() + commandExec.mid(1);

        newProfile->setProperty(Profile::Command, commandExec);
        newProfile->setProperty(Profile::Arguments, commandArguments);

        shouldUseNewProfile = true;
    }

    if (shouldUseNewProfile) {
        return newProfile;
    } else {
        return baseProfile;
    }
}

void Application::startBackgroundMode(MainWindow* window)
{
    if (_backgroundInstance) {
        return;
    }

/* FIXME: This doesn't work ATM - leave in here so I dont' forget about it
    KActionCollection* collection = window->actionCollection();
    QAction * action = collection->addAction("toggle-background-window");
    action->setObjectName(QLatin1String("Konsole Background Mode"));
    action->setText(i18n("Toggle Background Window"));
    action->setGlobalShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_F12)));

    connect(action, &QAction::triggered, this, &Application::toggleBackgroundInstance);
*/
    _backgroundInstance = window;
}

void Application::toggleBackgroundInstance()
{
    Q_ASSERT(_backgroundInstance);

    if (!_backgroundInstance->isVisible()) {
        _backgroundInstance->show();
        // ensure that the active terminal display has the focus. Without
        // this, an odd problem occurred where the focus widget would change
        // each time the background instance was shown
        _backgroundInstance->setFocus();
    } else {
        _backgroundInstance->hide();
    }
}

void Application::slotActivateRequested (const QStringList &args, const QString & /*workingDir*/)
{
    if (!args.isEmpty()) {
        m_parser.parse(args);
    }
    newInstance();
}

void Application::finalizeNewMainWindow(MainWindow* window)
{
    if (!KonsoleSettings::saveGeometryOnExit())
        window->resize(window->sizeHint());
    window->show();
}

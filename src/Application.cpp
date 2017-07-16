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
#include <QHashIterator>
#include <QFileInfo>
#include <QDir>
#include <QCommandLineParser>
#include <QStandardPaths>

// KDE
#include <KActionCollection>
#include <KLocalizedString>

// Konsole
#include "SessionManager.h"
#include "ProfileManager.h"
#include "MainWindow.h"
#include "Session.h"
#include "ShellCommand.h"
#include "KonsoleSettings.h"
#include "ViewManager.h"
#include "SessionController.h"
#include "WindowSystemInfo.h"

using namespace Konsole;

Application::Application(QSharedPointer<QCommandLineParser> parser,
                         const QStringList &customCommand) :
    _backgroundInstance(nullptr),
    m_parser(parser),
    m_customCommand(customCommand)
{
}

void Application::populateCommandLineParser(QCommandLineParser *parser)
{
    parser->addOption(QCommandLineOption(QStringList() << QStringLiteral("profile"),
                                         i18nc("@info:shell",
                                               "Name of profile to use for new Konsole instance"),
                                         QStringLiteral("name")));
    parser->addOption(QCommandLineOption(QStringList(QStringLiteral("fallback-profile")),
                                         i18nc("@info:shell",
                                               "Use the internal FALLBACK profile")));
    parser->addOption(QCommandLineOption(QStringList() << QStringLiteral("workdir"),
                                         i18nc("@info:shell",
                                               "Set the initial working directory of the new tab or"
                                               " window to 'dir'"),
                                         QStringLiteral("dir")));
    parser->addOption(QCommandLineOption(QStringList() << QStringLiteral("hold")
                                                       << QStringLiteral("noclose"),
                                         i18nc("@info:shell",
                                               "Do not close the initial session automatically when it"
                                               " ends.")));
    parser->addOption(QCommandLineOption(QStringList() << QStringLiteral("new-tab"),
                                         i18nc("@info:shell",
                                               "Create a new tab in an existing window rather than"
                                               " creating a new window")));
    parser->addOption(QCommandLineOption(QStringList() << QStringLiteral("tabs-from-file"),
                                         i18nc("@info:shell",
                                               "Create tabs as specified in given tabs configuration"
                                               " file"),
                                         QStringLiteral("file")));
    parser->addOption(QCommandLineOption(QStringList() << QStringLiteral("background-mode"),
                                         i18nc("@info:shell",
                                               "Start Konsole in the background and bring to the front"
                                               " when Ctrl+Shift+F12 (by default) is pressed")));
    // --nofork is a compatibility alias for separate
    parser->addOption(QCommandLineOption(QStringList() << QStringLiteral("separate")
                                                       << QStringLiteral("nofork"),
                                         i18nc("@info:shell", "Run in a separate process")));
    parser->addOption(QCommandLineOption(QStringList() << QStringLiteral("show-menubar"),
                                         i18nc("@info:shell",
                                               "Show the menubar, overriding the default setting")));
    parser->addOption(QCommandLineOption(QStringList() << QStringLiteral("hide-menubar"),
                                         i18nc("@info:shell",
                                               "Hide the menubar, overriding the default setting")));
    parser->addOption(QCommandLineOption(QStringList() << QStringLiteral("show-tabbar"),
                                         i18nc("@info:shell",
                                               "Show the tabbar, overriding the default setting")));
    parser->addOption(QCommandLineOption(QStringList() << QStringLiteral("hide-tabbar"),
                                         i18nc("@info:shell",
                                               "Hide the tabbar, overriding the default setting")));
    parser->addOption(QCommandLineOption(QStringList() << QStringLiteral("fullscreen"),
                                         i18nc("@info:shell", "Start Konsole in fullscreen mode")));
    parser->addOption(QCommandLineOption(QStringList() << QStringLiteral("notransparency"),
                                         i18nc("@info:shell",
                                               "Disable transparent backgrounds, even if the system"
                                               " supports them.")));
    parser->addOption(QCommandLineOption(QStringList() << QStringLiteral("list-profiles"),
                                         i18nc("@info:shell", "List the available profiles")));
    parser->addOption(QCommandLineOption(QStringList() << QStringLiteral("list-profile-properties"),
                                         i18nc("@info:shell",
                                               "List all the profile properties names and their type"
                                               " (for use with -p)")));
    parser->addOption(QCommandLineOption(QStringList() << QStringLiteral("p"),
                                         i18nc("@info:shell",
                                               "Change the value of a profile property."),
                                         QStringLiteral("property=value")));
    parser->addOption(QCommandLineOption(QStringList() << QStringLiteral("e"),
                                         i18nc("@info:shell",
                                               "Command to execute. This option will catch all following"
                                               " arguments, so use it as the last option."),
                                         QStringLiteral("cmd")));
    parser->addPositionalArgument(QStringLiteral("[args]"),
                                  i18nc("@info:shell", "Arguments passed to command"));

    // Add a no-op compatibility option to make Konsole compatible with
    // Debian's policy on X terminal emulators.
    // -T is technically meant to set a title, that is not really meaningful
    // for Konsole as we have multiple user-facing options controlling
    // the title and overriding whatever is set elsewhere.
    // https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=532029
    // https://www.debian.org/doc/debian-policy/ch-customized-programs.html#s11.8.3
    auto titleOption = QCommandLineOption(QStringList() << QStringLiteral("T"),
                                          QStringLiteral("Debian policy compatibility, not used"),
                                          QStringLiteral("value"));
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    titleOption.setHidden(true);
#endif
    parser->addOption(titleOption);
}

QStringList Application::getCustomCommand(QStringList &args)
{
    int i = args.indexOf(QLatin1String("-e"));
    QStringList customCommand;
    if ((0 < i) && (i < (args.size() - 1))) {
        // -e was specified with at least one extra argument
        // if -e was specified without arguments, QCommandLineParser will deal
        // with that
        args.removeAt(i);
        while (args.size() > i) {
            customCommand << args.takeAt(i);
        }
    }
    return customCommand;
}

Application::~Application()
{
    SessionManager::instance()->closeAllSessions();
    ProfileManager::instance()->saveSettings();
}

MainWindow *Application::newMainWindow()
{
    WindowSystemInfo::HAVE_TRANSPARENCY = !m_parser->isSet(QStringLiteral("notransparency"));

    auto window = new MainWindow();

    connect(window, &Konsole::MainWindow::newWindowRequest, this,
            &Konsole::Application::createWindow);
    connect(window, &Konsole::MainWindow::viewDetached, this, &Konsole::Application::detachView);

    return window;
}

void Application::createWindow(Profile::Ptr profile, const QString &directory)
{
    MainWindow *window = newMainWindow();
    window->createSession(profile, directory);
    finalizeNewMainWindow(window);
}

void Application::detachView(Session *session)
{
    MainWindow *window = newMainWindow();
    window->createView(session);

    // When detaching a view, the size of the new window should equal the
    // size of the source window
    Session *newsession = window->viewManager()->activeViewController()->session();
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
    if (processHelpArgs()) {
        return 0;
    }

    // create a new window or use an existing one
    MainWindow *window = processWindowArgs(createdNewMainWindow);

    if (m_parser->isSet(QStringLiteral("tabs-from-file"))) {
        // create new session(s) as described in file
        if (!processTabsFromFileArgs(window)) {
            return 0;
        }
    }

    // select profile to use
    Profile::Ptr baseProfile = processProfileSelectArgs();

    // process various command-line options which cause a property of the
    // selected profile to be changed
    Profile::Ptr newProfile = processProfileChangeArgs(baseProfile);

    // create new session
    Session *session = window->createSession(newProfile, QString());

    if (m_parser->isSet(QStringLiteral("noclose"))) {
        session->setAutoClose(false);
    }

    // if the background-mode argument is supplied, start the background
    // session ( or bring to the front if it already exists )
    if (m_parser->isSet(QStringLiteral("background-mode"))) {
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
        if (createdNewMainWindow) {
            finalizeNewMainWindow(window);
        } else {
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
bool Application::processTabsFromFileArgs(MainWindow *window)
{
    // Open tab configuration file
    const QString tabsFileName(m_parser->value(QStringLiteral("tabs-from-file")));
    QFile tabsFile(tabsFileName);
    if (!tabsFile.open(QFile::ReadOnly)) {
        qWarning() << "ERROR: Cannot open tabs file "
                   << tabsFileName.toLocal8Bit().data();
        return false;
    }

    unsigned int sessions = 0;
    while (!tabsFile.atEnd()) {
        QString lineString(QString::fromUtf8(tabsFile.readLine()).trimmed());
        if ((lineString.isEmpty()) || (lineString[0] == QLatin1Char('#'))) {
            continue;
        }

        QHash<QString, QString> lineTokens;
        QStringList lineParts = lineString.split(QStringLiteral(";;"), QString::SkipEmptyParts);

        for (int i = 0; i < lineParts.size(); ++i) {
            QString key = lineParts.at(i).section(QLatin1Char(':'), 0, 0).trimmed().toLower();
            QString value = lineParts.at(i).section(QLatin1Char(':'), 1, -1).trimmed();
            lineTokens[key] = value;
        }
        // should contain at least one of 'command' and 'profile'
        if (lineTokens.contains(QStringLiteral("command"))
            || lineTokens.contains(QStringLiteral("profile"))) {
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
        return false;
    }

    return true;
}

void Application::createTabFromArgs(MainWindow *window, const QHash<QString, QString> &tokens)
{
    const QString &title = tokens[QStringLiteral("title")];
    const QString &command = tokens[QStringLiteral("command")];
    const QString &profile = tokens[QStringLiteral("profile")];
    const QString &workdir = tokens[QStringLiteral("workdir")];

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
        newProfile->setProperty(Profile::Command, command);
        newProfile->setProperty(Profile::Arguments, command.split(QLatin1Char(' ')));
        shouldUseNewProfile = true;
    }

    if (!title.isEmpty()) {
        newProfile->setProperty(Profile::LocalTabTitleFormat, title);
        newProfile->setProperty(Profile::RemoteTabTitleFormat, title);
        shouldUseNewProfile = true;
    }

    if (m_parser->isSet(QStringLiteral("workdir"))) {
        newProfile->setProperty(Profile::Directory, m_parser->value(QStringLiteral("workdir")));
        shouldUseNewProfile = true;
    }

    if (!workdir.isEmpty()) {
        newProfile->setProperty(Profile::Directory, workdir);
        shouldUseNewProfile = true;
    }

    // Create the new session
    Profile::Ptr theProfile = shouldUseNewProfile ? newProfile : baseProfile;
    Session *session = window->createSession(theProfile, QString());

    if (m_parser->isSet(QStringLiteral("noclose"))) {
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
MainWindow *Application::processWindowArgs(bool &createdNewMainWindow)
{
    MainWindow *window = nullptr;
    if (m_parser->isSet(QStringLiteral("new-tab"))) {
        QListIterator<QWidget *> iter(QApplication::topLevelWidgets());
        iter.toBack();
        while (iter.hasPrevious()) {
            window = qobject_cast<MainWindow *>(iter.previous());
            if (window != nullptr) {
                break;
            }
        }
    }

    if (window == nullptr) {
        createdNewMainWindow = true;
        window = newMainWindow();

        // override default menubar visibility
        if (m_parser->isSet(QStringLiteral("show-menubar"))) {
            window->setMenuBarInitialVisibility(true);
        }
        if (m_parser->isSet(QStringLiteral("hide-menubar"))) {
            window->setMenuBarInitialVisibility(false);
        }
        if (m_parser->isSet(QStringLiteral("fullscreen"))) {
            window->viewFullScreen(true);
        }

        // override default tabbbar visibility
        // FIXME: remove those magic number
        // see ViewContainer::NavigationVisibility
        if (m_parser->isSet(QStringLiteral("show-tabbar"))) {
            // always show
            window->setNavigationVisibility(0);
        }
        if (m_parser->isSet(QStringLiteral("hide-tabbar"))) {
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

    if (m_parser->isSet(QStringLiteral("profile"))) {
        Profile::Ptr profile = ProfileManager::instance()->loadProfile(
            m_parser->value(QStringLiteral("profile")));
        if (profile) {
            return profile;
        }
    } else if (m_parser->isSet(QStringLiteral("fallback-profile"))) {
        Profile::Ptr profile = ProfileManager::instance()->loadProfile(QStringLiteral("FALLBACK/"));
        if (profile) {
            return profile;
        }
    }

    return defaultProfile;
}

bool Application::processHelpArgs()
{
    if (m_parser->isSet(QStringLiteral("list-profiles"))) {
        listAvailableProfiles();
        return true;
    } else if (m_parser->isSet(QStringLiteral("list-profile-properties"))) {
        listProfilePropertyInfo();
        return true;
    }
    return false;
}

void Application::listAvailableProfiles()
{
    QStringList paths = ProfileManager::instance()->availableProfilePaths();

    foreach (const QString &path, paths) {
        QFileInfo info(path);
        printf("%s\n", info.completeBaseName().toLocal8Bit().constData());
    }
}

void Application::listProfilePropertyInfo()
{
    Profile::Ptr tempProfile = ProfileManager::instance()->defaultProfile();
    const QStringList names = tempProfile->propertiesInfoList();

    foreach (const QString &name, names) {
        printf("%s\n", name.toLocal8Bit().constData());
    }
}

Profile::Ptr Application::processProfileChangeArgs(Profile::Ptr baseProfile)
{
    bool shouldUseNewProfile = false;

    Profile::Ptr newProfile = Profile::Ptr(new Profile(baseProfile));
    newProfile->setHidden(true);

    // change the initial working directory
    if (m_parser->isSet(QStringLiteral("workdir"))) {
        newProfile->setProperty(Profile::Directory, m_parser->value(QStringLiteral("workdir")));
        shouldUseNewProfile = true;
    }

    // temporary changes to profile options specified on the command line
    foreach (const QString &value, m_parser->values(QLatin1String("p"))) {
        ProfileCommandParser parser;

        QHashIterator<Profile::Property, QVariant> iter(parser.parse(value));
        while (iter.hasNext()) {
            iter.next();
            newProfile->setProperty(iter.key(), iter.value());
        }

        shouldUseNewProfile = true;
    }

    // run a custom command
    if (!m_customCommand.isEmpty()) {
        // Example: konsole -e man ls
        QString commandExec = m_customCommand[0];
        QStringList commandArguments(m_customCommand);
        if ((m_customCommand.size() == 1)
            && (QStandardPaths::findExecutable(commandExec).isEmpty())) {
            // Example: konsole -e "man ls"
            ShellCommand shellCommand(commandExec);
            commandExec = shellCommand.command();
            commandArguments = shellCommand.arguments();
        }

        if (commandExec.startsWith(QLatin1String("./"))) {
            commandExec = QDir::currentPath() + commandExec.mid(1);
        }

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

void Application::startBackgroundMode(MainWindow *window)
{
    if (_backgroundInstance != nullptr) {
        return;
    }

/* FIXME: This doesn't work ATM - leave in here so I dont' forget about it
    KActionCollection* collection = window->actionCollection();
    QAction * action = collection->addAction("toggle-background-window");
    action->setObjectName(QLatin1String("Konsole Background Mode"));
    action->setText(i18n("Toggle Background Window"));
    action->setGlobalShortcut(QKeySequence(Konsole::ACCEL + Qt::SHIFT + Qt::Key_F12)));

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

void Application::slotActivateRequested(QStringList args, const QString & /*workingDir*/)
{
    // QCommandLineParser expects the first argument to be the executable name
    // In the current version it just strips it away
    args.prepend(qApp->applicationFilePath());

    m_customCommand = getCustomCommand(args);

    // We can't re-use QCommandLineParser instances, it preserves earlier parsed values
    auto parser = new QCommandLineParser;
    populateCommandLineParser(parser);
    parser->parse(args);
    m_parser.reset(parser);

    newInstance();
}

void Application::finalizeNewMainWindow(MainWindow *window)
{
    if (!KonsoleSettings::saveGeometryOnExit()) {
        window->resize(window->sizeHint());
    }
    window->show();
}

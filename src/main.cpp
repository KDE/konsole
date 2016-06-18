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
#include "MainWindow.h"
#include "config-konsole.h"

// OS specific
#include <qplatformdefs.h>
#include <QCommandLineParser>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

// KDE
#include <KAboutData>
#include <KLocalizedString>
#include <Kdelibs4ConfigMigrator>
#include <Kdelibs4Migration>
#include <kdbusservice.h>

using Konsole::Application;

// fill the KAboutData structure with information about contributors to Konsole.
void fillAboutData(KAboutData& aboutData);

// fill the KCmdLineOptions object with konsole specific options.
void fillCommandLineOptions(QCommandLineParser &parser);

// check and report whether this konsole instance should use a new konsole
// process, or re-use an existing konsole process.
bool shouldUseNewProcess(int argc, char *argv[]);

// restore sessions saved by KDE.
void restoreSession(Application& app);

// Workaround for a bug in KDBusService: https://bugs.kde.org/show_bug.cgi?id=355545
// It calls exit(), but the program can't exit before the QApplication is deleted:
// https://bugreports.qt.io/browse/QTBUG-48709
static bool needToDeleteQApplication = false;
void deleteQApplication()
{
    if (needToDeleteQApplication) {
        delete qApp;
    }
}

// ***
// Entry point into the Konsole terminal application.
// ***
extern "C" int Q_DECL_EXPORT kdemain(int argc, char* argv[])
{
    // Check if any of the arguments makes it impossible to re-use an existing process.
    // We need to do this manually and before creating a QApplication, because
    // QApplication takes/removes the Qt specific arguments that are incompatible.
    KDBusService::StartupOption startupOption = KDBusService::Unique;
    if (shouldUseNewProcess(argc, argv)) {
        startupOption = KDBusService::Multiple;
    } else {
        needToDeleteQApplication = true;
    }

    QApplication *app = new QApplication(argc, argv);

    // enable high dpi support
    app->setAttribute(Qt::AA_UseHighDpiPixmaps, true);

#if defined(Q_OS_MAC)
    // this ensures that Ctrl and Meta are not swapped, so CTRL-C and friends
    // will work correctly in the terminal
    app->setAttribute(Qt::AA_MacDontSwapCtrlAndMeta);

    // KDE's menuBar()->isTopLevel() hasn't worked in a while.
    // For now, put menus inside Konsole window; this also make
    // the keyboard shortcut to show menus look reasonable.
    app->setAttribute(Qt::AA_DontUseNativeMenuBar);
#endif

    KLocalizedString::setApplicationDomain("konsole");

    KAboutData about(QStringLiteral("konsole"),
                     i18nc("@title", "Konsole"),
                     QStringLiteral(KONSOLE_VERSION),
                     i18nc("@title", "Terminal emulator"),
                     KAboutLicense::GPL_V2,
                     i18n("(c) 1997-2015, The Konsole Developers"),
                     QStringLiteral(),
                     QStringLiteral("https://konsole.kde.org/"));
    fillAboutData(about);

    KAboutData::setApplicationData(about);

    QCommandLineParser parser;
    parser.setApplicationDescription(about.shortDescription());
    parser.addHelpOption();
    parser.addVersionOption();
    fillCommandLineOptions(parser);
    about.setupCommandLine(&parser);

    parser.process(*app);
    about.processCommandLine(&parser);

    atexit(deleteQApplication);
    // Ensure that we only launch a new instance if we need to
    // If there is already an instance running, we will quit here
    KDBusService dbusService(startupOption | KDBusService::NoExitOnFailure);

    needToDeleteQApplication = false;

    Kdelibs4ConfigMigrator migrate(QStringLiteral("konsole"));
    migrate.setConfigFiles(QStringList() << QStringLiteral("konsolerc") << QStringLiteral("konsole.notifyrc"));
    migrate.setUiFiles(QStringList() << QStringLiteral("sessionui.rc") << QStringLiteral("partui.rc") << QStringLiteral("konsoleui.rc"));

    if (migrate.migrate()) {
        Kdelibs4Migration dataMigrator;
        const QString sourceBasePath = dataMigrator.saveLocation("data", "konsole");
        const QString targetBasePath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/konsole/");
        QString targetFilePath;

        QDir sourceDir(sourceBasePath);
        QDir targetDir(targetBasePath);

        if(sourceDir.exists()) {
            if(!targetDir.exists()) {
                QDir().mkpath(targetBasePath);
            }
            QStringList fileNames = sourceDir.entryList(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);
            foreach (const QString &fileName, fileNames) {
                targetFilePath = targetBasePath + fileName;
                if(!QFile::exists(targetFilePath))  {
                    QFile::copy(sourceBasePath + fileName, targetFilePath);
                }
            }
        }
    }

    // If we reach this location, there was no existing copy of Konsole
    // running, so create a new instance.
    Application konsoleApp(parser);

    // The activateRequested() signal is emitted when a second instance
    // of Konsole is started.
    QObject::connect(&dbusService, SIGNAL(activateRequested(QStringList,QString)),
                     &konsoleApp, SLOT(slotActivateRequested(QStringList,QString)));

    if (!konsoleApp.newInstance()) {
        // An argument that printed something and mean we should just quit was passed.
        delete app;
        return 0;
    }

    if (app->isSessionRestored())
        restoreSession(konsoleApp);

    // Since we've allocated the QApplication on the heap for the KDBusService workaround,
    // we need to delete it manually before returning from main().
    int ret = app->exec();
    delete app;
    return ret;
}

bool shouldUseNewProcess(int argc, char *argv[])
{
    // The "unique process" model of konsole is incompatible with some or all
    // Qt/KDE options. When those incompatible options are given, konsole must
    // use new process
    //
    // TODO: make sure the existing list is OK and add more incompatible options.

    // We need to manually parse the arguments because QApplication removes the
    // Qt specific arguments (like --reverse)
    QStringList arguments;
    for (int i=0; i < argc; i++) {
        arguments.append(QString::fromLocal8Bit(argv[i]));
    }

    // take Qt options into consideration
    QStringList qtProblematicOptions;
    qtProblematicOptions << "--session" << "--name" << "--reverse"
                         << "--stylesheet" << "--graphicssystem";
#if HAVE_X11
    qtProblematicOptions << "--display" << "--visual";
#endif
    foreach(const QString& option, qtProblematicOptions) {
        if (arguments.contains(option)) {
            return true;
        }
    }

    // take KDE options into consideration
    QStringList kdeProblematicOptions;
    kdeProblematicOptions << "--config" << "--style";
#if HAVE_X11
    kdeProblematicOptions << "--waitforwm";
#endif

    foreach(const QString& option, kdeProblematicOptions) {
        if (arguments.contains(option)) {
            return true;
        }
    }

    // if users have explictly requested starting a new process
    if (arguments.contains("--separate")) {
        return true;
    }

    // the only way to create new tab is to reuse existing Konsole process.
    if (arguments.contains("--new-tab")) {
        return false;
    }

    // when starting Konsole from a terminal, a new process must be used
    // so that the current environment is propagated into the shells of the new
    // Konsole and any debug output or warnings from Konsole are written to
    // the current terminal
    bool hasControllingTTY = false;
    const int fd = QT_OPEN("/dev/tty", O_RDONLY);
    if (fd != -1) {
        hasControllingTTY = true;
        close(fd);
    }

    return hasControllingTTY;
}

void fillCommandLineOptions(QCommandLineParser &parser)
{
    parser.addOption(QCommandLineOption(QStringList() << "profile",
                                        i18nc("@info:shell", "Name of profile to use for new Konsole instance"),
                                        QStringLiteral("name")));
    parser.addOption(QCommandLineOption(QStringList() << "fallback-profile",
                                        i18nc("@info:shell", "Use the internal FALLBACK profile")));
    parser.addOption(QCommandLineOption(QStringList() << "workdir",
                                        i18nc("@info:shell", "Set the initial working directory of the new tab or"
                                              " window to 'dir'"),
                                        QStringLiteral("dir")));
    parser.addOption(QCommandLineOption(QStringList() << "hold" << "noclose",
                                        i18nc("@info:shell", "Do not close the initial session automatically when it"
                                        " ends.")));
    parser.addOption(QCommandLineOption(QStringList() << "new-tab",
                                        i18nc("@info:shell", "Create a new tab in an existing window rather than"
                                              " creating a new window")));
    parser.addOption(QCommandLineOption(QStringList() << "tabs-from-file",
                                        i18nc("@info:shell", "Create tabs as specified in given tabs configuration"
                                        " file"),
                                        QStringLiteral("file")));
    parser.addOption(QCommandLineOption(QStringList() << "background-mode",
                                        i18nc("@info:shell", "Start Konsole in the background and bring to the front"
                                              " when Ctrl+Shift+F12 (by default) is pressed")));
    parser.addOption(QCommandLineOption(QStringList() << "separate", i18n("Run in a separate process")));
    parser.addOption(QCommandLineOption(QStringList() << "show-menubar", i18nc("@info:shell", "Show the menubar, overriding the default setting")));
    parser.addOption(QCommandLineOption(QStringList() << "hide-menubar", i18nc("@info:shell", "Hide the menubar, overriding the default setting")));
    parser.addOption(QCommandLineOption(QStringList() << "show-tabbar", i18nc("@info:shell", "Show the tabbar, overriding the default setting")));
    parser.addOption(QCommandLineOption(QStringList() << "hide-tabbar", i18nc("@info:shell", "Hide the tabbar, overriding the default setting")));
    parser.addOption(QCommandLineOption(QStringList() << "fullscreen", i18nc("@info:shell", "Start Konsole in fullscreen mode")));
    parser.addOption(QCommandLineOption(QStringList() << "notransparency",
                                        i18nc("@info:shell", "Disable transparent backgrounds, even if the system"
                                              " supports them.")));
    parser.addOption(QCommandLineOption(QStringList() << "list-profiles",
                                        i18nc("@info:shell", "List the available profiles")));
    parser.addOption(QCommandLineOption(QStringList() << "list-profile-properties",
                                        i18nc("@info:shell", "List all the profile properties names and their type"
                                              " (for use with -p)")));
    parser.addOption(QCommandLineOption(QStringList() << "p",
                                        i18nc("@info:shell", "Change the value of a profile property."),
                                        QStringLiteral("property=value")));
    parser.addOption(QCommandLineOption(QStringList() << "e",
                                        i18nc("@info:shell", "Command to execute. This option will catch all following"
                                              " arguments, so use it as the last option."),
                                        QStringLiteral("cmd")));
    parser.addPositionalArgument(QStringLiteral("[args]"),
                                 i18nc("@info:shell", "Arguments passed to command"));

}

void fillAboutData(KAboutData& aboutData)
{
    aboutData.setProgramIconName("utilities-terminal");
    aboutData.setOrganizationDomain("kde.org");

    aboutData.addAuthor(i18nc("@info:credit", "Kurt Hindenburg"),
                        i18nc("@info:credit", "General maintainer, bug fixes and general"
                               " improvements"),
                        "kurt.hindenburg@gmail.com");
    aboutData.addAuthor(i18nc("@info:credit", "Robert Knight"),
                        i18nc("@info:credit", "Previous maintainer, ported to KDE4"),
                        "robertknight@gmail.com");
    aboutData.addAuthor(i18nc("@info:credit", "Lars Doelle"),
                        i18nc("@info:credit", "Original author"),
                        "lars.doelle@on-line.de");
    aboutData.addCredit(i18nc("@info:credit", "Jekyll Wu"),
                        i18nc("@info:credit", "Bug fixes and general improvements"),
                        "adaptee@gmail.com");
    aboutData.addCredit(i18nc("@info:credit", "Waldo Bastian"),
                        i18nc("@info:credit", "Bug fixes and general improvements"),
                        "bastian@kde.org");
    aboutData.addCredit(i18nc("@info:credit", "Stephan Binner"),
                        i18nc("@info:credit", "Bug fixes and general improvements"),
                        "binner@kde.org");
    aboutData.addCredit(i18nc("@info:credit", "Thomas Dreibholz"),
                        i18nc("@info:credit", "General improvements"),
                        "dreibh@iem.uni-due.de");
    aboutData.addCredit(i18nc("@info:credit", "Chris Machemer"),
                        i18nc("@info:credit", "Bug fixes"),
                        "machey@ceinetworks.com");
    aboutData.addCredit(i18nc("@info:credit", "Francesco Cecconi"),
                        i18nc("@info:credit", "Bug fixes"),
                        "francesco.cecconi@gmail.com");
    aboutData.addCredit(i18nc("@info:credit", "Stephan Kulow"),
                        i18nc("@info:credit", "Solaris support and history"),
                        "coolo@kde.org");
    aboutData.addCredit(i18nc("@info:credit", "Alexander Neundorf"),
                        i18nc("@info:credit", "Bug fixes and improved startup performance"),
                        "neundorf@kde.org");
    aboutData.addCredit(i18nc("@info:credit", "Peter Silva"),
                        i18nc("@info:credit", "Marking improvements"),
                        "Peter.A.Silva@gmail.com");
    aboutData.addCredit(i18nc("@info:credit", "Lotzi Boloni"),
                        i18nc("@info:credit", "Embedded Konsole\n"
                               "Toolbar and session names"),
                        "boloni@cs.purdue.edu");
    aboutData.addCredit(i18nc("@info:credit", "David Faure"),
                        i18nc("@info:credit", "Embedded Konsole\n"
                               "General improvements"),
                        "faure@kde.org");
    aboutData.addCredit(i18nc("@info:credit", "Antonio Larrosa"),
                        i18nc("@info:credit", "Visual effects"),
                        "larrosa@kde.org");
    aboutData.addCredit(i18nc("@info:credit", "Matthias Ettrich"),
                        i18nc("@info:credit", "Code from the kvt project\n"
                               "General improvements"),
                        "ettrich@kde.org");
    aboutData.addCredit(i18nc("@info:credit", "Warwick Allison"),
                        i18nc("@info:credit", "Schema and text selection improvements"),
                        "warwick@troll.no");
    aboutData.addCredit(i18nc("@info:credit", "Dan Pilone"),
                        i18nc("@info:credit", "SGI port"),
                        "pilone@slac.com");
    aboutData.addCredit(i18nc("@info:credit", "Kevin Street"),
                        i18nc("@info:credit", "FreeBSD port"),
                        "street@iname.com");
    aboutData.addCredit(i18nc("@info:credit", "Sven Fischer"),
                        i18nc("@info:credit", "Bug fixes"),
                        "herpes@kawo2.renditionwth-aachen.de");
    aboutData.addCredit(i18nc("@info:credit", "Dale M. Flaven"),
                        i18nc("@info:credit", "Bug fixes"),
                        "dflaven@netport.com");
    aboutData.addCredit(i18nc("@info:credit", "Martin Jones"),
                        i18nc("@info:credit", "Bug fixes"),
                        "mjones@powerup.com.au");
    aboutData.addCredit(i18nc("@info:credit", "Lars Knoll"),
                        i18nc("@info:credit", "Bug fixes"),
                        "knoll@mpi-hd.mpg.de");
    aboutData.addCredit(i18nc("@info:credit", "Thanks to many others.\n"));
}

void restoreSession(Application& app)
{
    int n = 1;
    while (KMainWindow::canBeRestored(n))
        app.newMainWindow()->restore(n++);
}


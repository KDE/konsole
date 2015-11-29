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
#include <QDir>

// KDE
#include <KAboutData>
#include <KCmdLineArgs>
#include <KLocalizedString>
#include <kdemacros.h>
#include <Kdelibs4ConfigMigrator>
#include <Kdelibs4Migration>

using Konsole::Application;

// fill the KAboutData structure with information about contributors to Konsole.
void fillAboutData(KAboutData& aboutData);

// fill the KCmdLineOptions object with konsole specific options.
void fillCommandLineOptions(KCmdLineOptions& options);

// check and report whether this konsole instance should use a new konsole
// process, or re-use an existing konsole process.
bool shouldUseNewProcess();

// restore sessions saved by KDE.
void restoreSession(Application& app);

// ***
// Entry point into the Konsole terminal application.
// ***
extern "C" int KDE_EXPORT kdemain(int argc, char** argv)
{
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

    KCmdLineArgs::init(argc,
                       argv,
                       about.componentName().toUtf8(),
                       about.componentName().toUtf8(),
                       ki18nc("@title", "Konsole"),
                       about.version().toUtf8(),
                       ki18nc("@title", "Terminal emulator"));

    KCmdLineArgs::addStdCmdLineOptions();  // Qt and KDE options
    KUniqueApplication::addCmdLineOptions(); // KUniqueApplication options
    KCmdLineOptions konsoleOptions; // Konsole options
    fillCommandLineOptions(konsoleOptions);
    KCmdLineArgs::addCmdLineOptions(konsoleOptions);

    KUniqueApplication::StartFlags startFlags;
    if (shouldUseNewProcess())
        startFlags = KUniqueApplication::NonUniqueInstance;

    // create a new application instance if there are no running Konsole
    // instances, otherwise inform the existing Konsole process and exit
    if (!KUniqueApplication::start(startFlags)) {
        exit(0);
    }

    Application app;

    KAboutData::setApplicationData(about);

    restoreSession(app);
    return app.exec();
}
bool shouldUseNewProcess()
{
    // The "unique process" model of konsole is incompatible with some or all
    // Qt/KDE options. When those incompatible options are given, konsole must
    // use new process
    //
    // TODO: make sure the existing list is OK and add more incompatible options.

    // take Qt options into consideration
    const KCmdLineArgs* qtArgs = KCmdLineArgs::parsedArgs("qt");
    QStringList qtProblematicOptions;
    qtProblematicOptions << "session" << "name" << "reverse"
                         << "stylesheet" << "graphicssystem";
#if HAVE_X11
    qtProblematicOptions << "display" << "visual";
#endif
    foreach(const QString& option, qtProblematicOptions) {
        if ( qtArgs->isSet(option.toLocal8Bit()) ) {
            return true;
        }
    }

    // take KDE options into consideration
    const KCmdLineArgs* kdeArgs = KCmdLineArgs::parsedArgs("kde");
    QStringList kdeProblematicOptions;
    kdeProblematicOptions << "config" << "style";
#if HAVE_X11
    kdeProblematicOptions << "waitforwm";
#endif
    foreach(const QString& option, kdeProblematicOptions) {
        if ( kdeArgs->isSet(option.toLocal8Bit()) ) {
            return true;
        }
    }

    const KCmdLineArgs* kUniqueAppArgs = KCmdLineArgs::parsedArgs("kuniqueapp");

    // when user asks konsole to run in foreground through the --nofork option
    // provided by KUniqueApplication, we must use new process. Otherwise, there
    // will be no process for users to wait for finishing.
    const bool shouldRunInForeground = !kUniqueAppArgs->isSet("fork");
    if (shouldRunInForeground) {
        return true;
    }

    const KCmdLineArgs* konsoleArgs = KCmdLineArgs::parsedArgs();

    // if users have explictly requested starting a new process
    if (konsoleArgs->isSet("separate")) {
        return true;
    }

    // the only way to create new tab is to reuse existing Konsole process.
    if (konsoleArgs->isSet("new-tab")) {
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

void fillCommandLineOptions(KCmdLineOptions& options)
{
    options.add("profile <name>",
                ki18nc("@info:shell", "Name of profile to use for new Konsole instance"));
    options.add("fallback-profile",
                ki18nc("@info:shell", "Use the internal FALLBACK profile"));
    options.add("workdir <dir>",
                ki18nc("@info:shell", "Set the initial working directory of the new tab or"
                      " window to 'dir'"));
    options.add("hold");
    options.add("noclose",
                ki18nc("@info:shell", "Do not close the initial session automatically when it"
                      " ends."));
    options.add("new-tab",
                ki18nc("@info:shell", "Create a new tab in an existing window rather than"
                      " creating a new window"));
    options.add("tabs-from-file <file>",
                ki18nc("@info:shell", "Create tabs as specified in given tabs configuration"
                      " file"));
    options.add("background-mode",
                ki18nc("@info:shell", "Start Konsole in the background and bring to the front"
                      " when Ctrl+Shift+F12 (by default) is pressed"));
    options.add("separate", ki18n("Run in a separate process"));
    options.add("show-menubar", ki18nc("@info:shell", "Show the menubar, overriding the default setting"));
    options.add("hide-menubar", ki18nc("@info:shell", "Hide the menubar, overriding the default setting"));
    options.add("show-tabbar", ki18nc("@info:shell", "Show the tabbar, overriding the default setting"));
    options.add("hide-tabbar", ki18nc("@info:shell", "Hide the tabbar, overriding the default setting"));
    options.add("fullscreen", ki18nc("@info:shell", "Start Konsole in fullscreen mode"));
    options.add("notransparency",
                ki18nc("@info:shell", "Disable transparent backgrounds, even if the system"
                      " supports them."));
    options.add("list-profiles", ki18nc("@info:shell", "List the available profiles"));
    options.add("list-profile-properties",
                ki18nc("@info:shell", "List all the profile properties names and their type"
                      " (for use with -p)"));
    options.add("p <property=value>",
                ki18nc("@info:shell", "Change the value of a profile property."));
    options.add("!e <cmd>",
                ki18nc("@info:shell", "Command to execute. This option will catch all following"
                      " arguments, so use it as the last option."));
    options.add("+[args]", ki18nc("@info:shell", "Arguments passed to command"));
    options.add("", ki18nc("@info:shell", "Use --nofork to run in the foreground (helpful"
                          " with the -e option)."));
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
    if (app.isSessionRestored()) {
        int n = 1;
        while (KMainWindow::canBeRestored(n))
            app.newMainWindow()->restore(n++);
    }
}


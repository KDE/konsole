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
#include "KonsoleSettings.h"
#include "ViewManager.h"
#include "ViewContainer.h"

// OS specific
#include <qplatformdefs.h>
#include <QApplication>
#include <QCommandLineParser>
#include <QProxyStyle>
#include <QStandardPaths>
#include <QDir>

// KDE
#include <KAboutData>
#include <KLocalizedString>
#include <KCrash>
#include <Kdelibs4ConfigMigrator>
#include <Kdelibs4Migration>
#include <kdbusservice.h>

using Konsole::Application;

// fill the KAboutData structure with information about contributors to Konsole.
void fillAboutData(KAboutData &aboutData);

// check and report whether this konsole instance should use a new konsole
// process, or re-use an existing konsole process.
bool shouldUseNewProcess(int argc, char *argv[]);

// restore sessions saved by KDE.
void restoreSession(Application &app);

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

// This override resolves following problem: since some qt version if
// XDG_CURRENT_DESKTOP ≠ kde, then pressing and immediately releasing Alt
// key makes focus get stuck in QMenu.
// Upstream report: https://bugreports.qt.io/browse/QTBUG-77355
class MenuStyle : public QProxyStyle {
public:
    int styleHint(const StyleHint stylehint,
                  const QStyleOption *opt,
                  const QWidget *widget,
                  QStyleHintReturn *returnData) const override {
        return (stylehint == QStyle::SH_MenuBar_AltKeyNavigation)
            ? 0 : QProxyStyle::styleHint(stylehint, opt, widget, returnData);
    }
};

// ***
// Entry point into the Konsole terminal application.
// ***
extern "C" int Q_DECL_EXPORT kdemain(int argc, char *argv[])
{
    // enable high dpi support
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);

    // Check if any of the arguments makes it impossible to re-use an existing process.
    // We need to do this manually and before creating a QApplication, because
    // QApplication takes/removes the Qt specific arguments that are incompatible.
    const bool needNewProcess = shouldUseNewProcess(argc, argv);
    if (!needNewProcess) { // We need to avoid crashing
        needToDeleteQApplication = true;
    }

#if defined(Q_OS_LINUX) && (QT_VERSION < QT_VERSION_CHECK(5, 11, 2))
    // Workaround for https://bugreports.qt.io/browse/QTBUG-48344
    // See also https://bugs.kde.org/show_bug.cgi?id=230184
    // The Qt glib event loop doesn't let timers deliver events if there are a
    // lot of other events.
    const QByteArray qtUseGLibOld = qgetenv("QT_NO_GLIB");
    qputenv("QT_NO_GLIB", "1");
#endif

    auto app = new QApplication(argc, argv);
    app->setStyle(new MenuStyle());

#if defined(Q_OS_LINUX) && (QT_VERSION < QT_VERSION_CHECK(5, 11, 2))
    if (qtUseGLibOld.isNull()) {
        qunsetenv("QT_NO_GLIB");
    } else {
        qputenv("QT_NO_GLIB", qtUseGLibOld);
    }
#endif

#if defined(Q_OS_MACOS)
    // this ensures that Ctrl and Meta are not swapped, so CTRL-C and friends
    // will work correctly in the terminal
    app->setAttribute(Qt::AA_MacDontSwapCtrlAndMeta);

    // KDE's menuBar()->isTopLevel() hasn't worked in a while.
    // For now, put menus inside Konsole window; this also make
    // the keyboard shortcut to show menus look reasonable.
    app->setAttribute(Qt::AA_DontUseNativeMenuBar);
#endif

    app->setWindowIcon(QIcon::fromTheme(QStringLiteral("utilities-terminal")));

    KLocalizedString::setApplicationDomain("konsole");

    KAboutData about(QStringLiteral("konsole"),
                     i18nc("@title", "Konsole"),
                     QStringLiteral(KONSOLE_VERSION),
                     i18nc("@title", "Terminal emulator"),
                     KAboutLicense::GPL_V2,
                     i18nc("@info:credit", "(c) 1997-2020, The Konsole Developers"),
                     QString(),
                     QStringLiteral("https://konsole.kde.org/"));
    fillAboutData(about);

    KAboutData::setApplicationData(about);

    KCrash::initialize();

    QSharedPointer<QCommandLineParser> parser(new QCommandLineParser);
    parser->setApplicationDescription(about.shortDescription());
    about.setupCommandLine(parser.data());

    QStringList args = app->arguments();
    QStringList customCommand = Application::getCustomCommand(args);

    Application::populateCommandLineParser(parser.data());

    parser->process(args);
    about.processCommandLine(parser.data());


    /// ! DON'T TOUCH THIS ! ///
    const KDBusService::StartupOption startupOption = Konsole::KonsoleSettings::useSingleInstance() && !needNewProcess ?
        KDBusService::Unique :
        KDBusService::Multiple;
    /// ! DON'T TOUCH THIS ! ///
    // If you need to change something here, add your logic _at the bottom_ of
    // shouldUseNewProcess(), after reading the explanations there for why you
    // probably shouldn't.


    atexit(deleteQApplication);
    // Ensure that we only launch a new instance if we need to
    // If there is already an instance running, we will quit here
    KDBusService dbusService(startupOption | KDBusService::NoExitOnFailure);

    needToDeleteQApplication = false;

    Kdelibs4ConfigMigrator migrate(QStringLiteral("konsole"));
    migrate.setConfigFiles(QStringList() << QStringLiteral("konsolerc")
                                         << QStringLiteral("konsole.notifyrc"));
    migrate.setUiFiles(QStringList() << QStringLiteral("sessionui.rc") << QStringLiteral("partui.rc") << QStringLiteral("konsoleui.rc"));

    if (migrate.migrate()) {
        Kdelibs4Migration dataMigrator;
        const QString sourceBasePath = dataMigrator.saveLocation("data", QStringLiteral("konsole"));
        const QString targetBasePath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/konsole/");
        QString targetFilePath;

        QDir sourceDir(sourceBasePath);
        QDir targetDir(targetBasePath);

        if (sourceDir.exists()) {
            if (!targetDir.exists()) {
                QDir().mkpath(targetBasePath);
            }
            const QStringList fileNames = sourceDir.entryList(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);
            for (const QString &fileName : fileNames) {
                targetFilePath = targetBasePath + fileName;
                if (!QFile::exists(targetFilePath)) {
                    QFile::copy(sourceBasePath + fileName, targetFilePath);
                }
            }
        }
    }

    // If we reach this location, there was no existing copy of Konsole
    // running, so create a new instance.
    Application konsoleApp(parser, customCommand);

    // The activateRequested() signal is emitted when a second instance
    // of Konsole is started.
    QObject::connect(&dbusService, &KDBusService::activateRequested, &konsoleApp,
                     &Application::slotActivateRequested);

    if (app->isSessionRestored()) {
        restoreSession(konsoleApp);
    } else {
        // Do not finish starting Konsole due to:
        // 1. An argument was given to just printed info
        // 2. An invalid situation occurred
        const bool continueStarting = (konsoleApp.newInstance() != 0);
        if (!continueStarting) {
            delete app;
            return 0;
        }
    }

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
    arguments.reserve(argc);
    for (int i = 0; i < argc; i++) {
        arguments.append(QString::fromLocal8Bit(argv[i]));
    }

    // take Qt options into consideration
    QStringList qtProblematicOptions;
    qtProblematicOptions << QStringLiteral("--session")
                         << QStringLiteral("--name")
                         << QStringLiteral("--reverse")
                         << QStringLiteral("--stylesheet")
                         << QStringLiteral("--graphicssystem");
#if HAVE_X11
    qtProblematicOptions << QStringLiteral("--display")
                         << QStringLiteral("--visual");
#endif
    for (const QString &option : qAsConst(qtProblematicOptions)) {
        if (arguments.contains(option)) {
            return true;
        }
    }

    // take KDE options into consideration
    QStringList kdeProblematicOptions;
    kdeProblematicOptions << QStringLiteral("--config")
                          << QStringLiteral("--style");
#if HAVE_X11
    kdeProblematicOptions << QStringLiteral("--waitforwm");
#endif

    for (const QString &option : qAsConst(kdeProblematicOptions)) {
        if (arguments.contains(option)) {
            return true;
        }
    }

    // if users have explictly requested starting a new process
    // Support --nofork to retain argument compatibility with older
    // versions.
    if (arguments.contains(QStringLiteral("--separate"))
        || arguments.contains(QStringLiteral("--nofork"))) {
        return true;
    }

    // the only way to create new tab is to reuse existing Konsole process.
    if (arguments.contains(QStringLiteral("--new-tab"))) {
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

void fillAboutData(KAboutData &aboutData)
{
    aboutData.setOrganizationDomain("kde.org");

    aboutData.addAuthor(i18nc("@info:credit", "Kurt Hindenburg"),
                        i18nc("@info:credit", "General maintainer, bug fixes and general"
                                              " improvements"),
                        QStringLiteral("kurt.hindenburg@gmail.com"));
    aboutData.addAuthor(i18nc("@info:credit", "Robert Knight"),
                        i18nc("@info:credit", "Previous maintainer, ported to KDE4"),
                        QStringLiteral("robertknight@gmail.com"));
    aboutData.addAuthor(i18nc("@info:credit", "Lars Doelle"),
                        i18nc("@info:credit", "Original author"),
                        QStringLiteral("lars.doelle@on-line.de"));
    aboutData.addCredit(i18nc("@info:credit", "Jekyll Wu"),
                        i18nc("@info:credit", "Bug fixes and general improvements"),
                        QStringLiteral("adaptee@gmail.com"));
    aboutData.addCredit(i18nc("@info:credit", "Waldo Bastian"),
                        i18nc("@info:credit", "Bug fixes and general improvements"),
                        QStringLiteral("bastian@kde.org"));
    aboutData.addCredit(i18nc("@info:credit", "Stephan Binner"),
                        i18nc("@info:credit", "Bug fixes and general improvements"),
                        QStringLiteral("binner@kde.org"));
    aboutData.addCredit(i18nc("@info:credit", "Thomas Dreibholz"),
                        i18nc("@info:credit", "General improvements"),
                        QStringLiteral("dreibh@iem.uni-due.de"));
    aboutData.addCredit(i18nc("@info:credit", "Chris Machemer"),
                        i18nc("@info:credit", "Bug fixes"),
                        QStringLiteral("machey@ceinetworks.com"));
    aboutData.addCredit(i18nc("@info:credit", "Francesco Cecconi"),
                        i18nc("@info:credit", "Bug fixes"),
                        QStringLiteral("francesco.cecconi@gmail.com"));
    aboutData.addCredit(i18nc("@info:credit", "Stephan Kulow"),
                        i18nc("@info:credit", "Solaris support and history"),
                        QStringLiteral("coolo@kde.org"));
    aboutData.addCredit(i18nc("@info:credit", "Alexander Neundorf"),
                        i18nc("@info:credit", "Bug fixes and improved startup performance"),
                        QStringLiteral("neundorf@kde.org"));
    aboutData.addCredit(i18nc("@info:credit", "Peter Silva"),
                        i18nc("@info:credit", "Marking improvements"),
                        QStringLiteral("Peter.A.Silva@gmail.com"));
    aboutData.addCredit(i18nc("@info:credit", "Lotzi Boloni"),
                        i18nc("@info:credit", "Embedded Konsole\n"
                                              "Toolbar and session names"),
                        QStringLiteral("boloni@cs.purdue.edu"));
    aboutData.addCredit(i18nc("@info:credit", "David Faure"),
                        i18nc("@info:credit", "Embedded Konsole\n"
                                              "General improvements"),
                        QStringLiteral("faure@kde.org"));
    aboutData.addCredit(i18nc("@info:credit", "Antonio Larrosa"),
                        i18nc("@info:credit", "Visual effects"),
                        QStringLiteral("larrosa@kde.org"));
    aboutData.addCredit(i18nc("@info:credit", "Matthias Ettrich"),
                        i18nc("@info:credit", "Code from the kvt project\n"
                                              "General improvements"),
                        QStringLiteral("ettrich@kde.org"));
    aboutData.addCredit(i18nc("@info:credit", "Warwick Allison"),
                        i18nc("@info:credit", "Schema and text selection improvements"),
                        QStringLiteral("warwick@troll.no"));
    aboutData.addCredit(i18nc("@info:credit", "Dan Pilone"),
                        i18nc("@info:credit", "SGI port"),
                        QStringLiteral("pilone@slac.com"));
    aboutData.addCredit(i18nc("@info:credit", "Kevin Street"),
                        i18nc("@info:credit", "FreeBSD port"),
                        QStringLiteral("street@iname.com"));
    aboutData.addCredit(i18nc("@info:credit", "Sven Fischer"),
                        i18nc("@info:credit", "Bug fixes"),
                        QStringLiteral("herpes@kawo2.renditionwth-aachen.de"));
    aboutData.addCredit(i18nc("@info:credit", "Dale M. Flaven"),
                        i18nc("@info:credit", "Bug fixes"),
                        QStringLiteral("dflaven@netport.com"));
    aboutData.addCredit(i18nc("@info:credit", "Martin Jones"),
                        i18nc("@info:credit", "Bug fixes"),
                        QStringLiteral("mjones@powerup.com.au"));
    aboutData.addCredit(i18nc("@info:credit", "Lars Knoll"),
                        i18nc("@info:credit", "Bug fixes"),
                        QStringLiteral("knoll@mpi-hd.mpg.de"));
    aboutData.addCredit(i18nc("@info:credit", "Thanks to many others.\n"));
}

void restoreSession(Application &app)
{
    int n = 1;

    while (KMainWindow::canBeRestored(n)) {
        auto mainWindow = app.newMainWindow();
        mainWindow->restore(n++);
        mainWindow->viewManager()->toggleActionsBasedOnState();
        mainWindow->show();

        // TODO: HACK without the code below the sessions would be `uninitialized`
        // and the tabs wouldn't display the correct information.
        auto tabbedContainer = qobject_cast<Konsole::TabbedViewContainer*>(mainWindow->centralWidget());
        for(int i = 0; i < tabbedContainer->count(); i++) {
            tabbedContainer->setCurrentIndex(i);
        }
    }
}

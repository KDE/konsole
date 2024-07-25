/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// To time the creation and total launch time (i. e. until window is
// visible/responsive):
// #define PROFILE_STARTUP

// Own
#include "Application.h"
#include "KonsoleSettings.h"
#include "MainWindow.h"
#include "ViewManager.h"
#include "config-konsole.h"
#include "widgets/ViewContainer.h"

// OS specific
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QProxyStyle>
#include <QStandardPaths>
#include <qplatformdefs.h>

// KDE
#include <KAboutData>
#include <KCrash>
#include <KIconTheme>
#include <KLocalizedString>

#if HAVE_DBUS
#include <KDBusService>
#endif

#define HAVE_STYLE_MANAGER __has_include(<KStyleManager>)
#if HAVE_STYLE_MANAGER
#include <KStyleManager>
#endif

using Konsole::Application;

#ifdef PROFILE_STARTUP
#include <QDebug>
#include <QElapsedTimer>
#include <QTimer>
#endif

// fill the KAboutData structure with information about contributors to Konsole.
void fillAboutData(KAboutData &aboutData);

// check and report whether this konsole instance should use a new konsole
// process, or reuse an existing konsole process.
bool shouldUseNewProcess(int argc, char *argv[]);

// restore sessions saved by KDE.
void restoreSession(Application &app);

#if HAVE_DBUS
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
#endif

// This override resolves following problem: since some qt version if
// XDG_CURRENT_DESKTOP ≠ kde, then pressing and immediately releasing Alt
// key makes focus get stuck in QMenu.
// Upstream report: https://bugreports.qt.io/browse/QTBUG-77355
class MenuStyle : public QProxyStyle
{
public:
    MenuStyle(const QString &name)
        : QProxyStyle(name)
    {
    }

    int styleHint(const StyleHint stylehint, const QStyleOption *opt, const QWidget *widget, QStyleHintReturn *returnData) const override
    {
        return (stylehint == QStyle::SH_MenuBar_AltKeyNavigation) ? 0 : QProxyStyle::styleHint(stylehint, opt, widget, returnData);
    }
};

// Used to control migrating config entries.
// Increment when there are new keys to migrate.
static int CurrentConfigVersion = 1;

static void migrateRenamedConfigKeys()
{
    KSharedConfigPtr konsoleConfig = KSharedConfig::openConfig(QStringLiteral("konsolerc"));
    KConfigGroup verGroup = konsoleConfig->group(QStringLiteral("General"));
    const int savedVersion = verGroup.readEntry<int>("ConfigVersion", 0);
    if (savedVersion < CurrentConfigVersion) {
        struct KeyInfo {
            const char *groupName;
            const char *oldKeyName;
            const char *newKeyName;
        };

        static const KeyInfo keys[] = {{"KonsoleWindow", "SaveGeometryOnExit", "RememberWindowSize"}};

        // Migrate renamed config keys
        for (const auto &[group, oldName, newName] : keys) {
            KConfigGroup cg = konsoleConfig->group(QLatin1String(group));
            if (cg.exists() && cg.hasKey(oldName)) {
                const bool value = cg.readEntry(oldName, false);
                cg.deleteEntry(oldName);
                cg.writeEntry(newName, value);
            }
        }

        // With 5.93 KColorSchemeManager from KConfigWidgets, handles the loading
        // and saving of the widget color scheme, and uses "ColorScheme" as the
        // entry name, so clean-up here
        KConfigGroup cg(konsoleConfig, QStringLiteral("UiSettings"));
        const QString schemeName = cg.readEntry("WindowColorScheme");
        cg.deleteEntry("WindowColorScheme");
        cg.writeEntry("ColorScheme", schemeName);

        verGroup.writeEntry("ConfigVersion", CurrentConfigVersion);
        konsoleConfig->sync();
    }
}

// ***
// Entry point into the Konsole terminal application.
// ***
int main(int argc, char *argv[])
{
#ifdef PROFILE_STARTUP
    QElapsedTimer timer;
    timer.start();
#endif

    /**
     * trigger initialisation of proper icon theme
     */
#if KICONTHEMES_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    KIconTheme::initTheme();
#endif

#if HAVE_DBUS
    // Check if any of the arguments makes it impossible to reuse an existing process.
    // We need to do this manually and before creating a QApplication, because
    // QApplication takes/removes the Qt specific arguments that are incompatible.
    const bool needNewProcess = shouldUseNewProcess(argc, argv);
    if (!needNewProcess) { // We need to avoid crashing
        needToDeleteQApplication = true;
    }
#endif

    auto app = new QApplication(argc, argv);

#if HAVE_STYLE_MANAGER
    /**
     * trigger initialisation of proper application style
     */
    KStyleManager::initStyle();
#else
    /**
     * For Windows and macOS: use Breeze if available
     * Of all tested styles that works the best for us
     */
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    QApplication::setStyle(QStringLiteral("breeze"));
#endif
#endif

    // fix the alt key, ensure we keep the current selected style as base
    app->setStyle(new MenuStyle(app->style()->name()));

    migrateRenamedConfigKeys();

    app->setWindowIcon(QIcon::fromTheme(QStringLiteral("utilities-terminal")));

    KLocalizedString::setApplicationDomain("konsole");

    KAboutData about(QStringLiteral("konsole"),
                     i18nc("@title", "Konsole"),
                     QStringLiteral(KONSOLE_VERSION),
                     i18nc("@title", "Terminal emulator"),
                     KAboutLicense::GPL_V2,
                     i18nc("@info:credit", "(c) 1997-2022, The Konsole Developers"),
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

#if HAVE_DBUS
    /// ! DON'T TOUCH THIS ! ///
    const KDBusService::StartupOption startupOption =
        Konsole::KonsoleSettings::useSingleInstance() && !needNewProcess ? KDBusService::Unique : KDBusService::Multiple;
    /// ! DON'T TOUCH THIS ! ///
    // If you need to change something here, add your logic _at the bottom_ of
    // shouldUseNewProcess(), after reading the explanations there for why you
    // probably shouldn't.

    atexit(deleteQApplication);
    // Ensure that we only launch a new instance if we need to
    // If there is already an instance running, we will quit here
    KDBusService dbusService(startupOption | KDBusService::NoExitOnFailure);

    needToDeleteQApplication = false;
#endif

    // If we reach this location, there was no existing copy of Konsole
    // running, so create a new instance.
    Application konsoleApp(parser, customCommand);

#if HAVE_DBUS
    // The activateRequested() signal is emitted when a second instance
    // of Konsole is started.
    QObject::connect(&dbusService, &KDBusService::activateRequested, &konsoleApp, &Application::slotActivateRequested);
#endif

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

#ifdef PROFILE_STARTUP
    qDebug() << "Construction completed in" << timer.elapsed() << "ms";
    QTimer::singleShot(0, [&timer]() {
        qDebug() << "Startup complete in" << timer.elapsed() << "ms";
    });
#endif

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

    // the only way to create new tab is to reuse existing Konsole process.
    const bool wantsNewTab = arguments.contains(QStringLiteral("--new-tab"));

    if (arguments.contains(QLatin1String("--force-reuse"))) {
        return false;
    }

    // take Qt options into consideration
    QStringList qtProblematicOptions;
    qtProblematicOptions << QStringLiteral("--session") << QStringLiteral("--name") << QStringLiteral("--reverse") << QStringLiteral("--stylesheet")
                         << QStringLiteral("--graphicssystem");
#if WITH_X11
    qtProblematicOptions << QStringLiteral("--display") << QStringLiteral("--visual");
#endif
    for (const QString &option : std::as_const(qtProblematicOptions)) {
        if (arguments.contains(option)) {
            if (wantsNewTab) {
                qWarning() << "Argument" << option << "can't be used with --new-tab";
            }

            return true;
        }
    }

    // take KDE options into consideration
    QStringList kdeProblematicOptions;
    kdeProblematicOptions << QStringLiteral("--config") << QStringLiteral("--style");
#if WITH_X11
    kdeProblematicOptions << QStringLiteral("--waitforwm");
#endif

    for (const QString &option : std::as_const(kdeProblematicOptions)) {
        if (arguments.contains(option)) {
            if (wantsNewTab) {
                qWarning() << "Argument" << option << "can't be used with --new-tab";
            }

            return true;
        }
    }

    // if users have explicitly requested starting a new process
    // Support --nofork to retain argument compatibility with older
    // versions.
    if (arguments.contains(QStringLiteral("--separate"))) {
        if (wantsNewTab) {
            qWarning() << "--separate can't be used with --new-tab";
        }
        return true;
    }
    if (arguments.contains(QStringLiteral("--nofork"))) {
        if (wantsNewTab) {
            qWarning() << "--no-fork can't be used with --new-tab";
        }
        return true;
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

    // TODO: find out how to pass environment, so we don't need to do this
    if (wantsNewTab) {
        qWarning() << "--new-tab will not propagate the environment from your current environment, and warnings from Konsole will not be printed";
        return false;
    }

    return hasControllingTTY;
}

void fillAboutData(KAboutData &aboutData)
{
    aboutData.setOrganizationDomain("kde.org");

    aboutData.addAuthor(i18nc("@info:credit", "Kurt Hindenburg"),
                        i18nc("@info:credit",
                              "General maintainer, bug fixes and general"
                              " improvements"),
                        QStringLiteral("kurt.hindenburg@gmail.com"));
    aboutData.addAuthor(i18nc("@info:credit", "Robert Knight"),
                        i18nc("@info:credit", "Previous maintainer, ported to KDE4"),
                        QStringLiteral("robertknight@gmail.com"));
    aboutData.addAuthor(i18nc("@info:credit", "Lars Doelle"), i18nc("@info:credit", "Original author"), QStringLiteral("lars.doelle@on-line.de"));
    aboutData.addCredit(i18nc("@info:credit", "Ahmad Samir"),
                        i18nc("@info:credit", "Major refactoring, bug fixes and major improvements"),
                        QStringLiteral("a.samirh78@gmail.com"));
    aboutData.addCredit(i18nc("@info:credit", "Carlos Alves"),
                        i18nc("@info:credit", "Major refactoring, bug fixes and major improvements"),
                        QStringLiteral("cbc.alves@gmail.com"));
    aboutData.addCredit(i18nc("@info:credit", "Tomaz Canabrava"),
                        i18nc("@info:credit", "Major refactoring, bug fixes and major improvements"),
                        QStringLiteral("tcanabrava@kde.org"));
    aboutData.addCredit(i18nc("@info:credit", "Gustavo Carneiro"),
                        i18nc("@info:credit", "Major refactoring, bug fixes and major improvements"),
                        QStringLiteral("gcarneiroa@hotmail.com"));
    aboutData.addCredit(i18nc("@info:credit", "Edwin Pujols"),
                        i18nc("@info:credit", "Bug fixes and general improvements"),
                        QStringLiteral("edwin.pujols5@outlook.com"));
    aboutData.addCredit(i18nc("@info:credit", "Martin T. H. Sandsmark"),
                        i18nc("@info:credit", "Bug fixes and general improvements"),
                        QStringLiteral("martin.sandsmark@kde.org"));
    aboutData.addCredit(i18nc("@info:credit", "Nate Graham"), i18nc("@info:credit", "Bug fixes and general improvements"), QStringLiteral("nate@kde.org"));
    aboutData.addCredit(i18nc("@info:credit", "Mariusz Glebocki"),
                        i18nc("@info:credit", "Bug fixes and major improvements"),
                        QStringLiteral("mglb@arccos-1.net"));
    aboutData.addCredit(i18nc("@info:credit", "Thomas Surrel"),
                        i18nc("@info:credit", "Bug fixes and general improvements"),
                        QStringLiteral("thomas.surrel@protonmail.com"));
    aboutData.addCredit(i18nc("@info:credit", "Jekyll Wu"), i18nc("@info:credit", "Bug fixes and general improvements"), QStringLiteral("adaptee@gmail.com"));
    aboutData.addCredit(i18nc("@info:credit", "Waldo Bastian"), i18nc("@info:credit", "Bug fixes and general improvements"), QStringLiteral("bastian@kde.org"));
    aboutData.addCredit(i18nc("@info:credit", "Stephan Binner"), i18nc("@info:credit", "Bug fixes and general improvements"), QStringLiteral("binner@kde.org"));
    aboutData.addCredit(i18nc("@info:credit", "Thomas Dreibholz"), i18nc("@info:credit", "General improvements"), QStringLiteral("dreibh@iem.uni-due.de"));
    aboutData.addCredit(i18nc("@info:credit", "Chris Machemer"), i18nc("@info:credit", "Bug fixes"), QStringLiteral("machey@ceinetworks.com"));
    aboutData.addCredit(i18nc("@info:credit", "Francesco Cecconi"), i18nc("@info:credit", "Bug fixes"), QStringLiteral("francesco.cecconi@gmail.com"));
    aboutData.addCredit(i18nc("@info:credit", "Stephan Kulow"), i18nc("@info:credit", "Solaris support and history"), QStringLiteral("coolo@kde.org"));
    aboutData.addCredit(i18nc("@info:credit", "Alexander Neundorf"),
                        i18nc("@info:credit", "Bug fixes and improved startup performance"),
                        QStringLiteral("neundorf@kde.org"));
    aboutData.addCredit(i18nc("@info:credit", "Peter Silva"), i18nc("@info:credit", "Marking improvements"), QStringLiteral("Peter.A.Silva@gmail.com"));
    aboutData.addCredit(i18nc("@info:credit", "Lotzi Boloni"),
                        i18nc("@info:credit",
                              "Embedded Konsole\n"
                              "Toolbar and session names"),
                        QStringLiteral("boloni@cs.purdue.edu"));
    aboutData.addCredit(i18nc("@info:credit", "David Faure"),
                        i18nc("@info:credit",
                              "Embedded Konsole\n"
                              "General improvements"),
                        QStringLiteral("faure@kde.org"));
    aboutData.addCredit(i18nc("@info:credit", "Antonio Larrosa"), i18nc("@info:credit", "Visual effects"), QStringLiteral("larrosa@kde.org"));
    aboutData.addCredit(i18nc("@info:credit", "Matthias Ettrich"),
                        i18nc("@info:credit",
                              "Code from the kvt project\n"
                              "General improvements"),
                        QStringLiteral("ettrich@kde.org"));
    aboutData.addCredit(i18nc("@info:credit", "Warwick Allison"),
                        i18nc("@info:credit", "Schema and text selection improvements"),
                        QStringLiteral("warwick@troll.no"));
    aboutData.addCredit(i18nc("@info:credit", "Dan Pilone"), i18nc("@info:credit", "SGI port"), QStringLiteral("pilone@slac.com"));
    aboutData.addCredit(i18nc("@info:credit", "Kevin Street"), i18nc("@info:credit", "FreeBSD port"), QStringLiteral("street@iname.com"));
    aboutData.addCredit(i18nc("@info:credit", "Sven Fischer"), i18nc("@info:credit", "Bug fixes"), QStringLiteral("herpes@kawo2.renditionwth-aachen.de"));
    aboutData.addCredit(i18nc("@info:credit", "Dale M. Flaven"), i18nc("@info:credit", "Bug fixes"), QStringLiteral("dflaven@netport.com"));
    aboutData.addCredit(i18nc("@info:credit", "Martin Jones"), i18nc("@info:credit", "Bug fixes"), QStringLiteral("mjones@powerup.com.au"));
    aboutData.addCredit(i18nc("@info:credit", "Lars Knoll"), i18nc("@info:credit", "Bug fixes"), QStringLiteral("knoll@mpi-hd.mpg.de"));
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
        auto tabbedContainer = qobject_cast<Konsole::TabbedViewContainer *>(mainWindow->centralWidget());
        for (int i = 0; i < tabbedContainer->count(); i++) {
            tabbedContainer->setCurrentIndex(i);
        }
    }
}

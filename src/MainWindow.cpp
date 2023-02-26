/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "MainWindow.h"
#include "AppColorSchemeChooser.h"

// Qt
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QScreen>
#include <QWindow>

// KDE
#include <KAcceleratorManager>
#include <KActionCollection>
#include <KActionMenu>
#include <KCrash>
#include <KIconUtils>
#include <KLocalizedString>
#include <KShortcutsDialog>
#include <KWindowEffects>

#include <KMessageBox>
#include <KNotifyConfigWidget>
#include <KStandardAction>
#include <KStandardGuiItem>
#include <KWindowSystem>
#include <KXMLGUIFactory>

#include <kio_version.h>
#include <kwidgetsaddons_version.h>
#include <kwindowsystem_version.h>

// Konsole
#include "BookmarkHandler.h"
#include "KonsoleSettings.h"
#include "ViewManager.h"
#include "WindowSystemInfo.h"

#include "profile/ProfileList.h"
#include "profile/ProfileManager.h"

#include "session/Session.h"
#include "session/SessionController.h"
#include "session/SessionManager.h"

#include "settings/ConfigurationDialog.h"
#include "settings/GeneralSettings.h"
#include "settings/ProfileSettings.h"
#include "settings/TabBarSettings.h"
#include "settings/TemporaryFilesSettings.h"
#include "settings/ThumbnailsSettings.h"

#include "terminalDisplay/TerminalDisplay.h"
#include "widgets/ViewContainer.h"

#include <konsoledebug.h>

using namespace Konsole;

MainWindow::MainWindow()
    : KXmlGuiWindow()
    , _viewManager(nullptr)
    , _bookmarkHandler(nullptr)
    , _toggleMenuBarAction(nullptr)
    , _newTabMenuAction(nullptr)
    , _pluggedController(nullptr)
{
    // Set the WA_NativeWindow attribute to force the creation of the QWindow.
    // Without this QWidget::windowHandle() returns 0.
    // See https://phabricator.kde.org/D23108
    setAttribute(Qt::WA_NativeWindow);

    updateUseTransparency();

    // create actions for menus
    setupActions();

    // create view manager
    _viewManager = new ViewManager(this, actionCollection());
    connect(_viewManager, &Konsole::ViewManager::empty, this, &QWidget::close);
    connect(_viewManager, &Konsole::ViewManager::activeViewChanged, this, &Konsole::MainWindow::activeViewChanged);
    connect(_viewManager, &Konsole::ViewManager::unplugController, this, &Konsole::MainWindow::disconnectController);
    connect(_viewManager, &Konsole::ViewManager::viewPropertiesChanged, bookmarkHandler(), &Konsole::BookmarkHandler::setViews);
    connect(_viewManager, &Konsole::ViewManager::blurSettingChanged, this, &Konsole::MainWindow::setBlur);

    connect(_viewManager, &Konsole::ViewManager::updateWindowIcon, this, &Konsole::MainWindow::updateWindowIcon);
    connect(_viewManager, &Konsole::ViewManager::newViewWithProfileRequest, this, &Konsole::MainWindow::newFromProfile);
    connect(_viewManager, &Konsole::ViewManager::newViewRequest, this, &Konsole::MainWindow::newTab);
    connect(_viewManager, &Konsole::ViewManager::terminalsDetached, this, &Konsole::MainWindow::terminalsDetached);
    connect(_viewManager, &Konsole::ViewManager::activationRequest, this, &Konsole::MainWindow::activationRequest);

    setCentralWidget(_viewManager->widget());

    // disable automatically generated accelerators in top-level
    // menu items - to avoid conflicting with Alt+[Letter] shortcuts
    // in terminal applications
    KAcceleratorManager::setNoAccel(menuBar());

    constexpr KXmlGuiWindow::StandardWindowOptions guiOpts = ToolBar | Keys | Save | Create;
    const QString xmlFile = componentName() + QLatin1String("ui.rc"); // Typically "konsoleui.rc"
    // The "Create" flag will make it call createGUI()
    setupGUI(guiOpts, xmlFile);

    // remember the original menu accelerators for later use
    rememberMenuAccelerators();

    // replace standard shortcuts which cannot be used in a terminal
    // emulator (as they are reserved for use by terminal applications)
    correctStandardShortcuts();

    setProfileList(new ProfileList(true, this));

    // this must come at the end
    applyKonsoleSettings();
    connect(KonsoleSettings::self(), &Konsole::KonsoleSettings::configChanged, this, &Konsole::MainWindow::applyKonsoleSettings);

    KCrash::initialize();
}

// Convenience function to get a space-separated list of all connected screens, copied from KWindowConfig
static QString allConnectedScreens()
{
    QStringList names;
    const auto screens = QGuiApplication::screens();
    names.reserve(screens.length());
    for (auto screen : screens) {
#ifdef Q_OS_WIN
        // QScreen::name() returns garbage on Windows; see https://bugreports.qt.io/browse/QTBUG-74317
        // So we use the screens' serial numbers to identify them instead
        names << screen->serialNumber();
#else
        names << screen->name();
#endif
    }
    return names.join(QLatin1Char(' '));
}

// Convenience function to get an appropriate config file key under which to
// save window size, position, or maximization information.
// Copied from KWindowConfig before https://invent.kde.org/frameworks/kconfig/-/merge_requests/184
// changed the key format. TODO: use the public function in KWindowConfig for
// this once it exists.
static QString configFileStringV1(const QScreen *screen, const QString &key)
{
    // We include resolution data to also save data on a per-resolution basis
    const QString returnString =
        QStringLiteral("%1 %2 %3x%4 %5")
            .arg(allConnectedScreens(), key, QString::number(screen->geometry().width()), QString::number(screen->geometry().height()), screen->name());
    return returnString;
}

// Convenience function to get an appropriate config file key under which to
// save window size, position, or maximization information.
// Copied from KWindowConfig as of https://invent.kde.org/frameworks/kconfig/-/merge_requests/184
// TODO: use the public function in KWindowConfig for this once it exists.
static QString configFileStringV2(const QScreen *screen, const QString &key)
{
    Q_UNUSED(screen);
    QString returnString;
    const int numberOfScreens = QGuiApplication::screens().length();

    if (numberOfScreens == 1) {
        // For single-screen setups, we save data on a per-resolution basis.
        const QRect screenGeometry = QGuiApplication::primaryScreen()->geometry();
        returnString = QStringLiteral("%1x%2 screen: %3").arg(QString::number(screenGeometry.width()), QString::number(screenGeometry.height()), key);
    } else {
        // For multi-screen setups, we save data based on the number of screens.
        // Distinguishing individual screens based on their names is unreliable
        // due to name strings being inherently volatile.
        returnString = QStringLiteral("%1 screens: %2").arg(QString::number(numberOfScreens), key);
    }
    return returnString;
}

bool MainWindow::wasWindowGeometrySaved() const
{
    KSharedConfigPtr konsoleConfig = KSharedConfig::openConfig(QStringLiteral("konsolerc"));
    KConfigGroup cg = konsoleConfig->group(QStringLiteral("MainWindow"));
    if (!cg.exists()) { // First run, no existing konsolerc?
        return false;
    }

    return cg.hasKey(configFileStringV2(screen(), QStringLiteral("Width"))) || cg.hasKey(configFileStringV2(screen(), QStringLiteral("Height")))
        || cg.hasKey(configFileStringV2(screen(), QStringLiteral("XPosition"))) || cg.hasKey(configFileStringV2(screen(), QStringLiteral("YPosition")))
        || cg.hasKey(configFileStringV1(screen(), QStringLiteral("Width"))) || cg.hasKey(configFileStringV1(screen(), QStringLiteral("Height")))
        || cg.hasKey(configFileStringV1(screen(), QStringLiteral("XPosition"))) || cg.hasKey(configFileStringV1(screen(), QStringLiteral("YPosition")));
}

void MainWindow::updateUseTransparency()
{
    if (!WindowSystemInfo::HAVE_TRANSPARENCY) {
        return;
    }

    bool useTranslucency = KWindowSystem::compositingActive();

    setAttribute(Qt::WA_TranslucentBackground, useTranslucency);
    setAttribute(Qt::WA_NoSystemBackground, false);
    WindowSystemInfo::HAVE_TRANSPARENCY = useTranslucency;
}

void MainWindow::activationRequest(const QString &xdgActivationToken)
{
    KWindowSystem::setCurrentXdgActivationToken(xdgActivationToken);
    KWindowSystem::forceActiveWindow(winId());
}

void MainWindow::rememberMenuAccelerators()
{
    const QList<QAction *> actions = menuBar()->actions();
    for (QAction *menuItem : actions) {
        QString itemText = menuItem->text();
        menuItem->setData(itemText);
    }
}

// remove accelerators for standard menu items (eg. &File, &View, &Edit)
// etc. which are defined in kdelibs/kdeui/xmlgui/ui_standards.rc, again,
// to avoid conflicting with Alt+[Letter] terminal shortcuts
//
// TODO - Modify XMLGUI so that it allows the text for standard actions
// defined in ui_standards.rc to be re-defined in the local application
// XMLGUI file (konsoleui.rc in this case) - the text for standard items
// can then be redefined there to exclude the standard accelerators
void MainWindow::removeMenuAccelerators()
{
    const QList<QAction *> actions = menuBar()->actions();
    for (QAction *menuItem : actions) {
        menuItem->setText(menuItem->text().replace(QLatin1Char('&'), QString()));
    }
}

void MainWindow::restoreMenuAccelerators()
{
    const QList<QAction *> actions = menuBar()->actions();
    for (QAction *menuItem : actions) {
        QString itemText = menuItem->data().toString();
        menuItem->setText(itemText);
    }
}

void MainWindow::correctStandardShortcuts()
{
    // replace F1 shortcut for help contents
    QAction *helpAction = actionCollection()->action(QStringLiteral("help_contents"));
    if (helpAction != nullptr) {
        actionCollection()->setDefaultShortcut(helpAction, QKeySequence());
    }
}

ViewManager *MainWindow::viewManager() const
{
    return _viewManager;
}

void MainWindow::disconnectController(SessionController *controller)
{
    disconnect(controller, &Konsole::SessionController::titleChanged, this, &Konsole::MainWindow::activeViewTitleChanged);
    disconnect(controller, &Konsole::SessionController::rawTitleChanged, this, &Konsole::MainWindow::updateWindowCaption);
    disconnect(controller, &Konsole::SessionController::iconChanged, this, &Konsole::MainWindow::updateWindowIcon);

    if (auto view = controller->view()) {
        view->removeEventFilter(this);
    }

    // KXmlGuiFactory::removeClient() will try to access actions associated
    // with the controller internally, which may not be valid after the controller
    // itself is no longer valid (after the associated session and or view have
    // been destroyed)
    if (controller->isValid()) {
        guiFactory()->removeClient(controller);
    }

    if (_pluggedController == controller) {
        _pluggedController = nullptr;
    }
}

void MainWindow::activeViewChanged(SessionController *controller)
{
    if (!SessionManager::instance()->sessionProfile(controller->session())) {
        return;
    }
    // associate bookmark menu with current session
    bookmarkHandler()->setActiveView(controller);
    disconnect(bookmarkHandler(), &Konsole::BookmarkHandler::openUrl, nullptr, nullptr);
    connect(bookmarkHandler(), &Konsole::BookmarkHandler::openUrl, controller, &Konsole::SessionController::openUrl);

    if (!_pluggedController.isNull()) {
        disconnectController(_pluggedController);
    }

    Q_ASSERT(controller);
    _pluggedController = controller;
    _pluggedController->view()->installEventFilter(this);

    setBlur(ViewManager::profileHasBlurEnabled(SessionManager::instance()->sessionProfile(_pluggedController->session())));

    // listen for title changes from the current session
    connect(controller, &Konsole::SessionController::titleChanged, this, &Konsole::MainWindow::activeViewTitleChanged);
    connect(controller, &Konsole::SessionController::rawTitleChanged, this, &Konsole::MainWindow::updateWindowCaption);
    connect(controller, &Konsole::SessionController::iconChanged, this, &Konsole::MainWindow::updateWindowIcon);

    controller->setShowMenuAction(_toggleMenuBarAction);
    guiFactory()->addClient(controller);

    // update session title to match newly activated session
    activeViewTitleChanged(controller);

    // Update window icon to newly activated session's icon
    updateWindowIcon();

    for (IKonsolePlugin *plugin : _plugins) {
        plugin->activeViewChanged(controller, this);
    }
}

void MainWindow::activeViewTitleChanged(ViewProperties *properties)
{
    Q_UNUSED(properties)
    updateWindowCaption();
}

void MainWindow::updateWindowCaption()
{
    if (_pluggedController.isNull()) {
        return;
    }

    const QString &title = _pluggedController->title();
    const QString &userTitle = _pluggedController->userTitle();

    // use tab title as caption by default
    QString caption = title;

    // use window title as caption when this setting is enabled
    // if the userTitle is empty, use a blank space (using an empty string
    // removes the dash — before the application name; leaving the dash
    // looks better)
    if (KonsoleSettings::showWindowTitleOnTitleBar()) {
        !userTitle.isEmpty() ? caption = userTitle : caption = QStringLiteral(" ");
    }

    setCaption(caption);
}

void MainWindow::updateWindowIcon()
{
    if ((!_pluggedController.isNull()) && !_pluggedController->icon().isNull()) {
        setWindowIcon(_pluggedController->icon());
    }
}

void MainWindow::setupActions()
{
    KActionCollection *collection = actionCollection();

    // File Menu
    _newTabMenuAction = new KActionMenu(QIcon::fromTheme(QStringLiteral("tab-new")), i18nc("@action:inmenu", "&New Tab"), collection);
    collection->setDefaultShortcut(_newTabMenuAction, Konsole::ACCEL | Qt::Key_T);
    collection->setShortcutsConfigurable(_newTabMenuAction, true);
    _newTabMenuAction->setAutoRepeat(false);
    connect(_newTabMenuAction, &KActionMenu::triggered, this, &MainWindow::newTab);
    collection->addAction(QStringLiteral("new-tab"), _newTabMenuAction);
    collection->setShortcutsConfigurable(_newTabMenuAction, true);

    QAction *menuAction = collection->addAction(QStringLiteral("clone-tab"));
    menuAction->setIcon(QIcon::fromTheme(QStringLiteral("tab-duplicate")));
    menuAction->setText(i18nc("@action:inmenu", "&Clone Tab"));
    collection->setDefaultShortcut(menuAction, QKeySequence());
    menuAction->setAutoRepeat(false);
    connect(menuAction, &QAction::triggered, this, &Konsole::MainWindow::cloneTab);

    menuAction = collection->addAction(QStringLiteral("new-window"));
    menuAction->setIcon(QIcon::fromTheme(QStringLiteral("window-new")));
    menuAction->setText(i18nc("@action:inmenu", "New &Window"));
    collection->setDefaultShortcut(menuAction, Konsole::ACCEL | Qt::Key_N);
    menuAction->setAutoRepeat(false);
    connect(menuAction, &QAction::triggered, this, &Konsole::MainWindow::newWindow);

    menuAction = collection->addAction(QStringLiteral("close-window"));
    menuAction->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));
    menuAction->setText(i18nc("@action:inmenu", "Close Window"));
    collection->setDefaultShortcut(menuAction, Konsole::ACCEL | Qt::Key_Q);
    connect(menuAction, &QAction::triggered, this, &Konsole::MainWindow::close);

    // Bookmark Menu
    KActionMenu *bookmarkMenu = new KActionMenu(i18nc("@title:menu", "&Bookmarks"), collection);
    bookmarkMenu->setPopupMode(QToolButton::InstantPopup); // Don't require press+hold
    _bookmarkHandler = new BookmarkHandler(collection, bookmarkMenu->menu(), true, this);
    collection->addAction(QStringLiteral("bookmark"), bookmarkMenu);
    connect(_bookmarkHandler, &Konsole::BookmarkHandler::openUrls, this, &Konsole::MainWindow::openUrls);

    // Settings Menu
    _toggleMenuBarAction = KStandardAction::showMenubar(menuBar(), &QMenuBar::setVisible, collection);
    collection->setDefaultShortcut(_toggleMenuBarAction, Konsole::ACCEL | Qt::Key_M);
    // Set up themes
    actionCollection()->addAction(QStringLiteral("window-colorscheme-menu"), new AppColorSchemeChooser(actionCollection()));

    // Full Screen
    menuAction = KStandardAction::fullScreen(this, &MainWindow::viewFullScreen, this, collection);
    collection->setDefaultShortcut(menuAction, Qt::Key_F11);

    KStandardAction::configureNotifications(this, &MainWindow::configureNotifications, collection);
    KStandardAction::keyBindings(this, &MainWindow::showShortcutsDialog, collection);
    KStandardAction::preferences(this, &MainWindow::showSettingsDialog, collection);

    menuAction = collection->addAction(QStringLiteral("manage-profiles"));
    menuAction->setText(i18nc("@action:inmenu", "Manage Profiles..."));
    menuAction->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
    connect(menuAction, &QAction::triggered, this, &Konsole::MainWindow::showManageProfilesDialog);

    // Set up an shortcut-only action for activating menu bar.
    menuAction = collection->addAction(QStringLiteral("activate-menu"));
    menuAction->setText(i18nc("@item", "Activate Menu"));
    collection->setDefaultShortcut(menuAction, Konsole::ACCEL | Qt::Key_F10);
    connect(menuAction, &QAction::triggered, this, &Konsole::MainWindow::activateMenuBar);

    auto action = collection->addAction(QStringLiteral("save-layout"));
    action->setEnabled(true);
    action->setText(i18nc("@action:inmenu", "Save Tab Layout..."));
    connect(action, &QAction::triggered, this, [this]() {
        if (viewManager()) {
            viewManager()->saveLayoutFile();
        }
    });

    action = collection->addAction(QStringLiteral("load-layout"));
    action->setEnabled(true);
    action->setText(i18nc("@action:inmenu", "Load Tab Layout..."));
    connect(action, &QAction::triggered, this, [this]() {
        if (viewManager()) {
            viewManager()->loadLayoutFile();
        }
    });
}

void MainWindow::viewFullScreen(bool fullScreen)
{
    if (fullScreen) {
        setWindowState(windowState() | Qt::WindowFullScreen);
    } else {
        setWindowState(windowState() & ~Qt::WindowFullScreen);
    }
}

void MainWindow::applyMainWindowSettings(const KConfigGroup &config)
{
    KMainWindow::applyMainWindowSettings(config);

    // Override the menubar state from the config file
    if (_windowArgsMenuBarVisible.enabled) {
        menuBar()->setVisible(_windowArgsMenuBarVisible.showMenuBar);
    }

    _toggleMenuBarAction->setChecked(menuBar()->isVisibleTo(this));
}

BookmarkHandler *MainWindow::bookmarkHandler() const
{
    return _bookmarkHandler;
}

void MainWindow::setProfileList(ProfileList *list)
{
    profileListChanged(list->actions());

    connect(list, &Konsole::ProfileList::profileSelected, this, &MainWindow::newFromProfile);
    connect(list, &Konsole::ProfileList::actionsChanged, this, &Konsole::MainWindow::profileListChanged);
}

void MainWindow::profileListChanged(const QList<QAction *> &sessionActions)
{
    // Update the 'New Tab' KActionMenu
    _newTabMenuAction->menu()->clear();
    for (QAction *sessionAction : sessionActions) {
        _newTabMenuAction->menu()->addAction(sessionAction);

        auto setActionFontBold = [sessionAction](bool isBold) {
            QFont actionFont = sessionAction->font();
            actionFont.setBold(isBold);
            sessionAction->setFont(actionFont);
        };

        Profile::Ptr profile = ProfileManager::instance()->defaultProfile();
        if (profile && profile->name() == sessionAction->text().remove(QLatin1Char('&'))) {
            QIcon icon = KIconUtils::addOverlay(QIcon::fromTheme(profile->icon()), QIcon::fromTheme(QStringLiteral("emblem-favorite")), Qt::BottomRightCorner);
            sessionAction->setIcon(icon);
            setActionFontBold(true);
        } else {
            setActionFontBold(false);
        }
    }
}

QString MainWindow::activeSessionDir() const
{
    if (!_pluggedController.isNull()) {
        if (Session *session = _pluggedController->session()) {
            // For new tabs to get the correct working directory,
            // force the updating of the currentWorkingDirectory.
            session->getDynamicTitle();
        }
        return _pluggedController->currentDir();
    }
    return QString();
}

void MainWindow::openUrls(const QList<QUrl> &urls)
{
    Profile::Ptr defaultProfile = ProfileManager::instance()->defaultProfile();

    for (const auto &url : urls) {
        if (url.isLocalFile()) {
            createSession(defaultProfile, url.path());
        } else if (url.scheme() == QLatin1String("ssh")) {
            createSSHSession(defaultProfile, url);
        }
    }
}

void MainWindow::newTab()
{
    Profile::Ptr defaultProfile = ProfileManager::instance()->defaultProfile();
    createSession(defaultProfile, activeSessionDir());
}

void MainWindow::addPlugin(IKonsolePlugin *plugin)
{
    Q_ASSERT(std::find(_plugins.cbegin(), _plugins.cend(), plugin) == _plugins.cend());
    _plugins.push_back(plugin);
}

void MainWindow::cloneTab()
{
    Q_ASSERT(_pluggedController);

    Session *session = _pluggedController->session();
    Profile::Ptr profile = SessionManager::instance()->sessionProfile(session);
    if (profile) {
        createSession(profile, activeSessionDir());
    } else {
        // something must be wrong: every session should be associated with profile
        Q_ASSERT(false);
        newTab();
    }
}

Session *MainWindow::createSession(Profile::Ptr profile, const QString &directory)
{
    if (!profile) {
        profile = ProfileManager::instance()->defaultProfile();
    }

    const QString newSessionDirectory = profile->startInCurrentSessionDir() ? directory : QString();
    Session *session = _viewManager->createSession(profile, newSessionDirectory);

    // create view before starting the session process so that the session
    // doesn't suffer a change in terminal size right after the session
    // starts.  Some applications such as GNU Screen and Midnight Commander
    // don't like this happening
    auto newView = _viewManager->createView(session);
    _viewManager->activeContainer()->addView(newView);

    return session;
}

Session *MainWindow::createSSHSession(Profile::Ptr profile, const QUrl &url)
{
    if (!profile) {
        profile = ProfileManager::instance()->defaultProfile();
    }

    Session *session = SessionManager::instance()->createSession(profile);

    QString sshCommand = QStringLiteral("ssh ");
    if (url.port() > -1) {
        sshCommand += QStringLiteral("-p %1 ").arg(url.port());
    }
    if (!url.userName().isEmpty()) {
        sshCommand += (url.userName() + QLatin1Char('@'));
    }
    if (!url.host().isEmpty()) {
        sshCommand += url.host();
    }

    session->sendTextToTerminal(sshCommand, QLatin1Char('\r'));

    // create view before starting the session process so that the session
    // doesn't suffer a change in terminal size right after the session
    // starts.  some applications such as GNU Screen and Midnight Commander
    // don't like this happening
    auto newView = _viewManager->createView(session);
    _viewManager->activeContainer()->addView(newView);
    return session;
}

void MainWindow::setFocus()
{
    _viewManager->activeView()->setFocus();
}

void MainWindow::newWindow()
{
    Profile::Ptr defaultProfile = ProfileManager::instance()->defaultProfile();
    Q_EMIT newWindowRequest(defaultProfile, activeSessionDir());
}

bool MainWindow::queryClose()
{
    // Do not ask for confirmation during log out and power off
    // TODO: rework the dealing of this case to make it has its own confirmation
    // dialog.
    if (qApp->isSavingSession()) {
        return true;
    }

    // Check what processes are running, excluding the shell
    QStringList processesRunning;
    // Need to make a local copy so the begin() and end() point to the same QList
    const QList<Session *> sessionList = _viewManager->sessions();
    const QSet<Session *> uniqueSessions(sessionList.begin(), sessionList.end());

    for (Session *session : uniqueSessions) {
        if ((session == nullptr) || !session->isForegroundProcessActive()) {
            continue;
        }

        const QString defaultProc = session->program().split(QLatin1Char('/')).last();
        const QString currentProc = session->foregroundProcessName().split(QLatin1Char('/')).last();

        if (currentProc.isEmpty()) {
            continue;
        }

        if (defaultProc != currentProc) {
            processesRunning.append(currentProc);
        }
    }

    // Get number of open tabs
    const int openTabs = _viewManager->viewProperties().count();

    // If no processes running (except the shell) and no extra tabs, just close
    if (processesRunning.count() == 0 && openTabs < 2) {
        return true;
    }

    // NOTE: Some, if not all, of the below KWindowSystem calls are only
    //       implemented under x11 (KDE4.8 kdelibs/kdeui/windowmanagement).

    // make sure the window is shown on current desktop and is not minimized
    KWindowSystem::setOnDesktop(winId(), KWindowSystem::currentDesktop());
    int result;

    if (!processesRunning.isEmpty()) {
        if (openTabs == 1) {
#if KWIDGETSADDONS_VERSION >= QT_VERSION_CHECK(5, 100, 0)
            result = KMessageBox::warningTwoActionsList(this,
#else
            result = KMessageBox::warningYesNoList(this,
#endif
                                                        i18ncp("@info",
                                                               "There is a process running in this window. "
                                                               "Do you still want to quit?",
                                                               "There are %1 processes running in this window. "
                                                               "Do you still want to quit?",
                                                               processesRunning.count()),
                                                        processesRunning,
                                                        i18nc("@title", "Confirm Close"),
                                                        KGuiItem(i18nc("@action:button", "Close &Window"), QStringLiteral("window-close")),
                                                        KStandardGuiItem::cancel(),
                                                        // don't ask again name is wrong but I can't update.
                                                        // this is not about tabs anymore. it's about empty tabs *or* splits.
                                                        QStringLiteral("CloseAllTabs"));
#if KWIDGETSADDONS_VERSION >= QT_VERSION_CHECK(5, 100, 0)
            if (result == KMessageBox::SecondaryAction) // SecondaryAction is equal to cancel closing
#else
            if (result == KMessageBox::No) // No is equal to cancel closing
#endif
                result = KMessageBox::Cancel;
        } else {
#if KWIDGETSADDONS_VERSION >= QT_VERSION_CHECK(5, 100, 0)
            result = KMessageBox::warningTwoActionsCancelList(this,
#else
            result = KMessageBox::warningYesNoCancelList(this,
#endif
                                                              i18ncp("@info",
                                                                     "There is a process running in this window. "
                                                                     "Do you still want to quit?",
                                                                     "There are %1 processes running in this window. "
                                                                     "Do you still want to quit?",
                                                                     processesRunning.count()),
                                                              processesRunning,
                                                              i18nc("@title", "Confirm Close"),
                                                              KGuiItem(i18nc("@action:button", "Close &Window"), QStringLiteral("window-close")),
                                                              KGuiItem(i18nc("@action:button", "Close Current &Tab"), QStringLiteral("tab-close")),
                                                              KStandardGuiItem::cancel(),
                                                              // don't ask again name is wrong but I can't update.
                                                              // this is not about tabs anymore. it's about empty tabs *or* splits.
                                                              QStringLiteral("CloseAllTabs"));
        }
    } else {
#if KWIDGETSADDONS_VERSION >= QT_VERSION_CHECK(5, 100, 0)
        result = KMessageBox::warningTwoActionsCancel(this,
#else
        result = KMessageBox::warningYesNoCancel(this,
#endif
                                                      i18nc("@info",
                                                            "There are %1 open terminals in this window. "
                                                            "Do you still want to quit?",
                                                            openTabs),
                                                      i18nc("@title", "Confirm Close"),
                                                      KGuiItem(i18nc("@action:button", "Close &Window"), QStringLiteral("window-close")),
                                                      KGuiItem(i18nc("@action:button", "Close Current &Tab"), QStringLiteral("tab-close")),
                                                      KStandardGuiItem::cancel(),
                                                      // don't ask again name is wrong but I can't update.
                                                      // this is not about tabs anymore. it's about empty tabs *or* splits.
                                                      QStringLiteral("CloseAllEmptyTabs"));
    }

    switch (result) {
#if KWIDGETSADDONS_VERSION >= QT_VERSION_CHECK(5, 100, 0)
    case KMessageBox::PrimaryAction:
#else
    case KMessageBox::Yes:
#endif
        return true;
#if KWIDGETSADDONS_VERSION >= QT_VERSION_CHECK(5, 100, 0)
    case KMessageBox::SecondaryAction:
#else
    case KMessageBox::No:
#endif
        if ((!_pluggedController.isNull()) && (!_pluggedController->session().isNull())) {
            if (!(_pluggedController->session()->closeInNormalWay())) {
                if (_pluggedController->confirmForceClose()) {
                    _pluggedController->session()->closeInForceWay();
                }
            }
        }
        return false;
    case KMessageBox::Cancel:
        return false;
    }

    return true;
}

void MainWindow::saveProperties(KConfigGroup &group)
{
    _viewManager->saveSessions(group);
}

void MainWindow::readProperties(const KConfigGroup &group)
{
    _viewManager->restoreSessions(group);
}

void MainWindow::saveGlobalProperties(KConfig *config)
{
    SessionManager::instance()->saveSessions(config);
}

void MainWindow::readGlobalProperties(KConfig *config)
{
    SessionManager::instance()->restoreSessions(config);
}

void MainWindow::syncActiveShortcuts(KActionCollection *dest, const KActionCollection *source)
{
    const QList<QAction *> actionsList = source->actions();
    for (QAction *qAction : actionsList) {
        if (QAction *destQAction = dest->action(qAction->objectName())) {
            destQAction->setShortcut(qAction->shortcut());
        }
    }
}

void MainWindow::showShortcutsDialog()
{
    KShortcutsDialog dialog(KShortcutsEditor::AllActions, KShortcutsEditor::LetterShortcutsDisallowed, this);

    // add actions from this window and the current session controller
    const QList<KXMLGUIClient *> clientsList = guiFactory()->clients();
    for (KXMLGUIClient *client : clientsList) {
        dialog.addCollection(client->actionCollection());
    }

    if (dialog.configure()) {
        // sync shortcuts for non-session actions (defined in "konsoleui.rc") in other main windows
        const QList<QWidget *> widgets = QApplication::topLevelWidgets();
        for (QWidget *mainWindowWidget : widgets) {
            auto *mainWindow = qobject_cast<MainWindow *>(mainWindowWidget);
            if ((mainWindow != nullptr) && mainWindow != this) {
                syncActiveShortcuts(mainWindow->actionCollection(), actionCollection());
            }
        }
        // sync shortcuts for session actions (defined in "sessionui.rc") in other session controllers.
        // Controllers which are currently plugged in (ie. their actions are part of the current menu)
        // must be updated immediately via syncActiveShortcuts().  Other controllers will be updated
        // when they are plugged into a main window.
        const QSet<SessionController *> allControllers = SessionController::allControllers();
        for (SessionController *controller : allControllers) {
            controller->reloadXML();
            if ((controller->factory() != nullptr) && controller != _pluggedController) {
                syncActiveShortcuts(controller->actionCollection(), _pluggedController->actionCollection());
            }
        }
    }
}

void MainWindow::newFromProfile(const Profile::Ptr &profile)
{
    createSession(profile, activeSessionDir());
}

void MainWindow::showManageProfilesDialog()
{
    showSettingsDialog(true);
}

void MainWindow::showSettingsDialog(const bool showProfilePage)
{
    ConfigurationDialog *confDialog = findChild<ConfigurationDialog *>();
    const QString profilePageName = i18nc("@title Preferences page name", "Profiles");

    if (confDialog != nullptr) {
        if (showProfilePage) {
            for (auto page : confDialog->findChildren<KPageWidgetItem *>()) {
                if (page->name().contains(profilePageName)) {
                    confDialog->setCurrentPage(page);
                    break;
                }
            }
        }
        confDialog->show();
        return;
    }

    confDialog = new ConfigurationDialog(this, KonsoleSettings::self());

    const QString generalPageName = i18nc("@title Preferences page name", "General");
    auto *generalPage = new KPageWidgetItem(new GeneralSettings(confDialog), generalPageName);
    generalPage->setIcon(QIcon::fromTheme(QStringLiteral("utilities-terminal")));
    confDialog->addPage(generalPage, true);

    auto *profileSettings = new ProfileSettings(confDialog);
    auto *profilePage = new KPageWidgetItem(profileSettings, profilePageName);
    profilePage->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-theme")));
    confDialog->addPage(profilePage, true);
    connect(confDialog, &QDialog::accepted, profileSettings, &ProfileSettings::slotAccepted);

    const QString tabBarPageName = i18nc("@title Preferences page name", "Tab Bar / Splitters");
    auto tabBarPage = new KPageWidgetItem(new TabBarSettings(confDialog), tabBarPageName);
    tabBarPage->setIcon(QIcon::fromTheme(QStringLiteral("preferences-tabs")));
    confDialog->addPage(tabBarPage, true);

    const QString temporaryFilesPageName = i18nc("@title Preferences page name", "Temporary Files");
    auto temporaryFilesPage = new KPageWidgetItem(new TemporaryFilesSettings(confDialog), temporaryFilesPageName);
    temporaryFilesPage->setIcon(QIcon::fromTheme(QStringLiteral("folder-temp")));
    confDialog->addPage(temporaryFilesPage, true);

    const QString thumbnailPageName = i18nc("@title Preferences page name", "Thumbnails");
    auto thumbnailPage = new KPageWidgetItem(new ThumbnailsSettings(confDialog), thumbnailPageName);
    thumbnailPage->setIcon(QIcon::fromTheme(QStringLiteral("image-jpeg")));
    confDialog->addPage(thumbnailPage, true);

    if (showProfilePage) {
        confDialog->setCurrentPage(profilePage);
    }

    confDialog->show();
}

void MainWindow::applyKonsoleSettings()
{
    setRemoveWindowTitleBarAndFrame(KonsoleSettings::removeWindowTitleBarAndFrame());
    if (KonsoleSettings::allowMenuAccelerators()) {
        restoreMenuAccelerators();
    } else {
        removeMenuAccelerators();
    }

    _viewManager->activeContainer()->setNavigationBehavior(KonsoleSettings::newTabBehavior());

    // Save the toolbar/menu/dockwidget states and the window geometry
    setAutoSaveSettings();

    updateWindowCaption();
}

void MainWindow::activateMenuBar()
{
    const QList<QAction *> menuActions = menuBar()->actions();

    if (menuActions.isEmpty()) {
        return;
    }

    // Show menubar if it is hidden at the moment
    if (menuBar()->isHidden()) {
        menuBar()->setVisible(true);
        _toggleMenuBarAction->setChecked(true);
    }

    // First menu action should be 'File'
    QAction *menuAction = menuActions.first();

    // TODO: Handle when menubar is top level (MacOS)
    menuBar()->setActiveAction(menuAction);
}

void MainWindow::configureNotifications()
{
    KNotifyConfigWidget::configure(this);
}

void MainWindow::setBlur(bool blur)
{
    if (_pluggedController.isNull()) {
        return;
    }

    // Saves 70-100ms when starting
    if (blur == _blurEnabled) {
        return;
    }
    _blurEnabled = blur;

    if (!_pluggedController->isKonsolePart()) {
#if KWINDOWSYSTEM_VERSION < QT_VERSION_CHECK(5, 82, 0)
        KWindowEffects::enableBlurBehind(winId(), blur);
#else
        if (QWindow *window = windowHandle()) {
            KWindowEffects::enableBlurBehind(window, blur);
        } else {
            qCWarning(KonsoleDebug) << "Blur effect couldn't be enabled.";
        }
#endif
    }
}

void MainWindow::setMenuBarInitialVisibility(bool showMenuBar)
{
    _windowArgsMenuBarVisible.enabled = true;
    _windowArgsMenuBarVisible.showMenuBar = showMenuBar;
}

void MainWindow::setRemoveWindowTitleBarAndFrame(bool frameless)
{
    Qt::WindowFlags newFlags = frameless ? Qt::FramelessWindowHint : Qt::Window;

    // The window is not yet visible
    if (!isVisible()) {
        setWindowFlags(newFlags);

        // The window is visible and the setting changed
    } else if (windowFlags().testFlag(Qt::FramelessWindowHint) != frameless) {
        const auto oldGeometry = saveGeometry();
        // This happens for every Konsole window. It depends on
        // the fact that every window is processed in single thread
        const auto oldActiveWindow = KWindowSystem::activeWindow();

        setWindowFlags(newFlags);

        // The setWindowFlags() has hidden the window. Show it again
        // with previous geometry
        restoreGeometry(oldGeometry);
        setVisible(true);
        KWindowSystem::activateWindow(oldActiveWindow);
    }
}

void MainWindow::showEvent(QShowEvent *event)
{
    // Apply this code on first show only
    if (_firstShowEvent) {
        _firstShowEvent = false;

        if (!KonsoleSettings::rememberWindowSize() || !wasWindowGeometrySaved()) {
            // Delay resizing to here, so that the other parts of the UI
            // (ViewManager, TabbedViewContainer, TerminalDisplay ... etc)
            // have been created and TabbedViewContainer::sizeHint() returns
            // a usable size.

            // Remove the WindowMaximized state to override the Window-Maximized
            // config key
            if (QWindow *window = windowHandle()) {
                window->setWindowStates(window->windowStates() & ~Qt::WindowMaximized);
            }

            resize(sizeHint());
        }
    }

    // Call parent method
    KXmlGuiWindow::showEvent(event);
}

void MainWindow::triggerAction(const QString &name) const
{
    if (auto action = actionCollection()->action(name)) {
        if (action->isEnabled()) {
            action->trigger();
        }
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (!_pluggedController.isNull() && obj == _pluggedController->view()) {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
            switch (static_cast<QMouseEvent *>(event)->button()) {
            case Qt::ForwardButton:
                triggerAction(QStringLiteral("next-view"));
                break;
            case Qt::BackButton:
                triggerAction(QStringLiteral("previous-view"));
                break;
            default:;
            }
        default:;
        }
    }

    return KXmlGuiWindow::eventFilter(obj, event);
}

bool MainWindow::focusNextPrevChild(bool v)
{
    if (qobject_cast<TerminalDisplay *>(focusWidget())) {
        return false;
    }

    return QMainWindow::focusNextPrevChild(v);
}

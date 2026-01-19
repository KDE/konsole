/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "MainWindow.h"

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
#include <KColorSchemeManager>
#include <KColorSchemeMenu>
#include <KCrash>
#include <KHamburgerMenu>
#include <KIconUtils>
#include <KLocalizedString>
#include <KMessageBox>
#include <KNotifyConfigWidget>
#include <KShortcutsDialog>
#include <KStandardAction>
#include <KStandardGuiItem>
#include <KToolBar>
#include <KWindowConfig>
#include <KWindowEffects>
#include <KWindowSystem>
#include <KXMLGUIFactory>

#if WITH_X11
#include <KX11Extras>
#endif

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
#include "settings/MemorySettings.h"
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

    // Hamburger menu for when the menubar is hidden
    _hamburgerMenu = KStandardAction::hamburgerMenu(nullptr, nullptr, actionCollection());
    _hamburgerMenu->setShowMenuBarAction(_toggleMenuBarAction);
    _hamburgerMenu->setMenuBar(menuBar());
    connect(_hamburgerMenu, &KHamburgerMenu::aboutToShowMenu, this, &MainWindow::updateHamburgerMenu);

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

bool MainWindow::wasWindowGeometrySaved() const
{
    KSharedConfigPtr konsoleConfig = KSharedConfig::openStateConfig();
    KConfigGroup cg = konsoleConfig->group(QStringLiteral("MainWindow"));
    if (!cg.exists()) { // First run, no existing konsolerc?
        return false;
    }

    return KWindowConfig::hasSavedWindowSize(cg) || KWindowConfig::hasSavedWindowPosition(cg);
}

void MainWindow::updateUseTransparency()
{
    if (!WindowSystemInfo::HAVE_TRANSPARENCY) {
        return;
    }

    bool useTranslucency = WindowSystemInfo::compositingActive();

    setAttribute(Qt::WA_TranslucentBackground, useTranslucency);
    setAttribute(Qt::WA_NoSystemBackground, false);
    WindowSystemInfo::HAVE_TRANSPARENCY = useTranslucency;
}

void MainWindow::activationRequest(const QString &xdgActivationToken)
{
    KWindowSystem::setCurrentXdgActivationToken(xdgActivationToken);

    if (KWindowSystem::isPlatformX11()) {
#if WITH_X11
        KX11Extras::forceActiveWindow(winId());
#endif
    } else {
        KWindowSystem::activateWindow(windowHandle());
    }
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

    // replace F10 shortcut for main menu to pass F10 key to the console application
    QAction *openMenu = actionCollection()->action(QStringLiteral("hamburger_menu"));
    if (openMenu != nullptr) {
        actionCollection()->setDefaultShortcut(openMenu, QKeySequence(Qt::SHIFT | Qt::Key_F10));
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

    // add hamburger menu to controller
    controller->actionCollection()->addActions({_hamburgerMenu});

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

    // to prevent shortcuts conflict
    if (auto hamburgerMenu = _hamburgerMenu->menu()) {
        hamburgerMenu->clear();
    }

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
    connect(_toggleMenuBarAction, &QAction::triggered, [=] {
        // Remove menubar icons set for the hamburger menu, so they don't override
        // the text when they appear in the in-window menubar
        collection->action(QStringLiteral("bookmark"))->setIcon(QIcon());
        static_cast<QMenu *>(factory()->container(QStringLiteral("plugins"), this))->setIcon(QIcon());
    });

    // Set up themes
#if KCOLORSCHEME_VERSION < QT_VERSION_CHECK(6, 6, 0)
    auto *manager = new KColorSchemeManager(actionCollection());
#else
    auto *manager = KColorSchemeManager::instance();
#endif
    manager->setAutosaveChanges(true);
    KActionMenu *selectionMenu = KColorSchemeMenu::createMenu(manager, this);
    auto winColorSchemeMenu = new QAction(this);
    winColorSchemeMenu->setMenu(selectionMenu->menu());
    winColorSchemeMenu->menu()->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-color")));
    winColorSchemeMenu->menu()->setTitle(i18n("&Window Color Scheme"));
    actionCollection()->addAction(QStringLiteral("window-colorscheme-menu"), winColorSchemeMenu);

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

void MainWindow::updateHamburgerMenu()
{
    KActionCollection *collection = actionCollection();
    KActionCollection *controllerCollection = _pluggedController->actionCollection();

    QMenu *menu = _hamburgerMenu->menu();
    if (!menu) {
        menu = new QMenu(widget());
        _hamburgerMenu->setMenu(menu);
    } else {
        menu->clear();
    }

    menu->addAction(collection->action(QStringLiteral("new-window")));
    menu->addAction(collection->action(QStringLiteral("new-tab")));
    menu->addAction(controllerCollection->action(QStringLiteral("file_save_as")));

    menu->addSeparator();

    menu->addAction(controllerCollection->action(QStringLiteral("edit_copy")));
    menu->addAction(controllerCollection->action(QStringLiteral("edit_paste")));
    menu->addAction(controllerCollection->action(QStringLiteral("edit_find")));

    menu->addSeparator();

    menu->addAction(collection->action(KStandardAction::name(KStandardAction::FullScreen)));
    menu->addAction(collection->action(QStringLiteral("split-view")));
    menu->addAction(controllerCollection->action(QStringLiteral("clear-history-and-reset")));
    menu->addAction(controllerCollection->action(QStringLiteral("enlarge-font")));
    menu->addAction(controllerCollection->action(QStringLiteral("reset-font-size")));
    menu->addAction(controllerCollection->action(QStringLiteral("shrink-font")));

    menu->addSeparator();

    menu->addAction(controllerCollection->action(QStringLiteral("edit-current-profile")));
    menu->addAction(controllerCollection->action(QStringLiteral("switch-profile")));

    menu->addSeparator();

    if (auto viewMenu = static_cast<QMenu *>(factory()->container(QStringLiteral("view"), nullptr))) {
        auto monitorMenu = menu->addMenu(QIcon::fromTheme(QStringLiteral("visibility")), viewMenu->title());
        monitorMenu->addAction(controllerCollection->action(QStringLiteral("monitor-silence")));
        monitorMenu->addAction(controllerCollection->action(QStringLiteral("monitor-activity")));
        monitorMenu->addAction(controllerCollection->action(QStringLiteral("monitor-process-finish")));
        menu->addMenu(monitorMenu);
        _hamburgerMenu->hideActionsOf(monitorMenu);
    }

    auto bookmarkMenu = collection->action(QStringLiteral("bookmark"));
    bookmarkMenu->setIcon(QIcon::fromTheme(QStringLiteral("bookmarks"))); // Icon will be removed again when the menu bar is enabled.
    menu->addAction(bookmarkMenu);

    if (auto pluginsMenu = static_cast<QMenu *>(factory()->container(QStringLiteral("plugins"), this))) {
        pluginsMenu->setIcon(QIcon::fromTheme(QStringLiteral("plugins"))); // Icon will be removed again when the menu bar is enabled.
        menu->addMenu(pluginsMenu);
    }

    if (auto settingsMenu = static_cast<QMenu *>(factory()->container(QStringLiteral("settings"), nullptr))) {
        auto configureMenu = menu->addMenu(QIcon::fromTheme(QStringLiteral("configure")), settingsMenu->title());
        configureMenu->addAction(toolBarMenuAction());
        configureMenu->addSeparator();
        configureMenu->addAction(collection->action(KStandardAction::name(KStandardAction::SwitchApplicationLanguage)));
        configureMenu->addAction(collection->action(KStandardAction::name(KStandardAction::KeyBindings)));
        configureMenu->addAction(collection->action(KStandardAction::name(KStandardAction::ConfigureToolbars)));
        configureMenu->addAction(collection->action(KStandardAction::name(KStandardAction::ConfigureNotifications)));
        configureMenu->addAction(collection->action(KStandardAction::name(KStandardAction::Preferences)));
        _hamburgerMenu->hideActionsOf(configureMenu);
    }

    _hamburgerMenu->hideActionsOf(toolBar());
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
    if (_windowArgsShowMenuBar.has_value()) {
        menuBar()->setVisible(_windowArgsShowMenuBar.value());
    }

    // Override the toolbar state from the config file
    if (_windowArgsShowToolBars.has_value()) {
        for (const auto& name : toolBarNames()) {
            setToolBarVisible(name, _windowArgsShowToolBars.value());
        }
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

void MainWindow::setPluginsActions(const QList<QAction *> &actions)
{
    _pluginsActions = actions;
    unplugActionList(QStringLiteral("plugin-submenu"));
    plugActionList(QStringLiteral("plugin-submenu"), _pluginsActions);
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

#if WITH_X11
    // make sure the window is shown on current desktop and is not minimized
    KX11Extras::setOnDesktop(winId(), KX11Extras::currentDesktop());
#endif
    int result;

    if (!processesRunning.isEmpty()) {
        if (openTabs == 1) {
            result = KMessageBox::warningTwoActionsList(this,
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
            if (result == KMessageBox::SecondaryAction) // SecondaryAction is equal to cancel closing
                result = KMessageBox::Cancel;
        } else {
            result = KMessageBox::warningTwoActionsCancelList(this,
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
        result = KMessageBox::warningTwoActionsCancel(this,
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
    case KMessageBox::PrimaryAction:
        return true;
    case KMessageBox::SecondaryAction:
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
            const auto items = confDialog->findChildren<KPageWidgetItem *>();
            for (const auto page : items) {
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

#if defined(Q_OS_LINUX)
    const QString memoryPageName = i18nc("@title Preferences page name", "Memory");
    auto memoryPage = new KPageWidgetItem(new MemorySettings(confDialog), memoryPageName);
    memoryPage->setIcon(QIcon::fromTheme(QStringLiteral("memory")));
    confDialog->addPage(memoryPage, true);
#endif

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
        if (QWindow *window = windowHandle()) {
            KWindowEffects::enableBlurBehind(window, blur);
        } else {
            qCWarning(KonsoleDebug) << "Blur effect couldn't be enabled.";
        }
    }
}

void MainWindow::setMenuBarInitialVisibility(bool showMenuBar)
{
    _windowArgsShowMenuBar = showMenuBar;
}

void MainWindow::setToolBarsInitialVisibility(bool showToolbars)
{
    _windowArgsShowToolBars = showToolbars;
}

void MainWindow::setRemoveWindowTitleBarAndFrame(bool frameless)
{
    Qt::WindowFlags newFlags = frameless ? Qt::FramelessWindowHint : Qt::Window;

    // The window is not yet visible
    if (!isVisible()) {
        setWindowFlags(newFlags);

        // The window is visible and the setting changed
    } else if (windowFlags().testFlag(Qt::FramelessWindowHint) != frameless) {
        // Usually, a QDialog with a parent stays on top of its parent.
        // However, after hiding and showing the main window here, this
        // stops being true.
        //
        // This can confuse users, if they apply this setting without closing
        // the configuration dialog, and the dialog gets behind the main window.
        // Afterwards, trying to open the configuration dialog doesn't work
        // (it's already open, but behind the window).
        //
        // So, hide and show also the configuration dialog, which makes the dialog
        // stay on top of its parent again.
        ConfigurationDialog *confDialog = findChild<ConfigurationDialog *>();
        QByteArray confDialogGeometry;
        bool isConfDialogVisible = false;
        if (confDialog != nullptr && confDialog->isVisible()) {
            isConfDialogVisible = true;
            confDialogGeometry = confDialog->saveGeometry();
            confDialog->hide();
        }

        if (KWindowSystem::isPlatformX11()) {
#if WITH_X11
            const auto oldGeometry = saveGeometry();
            // This happens for every Konsole window. It depends on
            // the fact that every window is processed in single thread
            const auto oldActiveWindow = KX11Extras::activeWindow();

            setWindowFlags(newFlags);

            // The setWindowFlags() has hidden the window. Show it again
            // with previous geometry
            restoreGeometry(oldGeometry);
            setVisible(true);
            KX11Extras::activateWindow(oldActiveWindow);
#endif
        } else {
            // Restoring geometry ourselves doesn't work on Wayland
            setWindowFlags(newFlags);
            // The setWindowFlags() has hidden the window. Show it again
            setVisible(true);
        }

        if (confDialog != nullptr && isConfDialogVisible) {
            confDialog->restoreGeometry(confDialogGeometry);
            confDialog->show();
        }
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

QWidget *MainWindow::createContainer(QWidget *parent, int index, const QDomElement &element, QAction *&containerAction)
{
    // ensure we don't have toolbar accelerators that clash with other stuff
    QWidget *createdContainer = KXmlGuiWindow::createContainer(parent, index, element, containerAction);
    if (element.tagName() == QLatin1String("ToolBar")) {
        KAcceleratorManager::setNoAccel(createdContainer);
    }
    return createdContainer;
}

void MainWindow::saveNewToolbarConfig()
{
    KXmlGuiWindow::saveNewToolbarConfig();

    unplugActionList(QStringLiteral("plugin-submenu"));
    plugActionList(QStringLiteral("plugin-submenu"), _pluginsActions);
}

#include "moc_MainWindow.cpp"

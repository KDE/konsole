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
#include "MainWindow.h"

// Qt
#include <QMouseEvent>

// KDE
#include <KAcceleratorManager>
#include <KActionCollection>
#include <KActionMenu>
#include <KShortcutsDialog>
#include <KLocalizedString>
#include <KWindowEffects>

#include <QMenu>
#include <QMenuBar>
#include <KMessageBox>
#include <KStandardAction>
#include <KStandardGuiItem>
#include <KWindowSystem>
#include <KXMLGUIFactory>
#include <KNotifyConfigWidget>
#include <KIconLoader>

// Konsole
#include "BookmarkHandler.h"
#include "SessionController.h"
#include "ProfileList.h"
#include "Session.h"
#include "ViewContainer.h"
#include "ViewManager.h"
#include "SessionManager.h"
#include "ProfileManager.h"
#include "KonsoleSettings.h"
#include "WindowSystemInfo.h"
#include "TerminalDisplay.h"
#include "settings/ConfigurationDialog.h"
#include "settings/TemporaryFilesSettings.h"
#include "settings/GeneralSettings.h"
#include "settings/ProfileSettings.h"
#include "settings/TabBarSettings.h"

using namespace Konsole;

MainWindow::MainWindow() :
    KXmlGuiWindow(),
    _viewManager(nullptr),
    _bookmarkHandler(nullptr),
    _toggleMenuBarAction(nullptr),
    _newTabMenuAction(nullptr),
    _pluggedController(nullptr),
    _menuBarInitialVisibility(true),
    _menuBarInitialVisibilityApplied(false)
{
    if (!KonsoleSettings::saveGeometryOnExit()) {
        // If we are not using the global Konsole save geometry on exit,
        // remove all Height and Width from [MainWindow] from konsolerc
        // Each screen resolution will have entries (Width 1280=619)
        KSharedConfigPtr konsoleConfig = KSharedConfig::openConfig(QStringLiteral("konsolerc"));
        KConfigGroup group = konsoleConfig->group("MainWindow");
        QMap<QString, QString> configEntries = group.entryMap();
        QMapIterator<QString, QString> i(configEntries);
        while (i.hasNext()) {
            i.next();
            if (i.key().startsWith(QLatin1String("Width"))
                || i.key().startsWith(QLatin1String("Height"))) {
                group.deleteEntry(i.key());
            }
        }
    }

    updateUseTransparency();

    // create actions for menus
    setupActions();

    // create view manager
    _viewManager = new ViewManager(this, actionCollection());
    connect(_viewManager, &Konsole::ViewManager::empty, this, &Konsole::MainWindow::close);
    connect(_viewManager, &Konsole::ViewManager::activeViewChanged, this,
            &Konsole::MainWindow::activeViewChanged);
    connect(_viewManager, &Konsole::ViewManager::unplugController, this,
            &Konsole::MainWindow::disconnectController);
    connect(_viewManager, &Konsole::ViewManager::viewPropertiesChanged,
            bookmarkHandler(), &Konsole::BookmarkHandler::setViews);
    connect(_viewManager, &Konsole::ViewManager::blurSettingChanged,
            this, &Konsole::MainWindow::setBlur);

    connect(_viewManager, &Konsole::ViewManager::updateWindowIcon, this,
            &Konsole::MainWindow::updateWindowIcon);
    connect(_viewManager, &Konsole::ViewManager::newViewWithProfileRequest,
            this, &Konsole::MainWindow::newFromProfile);
    connect(_viewManager, &Konsole::ViewManager::newViewRequest,
            this, &Konsole::MainWindow::newTab);
    connect(_viewManager, &Konsole::ViewManager::terminalsDetached, this,
            &Konsole::MainWindow::terminalsDetached);

    setCentralWidget(_viewManager->widget());

    // disable automatically generated accelerators in top-level
    // menu items - to avoid conflicting with Alt+[Letter] shortcuts
    // in terminal applications
    KAcceleratorManager::setNoAccel(menuBar());

    // create menus
    createGUI();

    // remember the original menu accelerators for later use
    rememberMenuAccelerators();

    // replace standard shortcuts which cannot be used in a terminal
    // emulator (as they are reserved for use by terminal applications)
    correctStandardShortcuts();

    setProfileList(new ProfileList(true, this));

    // this must come at the end
    applyKonsoleSettings();
    connect(KonsoleSettings::self(), &Konsole::KonsoleSettings::configChanged, this,
            &Konsole::MainWindow::applyKonsoleSettings);
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

void MainWindow::rememberMenuAccelerators()
{
    foreach (QAction *menuItem, menuBar()->actions()) {
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
    foreach (QAction *menuItem, menuBar()->actions()) {
        menuItem->setText(menuItem->text().replace(QLatin1Char('&'), QString()));
    }
}

void MainWindow::restoreMenuAccelerators()
{
    foreach (QAction *menuItem, menuBar()->actions()) {
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

    // replace Ctrl+B shortcut for bookmarks only if user hasn't already
    // changed the shortcut; however, if the user changed it to Ctrl+B
    // this will still get changed to Ctrl+Shift+B
    QAction *bookmarkAction = actionCollection()->action(QStringLiteral("add_bookmark"));
    if ((bookmarkAction != nullptr)
        && bookmarkAction->shortcut() == QKeySequence(Konsole::ACCEL + Qt::Key_B)) {
        actionCollection()->setDefaultShortcut(bookmarkAction,
                                               Konsole::ACCEL + Qt::SHIFT + Qt::Key_B);
    }
}

ViewManager *MainWindow::viewManager() const
{
    return _viewManager;
}

void MainWindow::disconnectController(SessionController *controller)
{
    disconnect(controller, &Konsole::SessionController::titleChanged,
               this, &Konsole::MainWindow::activeViewTitleChanged);
    disconnect(controller, &Konsole::SessionController::rawTitleChanged,
               this, &Konsole::MainWindow::updateWindowCaption);
    disconnect(controller, &Konsole::SessionController::iconChanged,
               this, &Konsole::MainWindow::updateWindowIcon);

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
    connect(bookmarkHandler(), &Konsole::BookmarkHandler::openUrl, controller,
            &Konsole::SessionController::openUrl);

    if (!_pluggedController.isNull()) {
        disconnectController(_pluggedController);
    }

    Q_ASSERT(controller);
    _pluggedController = controller;
    _pluggedController->view()->installEventFilter(this);

    setBlur(ViewManager::profileHasBlurEnabled(SessionManager::instance()->sessionProfile(_pluggedController->session())));

    // listen for title changes from the current session
    connect(controller, &Konsole::SessionController::titleChanged, this,
            &Konsole::MainWindow::activeViewTitleChanged);
    connect(controller, &Konsole::SessionController::rawTitleChanged, this,
            &Konsole::MainWindow::updateWindowCaption);
    connect(controller, &Konsole::SessionController::iconChanged, this,
            &Konsole::MainWindow::updateWindowIcon);

    controller->setShowMenuAction(_toggleMenuBarAction);
    guiFactory()->addClient(controller);

    // update session title to match newly activated session
    activeViewTitleChanged(controller);

    // Update window icon to newly activated session's icon
    updateWindowIcon();
}

void MainWindow::activeViewTitleChanged(ViewProperties *properties)
{
    Q_UNUSED(properties);
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
    _newTabMenuAction = new KActionMenu(QIcon::fromTheme(QStringLiteral("tab-new")),
                                        i18nc("@action:inmenu", "&New Tab"), collection);
    collection->setDefaultShortcut(_newTabMenuAction, Konsole::ACCEL + Qt::SHIFT + Qt::Key_T);
    collection->setShortcutsConfigurable(_newTabMenuAction, true);
    _newTabMenuAction->setAutoRepeat(false);
    connect(_newTabMenuAction, &KActionMenu::triggered, this, &MainWindow::newTab);
    collection->addAction(QStringLiteral("new-tab"), _newTabMenuAction);
    collection->setShortcutsConfigurable(_newTabMenuAction, true);

    QAction* menuAction = collection->addAction(QStringLiteral("clone-tab"));
    menuAction->setIcon(QIcon::fromTheme(QStringLiteral("tab-duplicate")));
    menuAction->setText(i18nc("@action:inmenu", "&Clone Tab"));
    collection->setDefaultShortcut(menuAction, QKeySequence());
    menuAction->setAutoRepeat(false);
    connect(menuAction, &QAction::triggered, this, &Konsole::MainWindow::cloneTab);

    menuAction = collection->addAction(QStringLiteral("new-window"));
    menuAction->setIcon(QIcon::fromTheme(QStringLiteral("window-new")));
    menuAction->setText(i18nc("@action:inmenu", "New &Window"));
    collection->setDefaultShortcut(menuAction, Konsole::ACCEL + Qt::SHIFT + Qt::Key_N);
    menuAction->setAutoRepeat(false);
    connect(menuAction, &QAction::triggered, this, &Konsole::MainWindow::newWindow);

    menuAction = collection->addAction(QStringLiteral("close-window"));
    menuAction->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));
    menuAction->setText(i18nc("@action:inmenu", "Close Window"));
    collection->setDefaultShortcut(menuAction, Konsole::ACCEL + Qt::SHIFT + Qt::Key_Q);
    connect(menuAction, &QAction::triggered, this, &Konsole::MainWindow::close);

    // Bookmark Menu
    KActionMenu *bookmarkMenu = new KActionMenu(i18nc("@title:menu", "&Bookmarks"), collection);
    _bookmarkHandler = new BookmarkHandler(collection, bookmarkMenu->menu(), true, this);
    collection->addAction(QStringLiteral("bookmark"), bookmarkMenu);
    connect(_bookmarkHandler, &Konsole::BookmarkHandler::openUrls, this,
            &Konsole::MainWindow::openUrls);

    // Settings Menu
    _toggleMenuBarAction = KStandardAction::showMenubar(menuBar(), SLOT(setVisible(bool)), collection);
    collection->setDefaultShortcut(_toggleMenuBarAction, Konsole::ACCEL + Qt::SHIFT + Qt::Key_M);

    // Full Screen
    menuAction = KStandardAction::fullScreen(this, SLOT(viewFullScreen(bool)), this, collection);
    collection->setDefaultShortcut(menuAction, Qt::Key_F11);

    KStandardAction::configureNotifications(this, SLOT(configureNotifications()), collection);
    KStandardAction::keyBindings(this, SLOT(showShortcutsDialog()), collection);
    KStandardAction::preferences(this, SLOT(showSettingsDialog()), collection);

    menuAction = collection->addAction(QStringLiteral("manage-profiles"));
    menuAction->setText(i18nc("@action:inmenu", "Manage Profiles..."));
    menuAction->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
    connect(menuAction, &QAction::triggered, this, &Konsole::MainWindow::showManageProfilesDialog);

    // Set up an shortcut-only action for activating menu bar.
    menuAction = collection->addAction(QStringLiteral("activate-menu"));
    menuAction->setText(i18nc("@item", "Activate Menu"));
    collection->setDefaultShortcut(menuAction, Konsole::ACCEL + Qt::SHIFT + Qt::Key_F10);
    connect(menuAction, &QAction::triggered, this, &Konsole::MainWindow::activateMenuBar);
}

void MainWindow::viewFullScreen(bool fullScreen)
{
    if (fullScreen) {
        setWindowState(windowState() | Qt::WindowFullScreen);
    } else {
        setWindowState(windowState() & ~Qt::WindowFullScreen);
    }
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
    // If only 1 profile is to be shown in the menu, only display
    // it if it is the non-default profile.
    if (sessionActions.size() > 2) {
        // Update the 'New Tab' KActionMenu
        if (_newTabMenuAction->menu() != nullptr) {
            _newTabMenuAction->menu()->clear();
        } else {
            _newTabMenuAction->setMenu(new QMenu());
        }
        foreach (QAction *sessionAction, sessionActions) {
            _newTabMenuAction->menu()->addAction(sessionAction);

            // NOTE: defaultProfile seems to not work here, sigh.
            Profile::Ptr profile = ProfileManager::instance()->defaultProfile();
            if (profile && profile->name() == sessionAction->text().remove(QLatin1Char('&'))) {
                QIcon icon(KIconLoader::global()->loadIcon(profile->icon(), KIconLoader::Small, 0, KIconLoader::DefaultState, QStringList(QStringLiteral("emblem-favorite"))));
                sessionAction->setIcon(icon);
                _newTabMenuAction->menu()->setDefaultAction(sessionAction);
                QFont actionFont = sessionAction->font();
                actionFont.setBold(true);
                sessionAction->setFont(actionFont);
            }
        }
    } else {
        if (_newTabMenuAction->menu() != nullptr) {
            _newTabMenuAction->menu()->clear();
        } else {
            _newTabMenuAction->setMenu(new QMenu());
        }
        Profile::Ptr profile = ProfileManager::instance()->defaultProfile();

        // NOTE: Compare names w/o any '&'
        if (sessionActions.size() == 2
            && sessionActions[1]->text().remove(QLatin1Char('&')) != profile->name()) {
            _newTabMenuAction->menu()->addAction(sessionActions[1]);
        } else {
            _newTabMenuAction->menu()->deleteLater();
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
    } else {
        return QString();
    }
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
    emit newWindowRequest(defaultProfile, activeSessionDir());
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
    const auto uniqueSessions = QSet<Session*>::fromList(_viewManager->sessions());

    foreach (Session *session, uniqueSessions) {
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
    if (isMinimized()) {
        KWindowSystem::unminimizeWindow(winId());
    }
    int result;

    if (!processesRunning.isEmpty()) {
        if (openTabs == 1) {
            result = KMessageBox::warningYesNoList(this,
                                                         i18ncp("@info",
                                                                "There is a process running in this window. "
                                                                "Do you still want to quit?",
                                                                "There are %1 processes running in this window. "
                                                                "Do you still want to quit?",
                                                                processesRunning.count()),
                                                         processesRunning,
                                                         i18nc("@title", "Confirm Close"),
                                                         KGuiItem(i18nc("@action:button",
                                                                        "Close &Window"),
                                                                  QStringLiteral("window-close")),
                                                         KStandardGuiItem::cancel(),
                                                         // don't ask again name is wrong but I can't update.
                                                         // this is not about tabs anymore. it's about empty tabs *or* splits.
                                                         QStringLiteral("CloseAllTabs"));
            if (result == KMessageBox::No) // No is equal to cancel closing
                result = KMessageBox::Cancel;
        } else {
            result = KMessageBox::warningYesNoCancelList(this,
                                                         i18ncp("@info",
                                                                "There is a process running in this window. "
                                                                "Do you still want to quit?",
                                                                "There are %1 processes running in this window. "
                                                                "Do you still want to quit?",
                                                                processesRunning.count()),
                                                         processesRunning,
                                                         i18nc("@title", "Confirm Close"),
                                                         KGuiItem(i18nc("@action:button",
                                                                        "Close &Window"),
                                                                  QStringLiteral("window-close")),
                                                         KGuiItem(i18nc("@action:button",
                                                                        "Close Current &Tab"),
                                                                  QStringLiteral("tab-close")),
                                                         KStandardGuiItem::cancel(),
                                                         // don't ask again name is wrong but I can't update.
                                                         // this is not about tabs anymore. it's about empty tabs *or* splits.
                                                         QStringLiteral("CloseAllTabs"));
        }
    } else {
        result = KMessageBox::warningYesNoCancel(this,
                                                 i18nc("@info",
                                                       "There are %1 open terminals in this window. "
                                                       "Do you still want to quit?",
                                                       openTabs),
                                                 i18nc("@title", "Confirm Close"),
                                                 KGuiItem(i18nc("@action:button", "Close &Window"),
                                                          QStringLiteral("window-close")),
                                                 KGuiItem(i18nc("@action:button",
                                                                "Close Current &Tab"),
                                                          QStringLiteral("tab-close")),
                                                 KStandardGuiItem::cancel(),
                                                 // don't ask again name is wrong but I can't update.
                                                 // this is not about tabs anymore. it's about empty tabs *or* splits.
                                                 QStringLiteral("CloseAllEmptyTabs"));
    }

    switch (result) {
    case KMessageBox::Yes:
        return true;
    case KMessageBox::No:
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
    foreach (QAction *qAction, source->actions()) {
        if (QAction *destQAction = dest->action(qAction->objectName())) {
            destQAction->setShortcut(qAction->shortcut());
        }
    }
}

void MainWindow::showShortcutsDialog()
{
    KShortcutsDialog dialog(KShortcutsEditor::AllActions,
                            KShortcutsEditor::LetterShortcutsDisallowed, this);

    // add actions from this window and the current session controller
    foreach (KXMLGUIClient *client, guiFactory()->clients()) {
        dialog.addCollection(client->actionCollection());
    }

    if (dialog.configure()) {
        // sync shortcuts for non-session actions (defined in "konsoleui.rc") in other main windows
        foreach (QWidget *mainWindowWidget, QApplication::topLevelWidgets()) {
            auto *mainWindow = qobject_cast<MainWindow *>(mainWindowWidget);
            if ((mainWindow != nullptr) && mainWindow != this) {
                syncActiveShortcuts(mainWindow->actionCollection(), actionCollection());
            }
        }
        // sync shortcuts for session actions (defined in "sessionui.rc") in other session controllers.
        // Controllers which are currently plugged in (ie. their actions are part of the current menu)
        // must be updated immediately via syncActiveShortcuts().  Other controllers will be updated
        // when they are plugged into a main window.
        foreach (SessionController *controller, SessionController::allControllers()) {
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
    static ConfigurationDialog *confDialog = nullptr;
    if (confDialog != nullptr) {
        confDialog->show();
        return;
    }

    confDialog = new ConfigurationDialog(this, KonsoleSettings::self());

    const QString generalPageName = i18nc("@title Preferences page name", "General");
    auto *generalPage = new KPageWidgetItem(new GeneralSettings(confDialog), generalPageName);
    generalPage->setIcon(QIcon::fromTheme(QStringLiteral("utilities-terminal")));
    confDialog->addPage(generalPage, true);

    const QString profilePageName = i18nc("@title Preferences page name", "Profiles");
    auto profilePage = new KPageWidgetItem(new ProfileSettings(confDialog), profilePageName);
    profilePage->setIcon(QIcon::fromTheme(QStringLiteral("preferences-system-profiles")));
    confDialog->addPage(profilePage, true);

    const QString tabBarPageName = i18nc("@title Preferences page name", "Tab Bar");
    auto tabBarPage = new KPageWidgetItem(new TabBarSettings(confDialog), tabBarPageName);
    tabBarPage->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
    confDialog->addPage(tabBarPage, true);

    const QString temporaryFilesPageName = i18nc("@title Preferences page name", "Temporary Files");
    auto temporaryFilesPage = new KPageWidgetItem(new TemporaryFilesSettings(confDialog), temporaryFilesPageName);
    temporaryFilesPage->setIcon(QIcon::fromTheme(QStringLiteral("folder-temp")));
    confDialog->addPage(temporaryFilesPage, true);

    if (showProfilePage) {
        confDialog->setCurrentPage(profilePage);
    }

    confDialog->show();
}

void MainWindow::applyKonsoleSettings()
{
    setMenuBarInitialVisibility(KonsoleSettings::showMenuBarByDefault());
    setRemoveWindowTitleBarAndFrame(KonsoleSettings::removeWindowTitleBarAndFrame());
    if (KonsoleSettings::allowMenuAccelerators()) {
        restoreMenuAccelerators();
    } else {
        removeMenuAccelerators();
    }

    _viewManager->activeContainer()->setNavigationBehavior(KonsoleSettings::newTabBehavior());
    setAutoSaveSettings(QStringLiteral("MainWindow"), KonsoleSettings::saveGeometryOnExit());
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

    if (!_pluggedController->isKonsolePart()) {
        KWindowEffects::enableBlurBehind(winId(), blur);
    }
}

void MainWindow::setMenuBarInitialVisibility(bool visible)
{
    _menuBarInitialVisibility = visible;
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
    // Make sure the 'initial' visibility is applied only once.
    if (!_menuBarInitialVisibilityApplied) {
        // the initial visibility of menubar should be applied at this last
        // moment. Otherwise, the initial visibility will be determined by
        // what KMainWindow has automatically stored in konsolerc, but not by
        // what users has explicitly configured .
        menuBar()->setVisible(_menuBarInitialVisibility);
        _toggleMenuBarAction->setChecked(_menuBarInitialVisibility);
        _menuBarInitialVisibilityApplied = true;
        if (!KonsoleSettings::saveGeometryOnExit()) {
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
        switch(event->type()) {
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonDblClick:
            switch(static_cast<QMouseEvent*>(event)->button()) {
                case Qt::ForwardButton:
                    triggerAction(QStringLiteral("next-view"));
                    break;
                case Qt::BackButton:
                    triggerAction(QStringLiteral("previous-view"));
                    break;
                default: ;
            }
            default: ;
        }
    }

    return KXmlGuiWindow::eventFilter(obj, event);
}

bool MainWindow::focusNextPrevChild(bool)
{
    // In stand-alone konsole, always disable implicit focus switching
    // through 'Tab' and 'Shift+Tab'
    //
    // Kpart is another different story
    return false;
}

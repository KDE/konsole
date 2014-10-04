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
#include <QVBoxLayout>

// KDE
#include <KAcceleratorManager>
#include <KAction>
#include <KActionCollection>
#include <KActionMenu>
#include <KCmdLineArgs>
#include <KShortcutsDialog>
#include <KLocale>

#include <KMenu>
#include <KMenuBar>
#include <KMessageBox>
#include <KToggleFullScreenAction>
#include <KStandardAction>
#include <KStandardGuiItem>
#include <KWindowSystem>
#include <KXMLGUIFactory>
#include <KNotifyConfigWidget>
#include <KConfigDialog>
#include <KApplication>
#include <KGlobal>
#include <KShortcut>

// Konsole
#include "BookmarkHandler.h"
#include "SessionController.h"
#include "ProfileList.h"
#include "ManageProfilesDialog.h"
#include "Session.h"
#include "ViewManager.h"
#include "SessionManager.h"
#include "ProfileManager.h"
#include "KonsoleSettings.h"
#include "WindowSystemInfo.h"
#include "settings/GeneralSettings.h"
#include "settings/TabBarSettings.h"

using namespace Konsole;

MainWindow::MainWindow()
    : KXmlGuiWindow()
    , _bookmarkHandler(0)
    , _pluggedController(0)
    , _menuBarInitialVisibility(true)
    , _menuBarInitialVisibilityApplied(false)
{
    if (!KonsoleSettings::saveGeometryOnExit()) {
        // If we are not using the global Konsole save geometry on exit,
        // remove all Height and Width from [MainWindow] from konsolerc
        // Each screen resolution will have entries (Width 1280=619)
        KSharedConfigPtr konsoleConfig = KSharedConfig::openConfig("konsolerc");
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

    connect(KWindowSystem::self(), SIGNAL(compositingChanged(bool)), this, SLOT(updateUseTransparency()));

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

    connect(_viewManager, &Konsole::ViewManager::updateWindowIcon, this,
            &Konsole::MainWindow::updateWindowIcon);
    connect(_viewManager, static_cast<void(ViewManager::*)(Profile::Ptr)>(&Konsole::ViewManager::newViewRequest),
            this, &Konsole::MainWindow::newFromProfile);
    connect(_viewManager, static_cast<void(ViewManager::*)()>(&Konsole::ViewManager::newViewRequest),
            this, &Konsole::MainWindow::newTab);
    connect(_viewManager, &Konsole::ViewManager::viewDetached,
            this, &Konsole::MainWindow::viewDetached);

    // create the main widget
    setupMainWidget();

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
    connect(KonsoleSettings::self(), &Konsole::KonsoleSettings::configChanged, this, &Konsole::MainWindow::applyKonsoleSettings);
}

void MainWindow::updateUseTransparency()
{
    const KCmdLineArgs* args = KCmdLineArgs::parsedArgs();

    bool useTranslucency = KWindowSystem::compositingActive() && args->isSet("transparency");

    setAttribute(Qt::WA_TranslucentBackground, useTranslucency);
    setAttribute(Qt::WA_NoSystemBackground, false);

    WindowSystemInfo::HAVE_TRANSPARENCY = useTranslucency;
}

void MainWindow::rememberMenuAccelerators()
{
    foreach(QAction* menuItem, menuBar()->actions()) {
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
    foreach(QAction* menuItem, menuBar()->actions()) {
        menuItem->setText(menuItem->text().replace('&', QString()));
    }
}

void MainWindow::restoreMenuAccelerators()
{
    foreach(QAction* menuItem, menuBar()->actions()) {
        QString itemText = menuItem->data().toString();
        menuItem->setText(itemText);
    }
}

void MainWindow::correctStandardShortcuts()
{
    // replace F1 shortcut for help contents
    QAction* helpAction = actionCollection()->action("help_contents");
    if (helpAction) {
        helpAction->setShortcut(QKeySequence());
    }

    // replace Ctrl+B shortcut for bookmarks only if user hasn't already
    // changed the shortcut; however, if the user changed it to Ctrl+B
    // this will still get changed to Ctrl+Shift+B
    QAction* bookmarkAction = actionCollection()->action("add_bookmark");
    if (bookmarkAction && bookmarkAction->shortcut() == QKeySequence(Qt::CTRL + Qt::Key_B)) {
        bookmarkAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_B));
    }
}

ViewManager* MainWindow::viewManager() const
{
    return _viewManager;
}

void MainWindow::disconnectController(SessionController* controller)
{
    disconnect(controller, &Konsole::SessionController::titleChanged,
               this, &Konsole::MainWindow::activeViewTitleChanged);
    disconnect(controller, &Konsole::SessionController::rawTitleChanged,
               this, &Konsole::MainWindow::updateWindowCaption);

    // KXmlGuiFactory::removeClient() will try to access actions associated
    // with the controller internally, which may not be valid after the controller
    // itself is no longer valid (after the associated session and or view have
    // been destroyed)
    if (controller->isValid())
        guiFactory()->removeClient(controller);

    controller->setSearchBar(0);
}

void MainWindow::activeViewChanged(SessionController* controller)
{
    // associate bookmark menu with current session
    bookmarkHandler()->setActiveView(controller);
    disconnect(bookmarkHandler(), &Konsole::BookmarkHandler::openUrl, 0, 0);
    connect(bookmarkHandler(), &Konsole::BookmarkHandler::openUrl, controller,
            &Konsole::SessionController::openUrl);

    if (_pluggedController)
        disconnectController(_pluggedController);

    Q_ASSERT(controller);
    _pluggedController = controller;

    // listen for title changes from the current session
    connect(controller, &Konsole::SessionController::titleChanged,
            this, &Konsole::MainWindow::activeViewTitleChanged);
    connect(controller, &Konsole::SessionController::rawTitleChanged,
            this, &Konsole::MainWindow::updateWindowCaption);

    controller->setShowMenuAction(_toggleMenuBarAction);
    guiFactory()->addClient(controller);

    // set the current session's search bar
    controller->setSearchBar(searchBar());

    // update session title to match newly activated session
    activeViewTitleChanged(controller);

    // Update window icon to newly activated session's icon
    updateWindowIcon();
}

void MainWindow::activeViewTitleChanged(ViewProperties* properties)
{
    Q_UNUSED(properties);
    updateWindowCaption();
}

void MainWindow::updateWindowCaption()
{
    if (!_pluggedController)
        return;

    const QString& title = _pluggedController->title();
    const QString& userTitle = _pluggedController->userTitle();

    // use tab title as caption by default
    QString caption = title;

    // use window title as caption only when enabled and it is not empty
    if (KonsoleSettings::showWindowTitleOnTitleBar() && !userTitle.isEmpty()) {
        caption = userTitle;
    }

    if (KonsoleSettings::showAppNameOnTitleBar()) {
        setCaption(caption);
    } else {
        setPlainCaption(caption);
    }
}

void MainWindow::updateWindowIcon()
{
    if (_pluggedController)
        setWindowIcon(_pluggedController->icon());
}

IncrementalSearchBar* MainWindow::searchBar() const
{
    return _viewManager->searchBar();
}

void MainWindow::setupActions()
{
    KActionCollection* collection = actionCollection();
    QAction* menuAction = 0;

    // File Menu
    _newTabMenuAction = new KActionMenu(KIcon("tab-new"), i18nc("@action:inmenu", "&New Tab"), collection);
    _newTabMenuAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_T));

#pragma message("TODO: How to port KAction::setShortcutConfigurable()?")
#if 0
    _newTabMenuAction->setShortcutConfigurable(true);
#endif
    _newTabMenuAction->setAutoRepeat(false);
    connect(_newTabMenuAction, &KActionMenu::triggered, this, &Konsole::MainWindow::newTab);
    collection->addAction("new-tab", _newTabMenuAction);

    menuAction = collection->addAction("clone-tab");
    menuAction->setIcon(KIcon("tab-duplicate"));
    menuAction->setText(i18nc("@action:inmenu", "&Clone Tab"));
    menuAction->setShortcut(QKeySequence());
    menuAction->setAutoRepeat(false);
    connect(menuAction, &QAction::triggered, this, &Konsole::MainWindow::cloneTab);

    menuAction = collection->addAction("new-window");
    menuAction->setIcon(KIcon("window-new"));
    menuAction->setText(i18nc("@action:inmenu", "New &Window"));
    menuAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_N));
    menuAction->setAutoRepeat(false);
    connect(menuAction, &QAction::triggered, this, &Konsole::MainWindow::newWindow);

    menuAction = collection->addAction("close-window");
    menuAction->setIcon(KIcon("window-close"));
    menuAction->setText(i18nc("@action:inmenu", "Close Window"));
    menuAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_Q));
    connect(menuAction, &QAction::triggered, this, &Konsole::MainWindow::close);

    // Bookmark Menu
    KActionMenu* bookmarkMenu = new KActionMenu(i18nc("@title:menu", "&Bookmarks"), collection);
    _bookmarkHandler = new BookmarkHandler(collection, bookmarkMenu->menu(), true, this);
    collection->addAction("bookmark", bookmarkMenu);
    connect(_bookmarkHandler, &Konsole::BookmarkHandler::openUrls, this, &Konsole::MainWindow::openUrls);

    // Settings Menu
    _toggleMenuBarAction = KStandardAction::showMenubar(menuBar(), SLOT(setVisible(bool)), collection);
    _toggleMenuBarAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_M));

    // Full Screen
    menuAction = KStandardAction::fullScreen(this, SLOT(viewFullScreen(bool)), this, collection);
    menuAction->setShortcut(QKeySequence(Qt::Key_F11));

    KStandardAction::configureNotifications(this, SLOT(configureNotifications()), collection);
    KStandardAction::keyBindings(this, SLOT(showShortcutsDialog()), collection);
    KStandardAction::preferences(this, SLOT(showSettingsDialog()), collection);

    menuAction = collection->addAction("manage-profiles");
    menuAction->setText(i18nc("@action:inmenu", "Manage Profiles..."));
    menuAction->setIcon(KIcon("configure"));
    connect(menuAction, &QAction::triggered, this, &Konsole::MainWindow::showManageProfilesDialog);

    // Set up an shortcut-only action for activating menu bar.
    menuAction = collection->addAction("activate-menu");
    menuAction->setText(i18nc("@item", "Activate Menu"));
    menuAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_F10));
    connect(menuAction, &QAction::triggered, this, &Konsole::MainWindow::activateMenuBar);
}

void MainWindow::viewFullScreen(bool fullScreen)
{
    if (fullScreen)
        setWindowState(windowState() | Qt::WindowFullScreen);
    else
        setWindowState(windowState() & ~Qt::WindowFullScreen);
}

BookmarkHandler* MainWindow::bookmarkHandler() const
{
    return _bookmarkHandler;
}

void MainWindow::setProfileList(ProfileList* list)
{
    profileListChanged(list->actions());

    connect(list, &Konsole::ProfileList::profileSelected, this,
            &Konsole::MainWindow::newFromProfile);

    connect(list, &Konsole::ProfileList::actionsChanged, this,
            &Konsole::MainWindow::profileListChanged);
}

void MainWindow::profileListChanged(const QList<QAction*>& sessionActions)
{
    // If only 1 profile is to be shown in the menu, only display
    // it if it is the non-default profile.
    if (sessionActions.size() > 2) {
        // Update the 'New Tab' KActionMenu
        if (_newTabMenuAction->menu()) {
            _newTabMenuAction->menu()->clear();
        } else {
            _newTabMenuAction->setMenu(new QMenu());
        }
        foreach(QAction* sessionAction, sessionActions) {
            _newTabMenuAction->menu()->addAction(sessionAction);

            // NOTE: defaultProfile seems to not work here, sigh.
            Profile::Ptr profile = ProfileManager::instance()->defaultProfile();
            if (profile && profile->name() == sessionAction->text().remove('&')) {
                sessionAction->setIcon(KIcon(profile->icon(), 0, QStringList("emblem-favorite")));
                _newTabMenuAction->menu()->setDefaultAction(sessionAction);
                QFont actionFont = sessionAction->font();
                actionFont.setBold(true);
                sessionAction->setFont(actionFont);
            }
        }
    } else {
        if (_newTabMenuAction->menu()) {
            _newTabMenuAction->menu()->clear();
        } else {
           _newTabMenuAction->setMenu(new QMenu());
        }
        Profile::Ptr profile = ProfileManager::instance()->defaultProfile();

        // NOTE: Compare names w/o any '&'
        if (sessionActions.size() == 2 &&  sessionActions[1]->text().remove('&') != profile->name()) {
            _newTabMenuAction->menu()->addAction(sessionActions[1]);
        } else {
            _newTabMenuAction->menu()->deleteLater();
        }
    }
}

QString MainWindow::activeSessionDir() const
{
    if (_pluggedController) {
        if (Session* session = _pluggedController->session()) {
            // For new tabs to get the correct working directory,
            // force the updating of the currentWorkingDirectory.
            session->getDynamicTitle();
        }
        return _pluggedController->currentDir();
    } else {
        return QString();
    }
}

void MainWindow::openUrls(const QList<QUrl>& urls)
{
    Profile::Ptr defaultProfile = ProfileManager::instance()->defaultProfile();

    Q_FOREACH (auto url, urls) {
        if (url.isLocalFile())
            createSession(defaultProfile, url.path());

        else if (url.scheme() == "ssh")
            createSSHSession(defaultProfile, url);
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

    Session* session = _pluggedController->session();
    Profile::Ptr profile = SessionManager::instance()->sessionProfile(session);
    if (profile) {
        createSession(profile, activeSessionDir());
    } else {
        // something must be wrong: every session should be associated with profile
        Q_ASSERT(false);
        newTab();
    }
}

Session* MainWindow::createSession(Profile::Ptr profile, const QString& directory)
{
    if (!profile)
        profile = ProfileManager::instance()->defaultProfile();

    Session* session = SessionManager::instance()->createSession(profile);

    if (!directory.isEmpty() && profile->startInCurrentSessionDir())
        session->setInitialWorkingDirectory(directory);

    session->addEnvironmentEntry(QString("KONSOLE_DBUS_WINDOW=/Windows/%1").arg(_viewManager->managerId()));

    // create view before starting the session process so that the session
    // doesn't suffer a change in terminal size right after the session
    // starts.  Some applications such as GNU Screen and Midnight Commander
    // don't like this happening
    createView(session);

    return session;
}

Session* MainWindow::createSSHSession(Profile::Ptr profile, const QUrl& url)
{
    if (!profile)
        profile = ProfileManager::instance()->defaultProfile();

    Session* session = SessionManager::instance()->createSession(profile);

    QString sshCommand = "ssh ";
    if (url.port() > -1) {
        sshCommand += QString("-p %1 ").arg(url.port());
    }
    if (!url.userName().isEmpty()) {
        sshCommand += (url.userName() + '@');
    }
    if (!url.host().isEmpty()) {
        sshCommand += url.host();
    }

    session->sendText(sshCommand + '\r');

    // create view before starting the session process so that the session
    // doesn't suffer a change in terminal size right after the session
    // starts.  some applications such as GNU Screen and Midnight Commander
    // don't like this happening
    createView(session);

    return session;
}

void MainWindow::createView(Session* session)
{
    _viewManager->createView(session);
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
    if (kapp->sessionSaving()) {
        return true;
    }

    // Check what processes are running,
    // if just the default shell is running don't ask for confirmation

    QStringList processesRunning;
    foreach(Session *session, _viewManager->sessions()) {
        if (!session)
            continue;

        const QString defaultProc = session->program().split('/').last();
        const QString currentProc = session->foregroundProcessName().split('/').last();

        if (currentProc.isEmpty())
            continue;

        if (defaultProc != currentProc) {
            processesRunning.append(currentProc);
        }
    }
    if (processesRunning.count() == 0) {
        return true;
    }

    // NOTE: Some, if not all, of the below KWindowSystem calls are only
    //       implemented under x11 (KDE4.8 kdelibs/kdeui/windowmanagement).

    // make sure the window is shown on current desktop and is not minimized
    KWindowSystem::setOnDesktop(winId(), KWindowSystem::currentDesktop());
    if (isMinimized()) {
        KWindowSystem::unminimizeWindow(winId(), true);
    }

    int result = KMessageBox::warningYesNoCancelList(this,
                 i18ncp("@info", "There is a process running in this window. "
                        "Do you still want to quit?",
                        "There are %1 processes running in this window. "
                        "Do you still want to quit?",
                        processesRunning.count()),
                 processesRunning,
                 i18nc("@title", "Confirm Close"),
                 KGuiItem(i18nc("@action:button", "Close &Window"), "window-close"),
                 KGuiItem(i18nc("@action:button", "Close Current &Tab"), "tab-close"),
                 KStandardGuiItem::cancel(),
                 "CloseAllTabs");

    switch (result) {
    case KMessageBox::Yes:
        return true;
    case KMessageBox::No:
        if (_pluggedController && _pluggedController->session()) {
            disconnectController(_pluggedController);
            _pluggedController->closeSession();
        }
        return false;
    case KMessageBox::Cancel:
        return false;
    }

    return true;
}

void MainWindow::saveProperties(KConfigGroup& group)
{
    _viewManager->saveSessions(group);
}

void MainWindow::readProperties(const KConfigGroup& group)
{
    _viewManager->restoreSessions(group);
}

void MainWindow::saveGlobalProperties(KConfig* config)
{
    SessionManager::instance()->saveSessions(config);
}

void MainWindow::readGlobalProperties(KConfig* config)
{
    SessionManager::instance()->restoreSessions(config);
}

void MainWindow::syncActiveShortcuts(KActionCollection* dest, const KActionCollection* source)
{
    foreach(QAction * qAction, source->actions()) {
        if (KAction* kAction = qobject_cast<KAction*>(qAction)) {
            if (KAction* destKAction = qobject_cast<KAction*>(dest->action(kAction->objectName())))
                destKAction->setShortcut(kAction->shortcut(KAction::ActiveShortcut), KAction::ActiveShortcut);
        }
    }
}
void MainWindow::showShortcutsDialog()
{
    KShortcutsDialog dialog(KShortcutsEditor::AllActions, KShortcutsEditor::LetterShortcutsDisallowed, this);

    // add actions from this window and the current session controller
    foreach(KXMLGUIClient * client, guiFactory()->clients()) {
        dialog.addCollection(client->actionCollection());
    }

    if (dialog.configure()) {
        // sync shortcuts for non-session actions (defined in "konsoleui.rc") in other main windows
        foreach(QWidget* mainWindowWidget, QApplication::topLevelWidgets()) {
            MainWindow* mainWindow = qobject_cast<MainWindow*>(mainWindowWidget);
            if (mainWindow && mainWindow != this)
                syncActiveShortcuts(mainWindow->actionCollection(), actionCollection());
        }
        // sync shortcuts for session actions (defined in "sessionui.rc") in other session controllers.
        // Controllers which are currently plugged in (ie. their actions are part of the current menu)
        // must be updated immediately via syncActiveShortcuts().  Other controllers will be updated
        // when they are plugged into a main window.
        foreach(SessionController * controller, SessionController::allControllers()) {
            controller->reloadXML();
            if (controller->factory() && controller != _pluggedController)
                syncActiveShortcuts(controller->actionCollection(), _pluggedController->actionCollection());
        }
    }
}

void MainWindow::newFromProfile(Profile::Ptr profile)
{
    createSession(profile, activeSessionDir());
}
void MainWindow::showManageProfilesDialog()
{
    ManageProfilesDialog* dialog = new ManageProfilesDialog(this);
    dialog->show();
}

void MainWindow::showSettingsDialog()
{
    if (KConfigDialog::showDialog("settings"))
        return;

    KConfigDialog* settingsDialog = new KConfigDialog(this, "settings", KonsoleSettings::self());
    settingsDialog->setFaceType(KPageDialog::List);

    GeneralSettings* generalSettings = new GeneralSettings(settingsDialog);
    settingsDialog->addPage(generalSettings,
                            i18nc("@title Preferences page name", "General"),
                            "utilities-terminal");

    TabBarSettings* tabBarSettings = new TabBarSettings(settingsDialog);
    settingsDialog->addPage(tabBarSettings,
                            i18nc("@title Preferences page name", "TabBar"),
                            "system-run");

    settingsDialog->show();
}

void MainWindow::applyKonsoleSettings()
{
    setMenuBarInitialVisibility(KonsoleSettings::showMenuBarByDefault());

    if (KonsoleSettings::allowMenuAccelerators()) {
        restoreMenuAccelerators();
    } else {
        removeMenuAccelerators();
    }

    setNavigationVisibility(KonsoleSettings::tabBarVisibility());
    setNavigationPosition(KonsoleSettings::tabBarPosition());
    setNavigationBehavior(KonsoleSettings::newTabBehavior());
    setShowQuickButtons(KonsoleSettings::showQuickButtons());

    if (KonsoleSettings::tabBarUseUserStyleSheet()) {
        setNavigationStyleSheetFromFile(KonsoleSettings::tabBarUserStyleSheetFile());
    } else {
        // Apply default values
        setNavigationStyleSheet(KonsoleSettings::tabBarStyleSheet());
    }

    setAutoSaveSettings("MainWindow", KonsoleSettings::saveGeometryOnExit());

    updateWindowCaption();
}

void MainWindow::setNavigationVisibility(int visibility)
{
    _viewManager->setNavigationVisibility(visibility);
}

void MainWindow::setNavigationPosition(int position)
{
    _viewManager->setNavigationPosition(position);
}

void MainWindow::setNavigationStyleSheet(const QString& styleSheet)
{
    _viewManager->setNavigationStyleSheet(styleSheet);
}

void MainWindow::setNavigationBehavior(int behavior)
{
    _viewManager->setNavigationBehavior(behavior);
}

void MainWindow::setNavigationStyleSheetFromFile(const QUrl& styleSheetFile)
{
    // Let's only deal w/ local files for now
    if (!styleSheetFile.isLocalFile()) {
        setNavigationStyleSheet(KonsoleSettings::tabBarStyleSheet());
    }

    QFile file(styleSheetFile.toLocalFile());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setNavigationStyleSheet(KonsoleSettings::tabBarStyleSheet());
    }

    QString styleSheetText;
    QTextStream in(&file);
    while (!in.atEnd()) {
        styleSheetText.append(in.readLine());
    }

    // Replace current style sheet w/ loaded file
    setNavigationStyleSheet(styleSheetText);
}

void MainWindow::setShowQuickButtons(bool show)
{
    _viewManager->setShowQuickButtons(show);
}

void MainWindow::activateMenuBar()
{
    const QList<QAction*> menuActions = menuBar()->actions();

    if (menuActions.isEmpty())
        return;

    // Show menubar if it is hidden at the moment
    if (menuBar()->isHidden()) {
        menuBar()->setVisible(true);
        _toggleMenuBarAction->setChecked(true);
    }

    // First menu action should be 'File'
    QAction* menuAction = menuActions.first();

    // TODO: Handle when menubar is top level (MacOS)
    menuBar()->setActiveAction(menuAction);
}

void MainWindow::setupMainWidget()
{
    QWidget* mainWindowWidget = new QWidget(this);
    QVBoxLayout* mainWindowLayout = new QVBoxLayout();

    mainWindowLayout->addWidget(_viewManager->widget());
    mainWindowLayout->setContentsMargins(0, 0, 0, 0);
    mainWindowLayout->setSpacing(0);

    mainWindowWidget->setLayout(mainWindowLayout);

    setCentralWidget(mainWindowWidget);
}

void MainWindow::configureNotifications()
{
    KNotifyConfigWidget::configure(this);
}

void MainWindow::setMenuBarInitialVisibility(bool visible)
{
    _menuBarInitialVisibility = visible;
}
void MainWindow::showEvent(QShowEvent* aEvent)
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
    }

    // Call parent method
    KXmlGuiWindow::showEvent(aEvent);
}

bool MainWindow::focusNextPrevChild(bool)
{
    // In stand-alone konsole, always disable implicit focus switching
    // through 'Tab' and 'Shift+Tab'
    //
    // Kpart is another different story
    return false;
}


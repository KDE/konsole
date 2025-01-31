/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "ViewManager.h"

#include "config-konsole.h"

// Qt
#include <QFile>
#include <QFileDialog>
#include <QStringList>
#include <QTabBar>

#include <QJsonArray>
#include <QJsonDocument>

#if HAVE_DBUS
#include <QDBusArgument>
#include <QDBusMetaType>
#endif

// KDE
#include <KActionCollection>
#include <KActionMenu>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>

// Konsole
#if HAVE_DBUS
#include <windowadaptor.h>
#endif

#include "colorscheme/ColorScheme.h"
#include "colorscheme/ColorSchemeManager.h"

#include "profile/ProfileManager.h"

#include "session/Session.h"
#include "session/SessionController.h"
#include "session/SessionManager.h"

#include "terminalDisplay/TerminalDisplay.h"
#include "widgets/ViewContainer.h"
#include "widgets/ViewSplitter.h"

using namespace Konsole;

int ViewManager::lastManagerId = 0;

Q_DECLARE_METATYPE(QList<double>);

ViewManager::ViewManager(QObject *parent, KActionCollection *collection)
    : QObject(parent)
    , _viewContainer(nullptr)
    , _pluggedController(nullptr)
    , _sessionMap(QHash<TerminalDisplay *, Session *>())
    , _actionCollection(collection)
    , _navigationMethod(TabbedNavigation)
    , _navigationVisibility(NavigationNotSet)
    , _managerId(0)
    , _terminalDisplayHistoryIndex(-1)
{
#if HAVE_DBUS
    qDBusRegisterMetaType<QList<double>>();
#endif

    _viewContainer = createContainer();
    // setup actions which are related to the views
    setupActions();

    /* TODO: Reconnect
    // emit a signal when all of the views held by this view manager are destroyed
    */
    connect(_viewContainer.data(), &Konsole::TabbedViewContainer::empty, this, &Konsole::ViewManager::empty);

    // listen for profile changes
    connect(ProfileManager::instance(), &Konsole::ProfileManager::profileChanged, this, &Konsole::ViewManager::profileChanged);
    connect(SessionManager::instance(), &Konsole::SessionManager::sessionUpdated, this, &Konsole::ViewManager::updateViewsForSession);

    _managerId = ++lastManagerId;

#if HAVE_DBUS
    // prepare DBus communication
    new WindowAdaptor(this);
    QDBusConnection::sessionBus().registerObject(QLatin1String("/Windows/") + QString::number(_managerId), this);
#endif
}

ViewManager::~ViewManager() = default;

int ViewManager::managerId() const
{
    return _managerId;
}

QWidget *ViewManager::activeView() const
{
    return _viewContainer->currentWidget();
}

QWidget *ViewManager::widget() const
{
    return _viewContainer;
}

void ViewManager::setupActions()
{
    Q_ASSERT(_actionCollection);
    if (_actionCollection == nullptr) {
        return;
    }

    KActionCollection *collection = _actionCollection;
    KActionMenu *splitViewActions =
        new KActionMenu(QIcon::fromTheme(QStringLiteral("view-split-left-right")), i18nc("@action:inmenu", "Split View"), collection);
    splitViewActions->setPopupMode(QToolButton::InstantPopup);
    collection->addAction(QStringLiteral("split-view"), splitViewActions);

    // Let's reuse the pointer, no need not to.
    auto *action = new QAction(this);
    action->setIcon(QIcon::fromTheme(QStringLiteral("view-split-left-right")));
    action->setText(i18nc("@action:inmenu", "Split View Left/Right"));
    connect(action, &QAction::triggered, this, &ViewManager::splitLeftRight);
    collection->addAction(QStringLiteral("split-view-left-right"), action);
    collection->setDefaultShortcut(action, QKeySequence(Qt::CTRL | Qt::Key_ParenLeft));
    splitViewActions->addAction(action);

    action = new QAction(this);
    action->setIcon(QIcon::fromTheme(QStringLiteral("view-split-top-bottom")));
    action->setText(i18nc("@action:inmenu", "Split View Top/Bottom"));
    connect(action, &QAction::triggered, this, &ViewManager::splitTopBottom);
    collection->setDefaultShortcut(action, QKeySequence(Qt::CTRL | Qt::Key_ParenRight));
    collection->addAction(QStringLiteral("split-view-top-bottom"), action);
    splitViewActions->addAction(action);

    action = new QAction(this);
    action->setIcon(QIcon::fromTheme(QStringLiteral("view-split-auto")));
    action->setText(i18nc("@action:inmenu", "Split View Automatically"));
    connect(action, &QAction::triggered, this, &ViewManager::splitAuto);
    collection->setDefaultShortcut(action, QKeySequence(Qt::CTRL | Qt::Key_Asterisk));
    collection->addAction(QStringLiteral("split-view-auto"), action);
    splitViewActions->addAction(action);

    splitViewActions->addSeparator();

    action = new QAction(this);
    action->setIcon(QIcon::fromTheme(QStringLiteral("view-split-left-right")));
    action->setText(i18nc("@action:inmenu", "Split View Left/Right from next tab"));
    connect(action, &QAction::triggered, this, &ViewManager::splitLeftRightNextTab);
    collection->addAction(QStringLiteral("split-view-left-right-next-tab"), action);
    splitViewActions->addAction(action);
    _multiTabOnlyActions << action;

    action = new QAction(this);
    action->setIcon(QIcon::fromTheme(QStringLiteral("view-split-top-bottom")));
    action->setText(i18nc("@action:inmenu", "Split View Top/Bottom from next tab"));
    connect(action, &QAction::triggered, this, &ViewManager::splitTopBottomNextTab);
    collection->addAction(QStringLiteral("split-view-top-bottom-next-tab"), action);
    splitViewActions->addAction(action);
    _multiTabOnlyActions << action;

    action = new QAction(this);
    action->setIcon(QIcon::fromTheme(QStringLiteral("view-split-auto")));
    action->setText(i18nc("@action:inmenu", "Split View Automatically from next tab"));
    connect(action, &QAction::triggered, this, &ViewManager::splitAutoNextTab);
    collection->addAction(QStringLiteral("split-view-auto-next-tab"), action);
    splitViewActions->addAction(action);
    _multiTabOnlyActions << action;

    splitViewActions->addSeparator();

    action = new QAction(this);
    action->setIcon(QIcon::fromTheme(QStringLiteral("view-split-top-bottom")));
    action->setText(i18nc("@action:inmenu", "Load a new tab with layout 2x2 terminals"));
    connect(action, &QAction::triggered, this, [this]() {
        this->loadLayout(QStringLiteral(":/konsole/layouts/2x2-terminals.json"));
    });
    collection->addAction(QStringLiteral("load-terminals-layout-2x2"), action);
    splitViewActions->addAction(action);

    action = new QAction(this);
    action->setIcon(QIcon::fromTheme(QStringLiteral("view-split-left-right")));
    action->setText(i18nc("@action:inmenu", "Load a new tab with layout 2x1 terminals"));
    connect(action, &QAction::triggered, this, [this]() {
        this->loadLayout(QStringLiteral(":/konsole/layouts/2x1-terminals.json"));
    });
    collection->addAction(QStringLiteral("load-terminals-layout-2x1"), action);
    splitViewActions->addAction(action);

    action = new QAction(this);
    action->setIcon(QIcon::fromTheme(QStringLiteral("view-split-top-bottom")));
    action->setText(i18nc("@action:inmenu", "Load a new tab with layout 1x2 terminals"));
    connect(action, &QAction::triggered, this, [this]() {
        this->loadLayout(QStringLiteral(":/konsole/layouts/1x2-terminals.json"));
    });
    collection->addAction(QStringLiteral("load-terminals-layout-1x2"), action);
    splitViewActions->addAction(action);

    action = new QAction(this);
    action->setText(i18nc("@action:inmenu", "Expand View"));
    action->setEnabled(false);
    connect(action, &QAction::triggered, this, &ViewManager::expandActiveContainer);
    collection->setDefaultShortcut(action, Konsole::ACCEL | Qt::Key_BracketRight);
    collection->addAction(QStringLiteral("expand-active-view"), action);
    _multiSplitterOnlyActions << action;

    action = new QAction(this);
    action->setText(i18nc("@action:inmenu", "Shrink View"));
    collection->setDefaultShortcut(action, Konsole::ACCEL | Qt::Key_BracketLeft);
    action->setEnabled(false);
    collection->addAction(QStringLiteral("shrink-active-view"), action);
    connect(action, &QAction::triggered, this, &ViewManager::shrinkActiveContainer);
    _multiSplitterOnlyActions << action;

    action = collection->addAction(QStringLiteral("detach-view"));
    action->setEnabled(true);
    action->setIcon(QIcon::fromTheme(QStringLiteral("tab-detach")));
    action->setText(i18nc("@action:inmenu", "Detach Current &View"));

    connect(action, &QAction::triggered, this, &ViewManager::detachActiveView);
    _multiSplitterOnlyActions << action;

    // Ctrl+Shift+D is not used as a shortcut by default because it is too close
    // to Ctrl+D - which will terminate the session in many cases
    collection->setDefaultShortcut(action, Konsole::ACCEL | Qt::Key_H);

    action = collection->addAction(QStringLiteral("detach-tab"));
    action->setEnabled(true);
    action->setIcon(QIcon::fromTheme(QStringLiteral("tab-detach")));
    action->setText(i18nc("@action:inmenu", "Detach Current &Tab"));
    connect(action, &QAction::triggered, this, &ViewManager::detachActiveTab);
    _multiTabOnlyActions << action;

    // keyboard shortcut only actions
    action = new QAction(i18nc("@action Shortcut entry", "Next Tab"), this);
    const QList<QKeySequence> nextViewActionKeys{QKeySequence{Qt::SHIFT | Qt::Key_Right}, QKeySequence{Qt::CTRL | Qt::Key_PageDown}};
    collection->setDefaultShortcuts(action, nextViewActionKeys);
    collection->addAction(QStringLiteral("next-tab"), action);
    connect(action, &QAction::triggered, this, &ViewManager::nextView);
    _multiTabOnlyActions << action;
    // _viewSplitter->addAction(nextViewAction);

    action = new QAction(i18nc("@action Shortcut entry", "Previous Tab"), this);
    const QList<QKeySequence> previousViewActionKeys{QKeySequence{Qt::SHIFT | Qt::Key_Left}, QKeySequence{Qt::CTRL | Qt::Key_PageUp}};
    collection->setDefaultShortcuts(action, previousViewActionKeys);
    collection->addAction(QStringLiteral("previous-tab"), action);
    connect(action, &QAction::triggered, this, &ViewManager::previousView);
    _multiTabOnlyActions << action;
    // _viewSplitter->addAction(previousViewAction);

    action = new QAction(i18nc("@action Shortcut entry", "Focus Above Terminal"), this);
    connect(action, &QAction::triggered, this, &ViewManager::focusUp);
    collection->addAction(QStringLiteral("focus-view-above"), action);
    collection->setDefaultShortcut(action, Qt::CTRL | Qt::SHIFT | Qt::Key_Up);
    _viewContainer->addAction(action);
    _multiSplitterOnlyActions << action;

    action = new QAction(i18nc("@action Shortcut entry", "Focus Below Terminal"), this);
    collection->setDefaultShortcut(action, Qt::CTRL | Qt::SHIFT | Qt::Key_Down);
    collection->addAction(QStringLiteral("focus-view-below"), action);
    connect(action, &QAction::triggered, this, &ViewManager::focusDown);
    _multiSplitterOnlyActions << action;
    _viewContainer->addAction(action);

    action = new QAction(i18nc("@action Shortcut entry", "Focus Left Terminal"), this);
    collection->setDefaultShortcut(action, Qt::CTRL | Qt::SHIFT | Qt::Key_Left);
    connect(action, &QAction::triggered, this, &ViewManager::focusLeft);
    collection->addAction(QStringLiteral("focus-view-left"), action);
    _multiSplitterOnlyActions << action;

    action = new QAction(i18nc("@action Shortcut entry", "Focus Right Terminal"), this);
    collection->setDefaultShortcut(action, Qt::CTRL | Qt::SHIFT | Qt::Key_Right);
    connect(action, &QAction::triggered, this, &ViewManager::focusRight);
    collection->addAction(QStringLiteral("focus-view-right"), action);
    _multiSplitterOnlyActions << action;

    action = new QAction(i18nc("@action Shortcut entry", "Switch to Last Tab"), this);
    connect(action, &QAction::triggered, this, &ViewManager::lastView);
    collection->addAction(QStringLiteral("last-tab"), action);
    _multiTabOnlyActions << action;

    action = new QAction(i18nc("@action Shortcut entry", "Last Used Tabs"), this);
    connect(action, &QAction::triggered, this, &ViewManager::lastUsedView);
    collection->setDefaultShortcut(action, QKeySequence(Qt::CTRL | Qt::Key_Tab));
    collection->addAction(QStringLiteral("last-used-tab"), action);

    action = new QAction(i18nc("@action Shortcut entry", "Toggle Between Two Tabs"), this);
    connect(action, &QAction::triggered, this, &Konsole::ViewManager::toggleTwoViews);
    collection->addAction(QStringLiteral("toggle-two-tabs"), action);
    _multiTabOnlyActions << action;

    action = new QAction(i18nc("@action Shortcut entry", "Last Used Tabs (Reverse)"), this);
    collection->addAction(QStringLiteral("last-used-tab-reverse"), action);
    collection->setDefaultShortcut(action, Qt::CTRL | Qt::SHIFT | Qt::Key_Tab);
    connect(action, &QAction::triggered, this, &ViewManager::lastUsedViewReverse);

    action = new QAction(i18nc("@action Shortcut entry", "Toggle maximize current view"), this);
    action->setText(i18nc("@action:inmenu", "Toggle maximize current view"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("view-fullscreen")));
    collection->addAction(QStringLiteral("toggle-maximize-current-view"), action);
    collection->setDefaultShortcut(action, Qt::CTRL | Qt::SHIFT | Qt::Key_E);
    connect(action, &QAction::triggered, _viewContainer, &TabbedViewContainer::toggleMaximizeCurrentTerminal);
    _multiSplitterOnlyActions << action;
    _viewContainer->addAction(action);

    action = new QAction(i18nc("@action Shortcut entry", "Toggle zoom-maximize current view"), this);
    action->setText(i18nc("@action:inmenu", "Toggle zoom-maximize current view"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("view-fullscreen")));
    collection->addAction(QStringLiteral("toggle-zoom-current-view"), action);
    collection->setDefaultShortcut(action, Qt::CTRL | Qt::SHIFT | Qt::Key_Z);
    connect(action, &QAction::triggered, _viewContainer, &TabbedViewContainer::toggleZoomMaximizeCurrentTerminal);
    _multiSplitterOnlyActions << action;
    _viewContainer->addAction(action);

    action = new QAction(i18nc("@action Shortcut entry", "Move tab to the right"), this);
    collection->addAction(QStringLiteral("move-tab-to-right"), action);
    collection->setDefaultShortcut(action, Qt::CTRL | Qt::ALT | Qt::Key_Right);
    connect(action, &QAction::triggered, _viewContainer, &TabbedViewContainer::moveTabRight);
    _multiTabOnlyActions << action;
    _viewContainer->addAction(action);

    action = new QAction(i18nc("@action Shortcut entry", "Move tab to the left"), this);
    collection->addAction(QStringLiteral("move-tab-to-left"), action);
    collection->setDefaultShortcut(action, Qt::CTRL | Qt::ALT | Qt::Key_Left);
    connect(action, &QAction::triggered, _viewContainer, &TabbedViewContainer::moveTabLeft);
    _multiTabOnlyActions << action;
    _viewContainer->addAction(action);

    action = new QAction(i18nc("@action Shortcut entry", "Setup semantic integration (bash)"), this);
    collection->addAction(QStringLiteral("semantic-setup-bash"), action);
    collection->setDefaultShortcut(action, Qt::CTRL | Qt::ALT | Qt::Key_BracketRight);
    connect(action, &QAction::triggered, this, &ViewManager::semanticSetupBash);
    _viewContainer->addAction(action);

    action = new QAction(i18nc("@action Shortcut entry", "Toggle semantic hints display"), this);
    collection->addAction(QStringLiteral("toggle-semantic-hints"), action);
    collection->setDefaultShortcut(action, Qt::CTRL | Qt::ALT | Qt::Key_BracketLeft);
    connect(action, &QAction::triggered, this, &ViewManager::toggleSemanticHints);
    _viewContainer->addAction(action);

    action = new QAction(i18nc("@action Shortcut entry", "Toggle line numbers display"), this);
    collection->addAction(QStringLiteral("toggle-line-numbers"), action);
    collection->setDefaultShortcut(action, Qt::CTRL | Qt::ALT | Qt::Key_Backslash);
    connect(action, &QAction::triggered, this, &ViewManager::toggleLineNumbers);
    _viewContainer->addAction(action);

    action = new QAction(this);
    action->setText(i18nc("@action:inmenu", "Equal size to all views"));
    collection->setDefaultShortcut(action, Konsole::ACCEL | Qt::SHIFT | Qt::Key_Backslash);
    action->setEnabled(false);
    collection->addAction(QStringLiteral("equal-size-view"), action);
    connect(action, &QAction::triggered, this, &ViewManager::equalSizeAllContainers);
    _multiSplitterOnlyActions << action;

    // _viewSplitter->addAction(lastUsedViewReverseAction);
    const int SWITCH_TO_TAB_COUNT = 19;
    for (int i = 0; i < SWITCH_TO_TAB_COUNT; ++i) {
        action = new QAction(i18nc("@action Shortcut entry", "Switch to Tab %1", i + 1), this);
        connect(action, &QAction::triggered, this, [this, i]() {
            switchToView(i);
        });
        collection->addAction(QStringLiteral("switch-to-tab-%1").arg(i), action);
        _multiTabOnlyActions << action;

        // only add default shortcut bindings for the first 10 tabs, regardless of SWITCH_TO_TAB_COUNT
        if (i < 9) {
            collection->setDefaultShortcut(action, QStringLiteral("Alt+%1").arg(i + 1));
        } else if (i == 9) {
            // add shortcut for 10th tab
            collection->setDefaultShortcut(action, QKeySequence(Qt::ALT | Qt::Key_0));
        }
    }

    connect(_viewContainer, &TabbedViewContainer::viewAdded, this, &ViewManager::toggleActionsBasedOnState);
    connect(_viewContainer, &QTabWidget::currentChanged, this, &ViewManager::toggleActionsBasedOnState);
    // Let the view container remove the view before counting tabs
    connect(_viewContainer, &TabbedViewContainer::viewRemoved, this, &ViewManager::toggleActionsBasedOnState);

    toggleActionsBasedOnState();
}

void ViewManager::toggleActionsBasedOnState()
{
    const int count = _viewContainer->count();
    for (QAction *tabOnlyAction : std::as_const(_multiTabOnlyActions)) {
        tabOnlyAction->setEnabled(count > 1);
    }

    if ((_viewContainer != nullptr) && (_viewContainer->activeViewSplitter() != nullptr)) {
        const int splitCount = _viewContainer->activeViewSplitter()->getToplevelSplitter()->findChildren<TerminalDisplay *>().count();

        for (QAction *action : std::as_const(_multiSplitterOnlyActions)) {
            action->setEnabled(splitCount > 1);
        }
    }
}

void ViewManager::switchToView(int index)
{
    _viewContainer->setCurrentIndex(index);
}

void ViewManager::switchToTerminalDisplay(Konsole::TerminalDisplay *terminalDisplay)
{
    auto splitter = qobject_cast<ViewSplitter *>(terminalDisplay->parentWidget());
    auto toplevelSplitter = splitter->getToplevelSplitter();

    // Focus the TermialDisplay
    terminalDisplay->setFocus();

    if (_viewContainer->currentWidget() != toplevelSplitter) {
        // Focus the tab
        switchToView(_viewContainer->indexOf(toplevelSplitter));
    }
}

void ViewManager::focusUp()
{
    _viewContainer->activeViewSplitter()->focusUp();
}

void ViewManager::focusDown()
{
    _viewContainer->activeViewSplitter()->focusDown();
}

void ViewManager::focusLeft()
{
    _viewContainer->activeViewSplitter()->focusLeft();
}

void ViewManager::focusRight()
{
    _viewContainer->activeViewSplitter()->focusRight();
}

void ViewManager::moveActiveViewLeft()
{
    _viewContainer->moveActiveView(TabbedViewContainer::MoveViewLeft);
}

void ViewManager::moveActiveViewRight()
{
    _viewContainer->moveActiveView(TabbedViewContainer::MoveViewRight);
}

void ViewManager::nextContainer()
{
    //    _viewSplitter->activateNextContainer();
}

void ViewManager::nextView()
{
    _viewContainer->activateNextView();
}

void ViewManager::previousView()
{
    _viewContainer->activatePreviousView();
}

void ViewManager::lastView()
{
    _viewContainer->activateLastView();
}

void ViewManager::activateLastUsedView(bool reverse)
{
    if (_terminalDisplayHistory.count() <= 1) {
        return;
    }

    if (_terminalDisplayHistoryIndex == -1) {
        _terminalDisplayHistoryIndex = reverse ? _terminalDisplayHistory.count() - 1 : 1;
    } else if (reverse) {
        if (_terminalDisplayHistoryIndex == 0) {
            _terminalDisplayHistoryIndex = _terminalDisplayHistory.count() - 1;
        } else {
            _terminalDisplayHistoryIndex--;
        }
    } else {
        if (_terminalDisplayHistoryIndex >= _terminalDisplayHistory.count() - 1) {
            _terminalDisplayHistoryIndex = 0;
        } else {
            _terminalDisplayHistoryIndex++;
        }
    }

    switchToTerminalDisplay(_terminalDisplayHistory[_terminalDisplayHistoryIndex]);
}

void ViewManager::lastUsedView()
{
    activateLastUsedView(false);
}

void ViewManager::lastUsedViewReverse()
{
    activateLastUsedView(true);
}

void ViewManager::toggleTwoViews()
{
    if (_terminalDisplayHistory.count() <= 1) {
        return;
    }

    switchToTerminalDisplay(_terminalDisplayHistory.at(1));
}

void ViewManager::detachActiveView()
{
    // find the currently active view and remove it from its container
    if ((_viewContainer->findChildren<TerminalDisplay *>()).count() > 1) {
        auto activeSplitter = _viewContainer->activeViewSplitter();
        activeSplitter->clearMaximized();
        auto terminal = activeSplitter->activeTerminalDisplay();
        auto newSplitter = new ViewSplitter();
        newSplitter->addTerminalDisplay(terminal, Qt::Horizontal);
        QHash<TerminalDisplay *, Session *> detachedSessions = forgetAll(newSplitter);
        Q_EMIT terminalsDetached(newSplitter, detachedSessions);
        focusAnotherTerminal(activeSplitter->getToplevelSplitter());
        toggleActionsBasedOnState();
    }
}

void ViewManager::detachActiveTab()
{
    if (_viewContainer->count() < 2) {
        return;
    }
    const int currentIdx = _viewContainer->currentIndex();
    detachTab(currentIdx);
}

void ViewManager::detachTab(int tabIdx)
{
    ViewSplitter *splitter = _viewContainer->viewSplitterAt(tabIdx);
    QHash<TerminalDisplay *, Session *> detachedSessions = forgetAll(_viewContainer->viewSplitterAt(tabIdx));
    Q_EMIT terminalsDetached(splitter, detachedSessions);
}

void ViewManager::semanticSetupBash()
{
    int currentSessionId = currentSession();
    // At least one display/session exists if we are splitting
    Q_ASSERT(currentSessionId >= 0);

    Session *activeSession = SessionManager::instance()->idToSession(currentSessionId);
    Q_ASSERT(activeSession);

    activeSession->sendTextToTerminal(QStringLiteral(R"(if [[ ! $PS1 =~ 133 ]] ; then
        PS1='\[\e]133;L\a\]\[\e]133;D;$?\]\[\e]133;A\a\]'$PS1'\[\e]133;B\a\]' ;
        PS2='\[\e]133;A\a\]'$PS2'\[\e]133;B\a\]' ;
        PS0='\[\e]133;C\a\]' ; fi)"),
                                      QChar());
}

void ViewManager::toggleSemanticHints()
{
    int currentSessionId = currentSession();
    Q_ASSERT(currentSessionId >= 0);
    Session *activeSession = SessionManager::instance()->idToSession(currentSessionId);
    Q_ASSERT(activeSession);
    auto profile = SessionManager::instance()->sessionProfile(activeSession);

    profile->setProperty(Profile::SemanticHints, (profile->semanticHints() + 1) % 3);

    auto activeTerminalDisplay = _viewContainer->activeViewSplitter()->activeTerminalDisplay();
    const char *names[3] = {"Never", "Sometimes", "Always"};
    activeTerminalDisplay->showNotification(i18n("Semantic hints ") + i18n(names[profile->semanticHints()]));
    activeTerminalDisplay->update();
}

void ViewManager::toggleLineNumbers()
{
    int currentSessionId = currentSession();
    Q_ASSERT(currentSessionId >= 0);
    Session *activeSession = SessionManager::instance()->idToSession(currentSessionId);
    Q_ASSERT(activeSession);
    auto profile = SessionManager::instance()->sessionProfile(activeSession);

    profile->setProperty(Profile::LineNumbers, (profile->lineNumbers() + 1) % 3);

    auto activeTerminalDisplay = _viewContainer->activeViewSplitter()->activeTerminalDisplay();
    const char *names[3] = {"Never", "Sometimes", "Always"};
    activeTerminalDisplay->showNotification(i18n("Line numbers ") + i18n(names[profile->lineNumbers()]));
    activeTerminalDisplay->update();
}

QHash<TerminalDisplay *, Session *> ViewManager::forgetAll(ViewSplitter *splitter)
{
    splitter->setParent(nullptr);
    QHash<TerminalDisplay *, Session *> detachedSessions;
    const QList<TerminalDisplay *> displays = splitter->findChildren<TerminalDisplay *>();
    for (TerminalDisplay *terminal : displays) {
        Session *session = forgetTerminal(terminal);
        detachedSessions[terminal] = session;
    }
    return detachedSessions;
}

Session *ViewManager::forgetTerminal(TerminalDisplay *terminal)
{
    unregisterTerminal(terminal);

    removeController(terminal->sessionController());
    auto session = _sessionMap.take(terminal);
    if (session != nullptr) {
        disconnect(session, &Konsole::Session::finished, this, &Konsole::ViewManager::sessionFinished);
    }
    _viewContainer->disconnectTerminalDisplay(terminal);
    updateTerminalDisplayHistory(terminal, true);
    return session;
}

Session *ViewManager::createSession(const Profile::Ptr &profile, const QString &directory)
{
    Session *session = SessionManager::instance()->createSession(profile);
    Q_ASSERT(session);
    if (!directory.isEmpty()) {
        session->setInitialWorkingDirectory(directory);
    }
    session->addEnvironmentEntry(QStringLiteral("KONSOLE_DBUS_WINDOW=/Windows/%1").arg(managerId()));
    return session;
}

void ViewManager::sessionFinished(Session *session)
{
    // if this slot is called after the view manager's main widget
    // has been destroyed, do nothing
    if (_viewContainer.isNull()) {
        return;
    }

    if (_navigationMethod == TabbedNavigation) {
        // The last session/tab, and only one view (no splits), emit empty()
        // so that close() is called in MainWindow, fixes #432077
        if (_viewContainer->count() == 1 && _viewContainer->currentTabViewCount() == 1) {
            Q_EMIT empty();
            return;
        }
    }

    Q_ASSERT(session);

    auto view = _sessionMap.key(session);
    _sessionMap.remove(view);

    if (SessionManager::instance()->isClosingAllSessions()) {
        return;
    }

    // Before deleting the view, let's unmaximize if it's maximized.
    auto *splitter = qobject_cast<ViewSplitter *>(view->parentWidget());
    if (splitter == nullptr) {
        return;
    }
    splitter->clearMaximized();

    view->deleteLater();
    connect(view, &QObject::destroyed, this, [this]() {
        toggleActionsBasedOnState();
    });

    // Only remove the controller from factory() if it's actually controlling
    // the session from the sender.
    // This fixes BUG: 348478 - messed up menus after a detached tab is closed
    if ((!_pluggedController.isNull()) && (_pluggedController->session() == session)) {
        // This is needed to remove this controller from factory() in
        // order to prevent BUG: 185466 - disappearing menu popup
        Q_EMIT unplugController(_pluggedController);
    }

    if (!_sessionMap.empty()) {
        updateTerminalDisplayHistory(view, true);
        focusAnotherTerminal(splitter->getToplevelSplitter());
    }
}

void ViewManager::focusAnotherTerminal(ViewSplitter *toplevelSplitter)
{
    auto tabTterminalDisplays = toplevelSplitter->findChildren<TerminalDisplay *>();
    if (tabTterminalDisplays.count() == 0) {
        return;
    }

    if (tabTterminalDisplays.count() > 1) {
        // Give focus to the last used terminal in this tab
        for (auto *historyItem : _terminalDisplayHistory) {
            for (auto *terminalDisplay : tabTterminalDisplays) {
                if (terminalDisplay == historyItem) {
                    terminalDisplay->setFocus(Qt::OtherFocusReason);
                    return;
                }
            }
        }
    }

    if (_terminalDisplayHistory.count() >= 1) {
        // Give focus to the last used terminal tab
        switchToTerminalDisplay(_terminalDisplayHistory[0]);
    }
}

void ViewManager::activateView(TerminalDisplay *view)
{
    if (view) {
        // focus the activated view, this will cause the SessionController
        // to notify the world that the view has been focused and the appropriate UI
        // actions will be plugged in.
        view->setFocus(Qt::OtherFocusReason);
    }
}

void ViewManager::splitLeftRight()
{
    splitView(Qt::Horizontal);
}

void ViewManager::splitTopBottom()
{
    splitView(Qt::Vertical);
}

void ViewManager::splitAuto(bool fromNextTab)
{
    Qt::Orientation orientation;
    auto activeTerminalDisplay = _viewContainer->activeViewSplitter()->activeTerminalDisplay();
    if (activeTerminalDisplay->width() > activeTerminalDisplay->height()) {
        orientation = Qt::Horizontal;
    } else {
        orientation = Qt::Vertical;
    }
    splitView(orientation, fromNextTab);
}

void ViewManager::splitLeftRightNextTab()
{
    splitView(Qt::Horizontal, true);
}

void ViewManager::splitTopBottomNextTab()
{
    splitView(Qt::Vertical, true);
}

void ViewManager::splitAutoNextTab()
{
    splitAuto(true);
}

void ViewManager::splitView(Qt::Orientation orientation, bool fromNextTab)
{
    TerminalDisplay *terminalDisplay;
    TerminalDisplay *focused;
    if (fromNextTab) {
        int tabId = _viewContainer->indexOf(_viewContainer->activeViewSplitter());
        auto nextTab = _viewContainer->viewSplitterAt(tabId + 1);

        if (!nextTab) {
            return;
        }
        terminalDisplay = nextTab->activeTerminalDisplay();
        focused = _viewContainer->activeViewSplitter()->activeTerminalDisplay();
    } else {
        int currentSessionId = currentSession();
        // At least one display/session exists if we are splitting
        Q_ASSERT(currentSessionId >= 0);

        Session *activeSession = SessionManager::instance()->idToSession(currentSessionId);
        Q_ASSERT(activeSession);

        auto profile = SessionManager::instance()->sessionProfile(activeSession);

        const QString directory = profile->startInCurrentSessionDir() ? activeSession->currentWorkingDirectory() : QString();
        auto *session = createSession(profile, directory);

        focused = terminalDisplay = createView(session);
        Q_EMIT activeViewChanged(activeViewController());
    }

    _viewContainer->splitView(terminalDisplay, orientation);

    toggleActionsBasedOnState();

    // focus the new container if created, else keep the currently focused view
    focused->setFocus();
}

void ViewManager::expandActiveContainer()
{
    _viewContainer->activeViewSplitter()->adjustActiveTerminalDisplaySize(10);
}

void ViewManager::shrinkActiveContainer()
{
    _viewContainer->activeViewSplitter()->adjustActiveTerminalDisplaySize(-10);
}

void ViewManager::equalSizeAllContainers()
{
    std::function<void(ViewSplitter *)> processChildren = [&processChildren](ViewSplitter *viewSplitter) -> void {
        // divide the size of the parent widget by the amount of children splits
        auto hintSize = viewSplitter->size();
        auto sizes = viewSplitter->sizes();
        auto sharedSize = hintSize / sizes.size();
        if (viewSplitter->orientation() == Qt::Horizontal) {
            for (auto &&size : sizes) {
                size = sharedSize.width();
            }
        } else {
            for (auto &&size : sizes) {
                size = sharedSize.height();
            }
        }
        // set new sizes
        viewSplitter->setSizes(sizes);

        // set equal sizes for each splitter children
        for (auto &&child : viewSplitter->children()) {
            auto childViewSplitter = qobject_cast<ViewSplitter *>(child);
            if (childViewSplitter) {
                processChildren(childViewSplitter);
            }
        }
    };
    processChildren(_viewContainer->activeViewSplitter()->getToplevelSplitter());
}

SessionController *ViewManager::createController(Session *session, TerminalDisplay *view)
{
    // create a new controller for the session, and ensure that this view manager
    // is notified when the view gains the focus
    auto controller = new SessionController(session, view, this);
    connect(controller, &Konsole::SessionController::viewFocused, this, &Konsole::ViewManager::controllerChanged);
    connect(session, &Konsole::Session::destroyed, controller, &Konsole::SessionController::deleteLater);
    connect(session, &Konsole::Session::primaryScreenInUse, controller, &Konsole::SessionController::setupPrimaryScreenSpecificActions);
    connect(session, &Konsole::Session::selectionChanged, controller, &Konsole::SessionController::selectionChanged);
    connect(view, &Konsole::TerminalDisplay::destroyed, controller, &Konsole::SessionController::deleteLater);
    connect(controller, &Konsole::SessionController::viewDragAndDropped, this, &Konsole::ViewManager::forgetController);
    connect(controller, &Konsole::SessionController::requestSplitViewLeftRight, this, &Konsole::ViewManager::splitLeftRight);
    connect(controller, &Konsole::SessionController::requestSplitViewTopBotton, this, &Konsole::ViewManager::splitTopBottom);

    // if this is the first controller created then set it as the active controller
    if (_pluggedController.isNull()) {
        controllerChanged(controller);
    }

    return controller;
}

void ViewManager::forgetController(SessionController *controller)
{
    Q_ASSERT(controller->session() != nullptr && controller->view() != nullptr);

    forgetTerminal(controller->view());
    toggleActionsBasedOnState();
}

// should this be handed by ViewManager::unplugController signal
void ViewManager::removeController(SessionController *controller)
{
    Q_EMIT unplugController(controller);

    if (_pluggedController == controller) {
        _pluggedController.clear();
    }
    // disconnect now!! important as a focus change may happen in between and we will end up using a deleted controller
    disconnect(controller, &Konsole::SessionController::viewFocused, this, &Konsole::ViewManager::controllerChanged);
    controller->deleteLater();
}

void ViewManager::controllerChanged(SessionController *controller)
{
    if (controller == _pluggedController) {
        return;
    }

    _viewContainer->setFocusProxy(controller->view());
    updateTerminalDisplayHistory(controller->view());

    _pluggedController = controller;
    Q_EMIT activeViewChanged(controller);
}

SessionController *ViewManager::activeViewController() const
{
    return _pluggedController;
}

void ViewManager::attachView(TerminalDisplay *terminal, Session *session)
{
    connect(session, &Konsole::Session::finished, this, &Konsole::ViewManager::sessionFinished, Qt::UniqueConnection);

    // Disconnect from the other viewcontainer.
    unregisterTerminal(terminal);

    // reconnect on this container.
    registerTerminal(terminal);

    _sessionMap[terminal] = session;
    createController(session, terminal);
    toggleActionsBasedOnState();
    _terminalDisplayHistory.append(terminal);
}

TerminalDisplay *ViewManager::findTerminalDisplay(int viewId)
{
    for (auto i = _sessionMap.keyBegin(); i != _sessionMap.keyEnd(); ++i) {
        TerminalDisplay *view = *i;
        if (view->id() == viewId)
            return view;
    }

    return nullptr;
}

void ViewManager::setCurrentView(TerminalDisplay *view)
{
    auto parentSplitter = qobject_cast<ViewSplitter *>(view->parentWidget());
    _viewContainer->setCurrentWidget(parentSplitter->getToplevelSplitter());
    view->setFocus();
    setCurrentSession(_sessionMap[view]->sessionId());
}

TerminalDisplay *ViewManager::createView(Session *session)
{
    // notify this view manager when the session finishes so that its view
    // can be deleted
    //
    // Use Qt::UniqueConnection to avoid duplicate connection
    connect(session, &Konsole::Session::finished, this, &Konsole::ViewManager::sessionFinished, Qt::UniqueConnection);
    TerminalDisplay *display = createTerminalDisplay();
    createController(session, display);

    const Profile::Ptr profile = SessionManager::instance()->sessionProfile(session);
    applyProfileToView(display, profile);

    // set initial size
    const QSize &preferredSize = session->preferredSize();

    display->setSize(preferredSize.width(), preferredSize.height());

    _sessionMap[display] = session;
    session->addView(display);
    _terminalDisplayHistory.append(display);

    // tell the session whether it has a light or dark background
    session->setDarkBackground(colorSchemeForProfile(profile)->hasDarkBackground());
    display->setFocus(Qt::OtherFocusReason);
    //     updateDetachViewState();
    connect(display, &TerminalDisplay::activationRequest, this, &Konsole::ViewManager::activationRequest);

    return display;
}

TabbedViewContainer *ViewManager::createContainer()
{
    auto *container = new TabbedViewContainer(this, nullptr);
    container->setNavigationVisibility(_navigationVisibility);
    connect(container, &TabbedViewContainer::detachTab, this, &ViewManager::detachTab);

    // connect signals and slots
    connect(container, &Konsole::TabbedViewContainer::viewAdded, this, [this, container]() {
        containerViewsChanged(container);
    });
    connect(container, &Konsole::TabbedViewContainer::viewRemoved, this, [this, container]() {
        containerViewsChanged(container);
    });

    connect(container, &TabbedViewContainer::newViewRequest, this, &ViewManager::newViewRequest);
    connect(container, &Konsole::TabbedViewContainer::newViewWithProfileRequest, this, &Konsole::ViewManager::newViewWithProfileRequest);
    connect(container, &Konsole::TabbedViewContainer::activeViewChanged, this, &Konsole::ViewManager::activateView);

    return container;
}

void ViewManager::setNavigationMethod(NavigationMethod method)
{
    Q_ASSERT(_actionCollection);
    if (_actionCollection == nullptr) {
        return;
    }
    KActionCollection *collection = _actionCollection;

    _navigationMethod = method;

    // FIXME: The following disables certain actions for the KPart that it
    // doesn't actually have a use for, to avoid polluting the action/shortcut
    // namespace of an application using the KPart (otherwise, a shortcut may
    // be in use twice, and the user gets to see an "ambiguous shortcut over-
    // load" error dialog). However, this approach sucks - it's the inverse of
    // what it should be. Rather than disabling actions not used by the KPart,
    // a method should be devised to only enable those that are used, perhaps
    // by using a separate action collection.

    const bool enable = (method != NoNavigation);

    auto enableAction = [&enable, &collection](const QString &actionName) {
        auto *action = collection->action(actionName);
        if (action != nullptr) {
            action->setEnabled(enable);
        }
    };

    enableAction(QStringLiteral("next-view"));
    enableAction(QStringLiteral("previous-view"));
    enableAction(QStringLiteral("last-tab"));
    enableAction(QStringLiteral("last-used-tab"));
    enableAction(QStringLiteral("last-used-tab-reverse"));
    enableAction(QStringLiteral("split-view-left-right"));
    enableAction(QStringLiteral("split-view-top-bottom"));
    enableAction(QStringLiteral("split-view-left-right-next-tab"));
    enableAction(QStringLiteral("split-view-top-bottom-next-tab"));
    enableAction(QStringLiteral("rename-session"));
    enableAction(QStringLiteral("move-view-left"));
    enableAction(QStringLiteral("move-view-right"));
}

ViewManager::NavigationMethod ViewManager::navigationMethod() const
{
    return _navigationMethod;
}

void ViewManager::containerViewsChanged(TabbedViewContainer *container)
{
    Q_UNUSED(container)
    // TODO: Verify that this is right.
    Q_EMIT viewPropertiesChanged(viewProperties());
}

void ViewManager::viewDestroyed(QWidget *view)
{
    // Note: the received QWidget has already been destroyed, so
    // using dynamic_cast<> or qobject_cast<> does not work here
    // We only need the pointer address to look it up below
    auto *display = reinterpret_cast<TerminalDisplay *>(view);

    // 1. detach view from session
    // 2. if the session has no views left, close it
    Session *session = _sessionMap[display];
    _sessionMap.remove(display);
    if (session != nullptr) {
        if (session->views().count() == 0) {
            session->close();
        }
    }

    // we only update the focus if the splitter is still alive
    toggleActionsBasedOnState();

    // The below causes the menus  to be messed up
    // Only happens when using the tab bar close button
    //    if (_pluggedController)
    //        Q_EMIT unplugController(_pluggedController);
}

TerminalDisplay *ViewManager::createTerminalDisplay()
{
    auto display = new TerminalDisplay(nullptr);
    registerTerminal(display);

    return display;
}

std::shared_ptr<const ColorScheme> ViewManager::colorSchemeForProfile(const Profile::Ptr &profile)
{
    std::shared_ptr<const ColorScheme> colorScheme = ColorSchemeManager::instance()->findColorScheme(profile->colorScheme());
    if (colorScheme == nullptr) {
        colorScheme = ColorSchemeManager::instance()->defaultColorScheme();
    }
    Q_ASSERT(colorScheme);

    return colorScheme;
}

bool ViewManager::profileHasBlurEnabled(const Profile::Ptr &profile)
{
    return colorSchemeForProfile(profile)->blur();
}

void ViewManager::applyProfileToView(TerminalDisplay *view, const Profile::Ptr &profile)
{
    Q_ASSERT(profile);
    view->applyProfile(profile);
    Q_EMIT updateWindowIcon();
    Q_EMIT blurSettingChanged(view->colorScheme()->blur());
    Q_EMIT translucentBackgroundSettingChanged(view->colorScheme()->opacity() != 1.0);
}

void ViewManager::updateViewsForSession(Session *session)
{
    const Profile::Ptr profile = SessionManager::instance()->sessionProfile(session);

    const QList<TerminalDisplay *> sessionMapKeys = _sessionMap.keys(session);
    for (TerminalDisplay *view : sessionMapKeys) {
        applyProfileToView(view, profile);
    }
}

void ViewManager::profileChanged(const Profile::Ptr &profile)
{
    // update all views associated with this profile
    QHashIterator<TerminalDisplay *, Session *> iter(_sessionMap);
    while (iter.hasNext()) {
        iter.next();

        // if session uses this profile, update the display
        if (iter.key() != nullptr && iter.value() != nullptr && SessionManager::instance()->sessionProfile(iter.value()) == profile) {
            applyProfileToView(iter.key(), profile);
        }
    }
}

QList<ViewProperties *> ViewManager::viewProperties() const
{
    QList<ViewProperties *> list;

    TabbedViewContainer *container = _viewContainer;
    if (container == nullptr) {
        return {};
    }

    auto terminalContainers = _viewContainer->findChildren<TerminalDisplay *>();
    list.reserve(terminalContainers.size());

    for (auto terminalDisplay : _viewContainer->findChildren<TerminalDisplay *>()) {
        list.append(terminalDisplay->sessionController());
    }

    return list;
}

namespace
{
QJsonObject saveSessionTerminal(TerminalDisplay *terminalDisplay)
{
    QJsonObject thisTerminal;
    auto terminalSession = terminalDisplay->sessionController()->session();
    const int sessionRestoreId = SessionManager::instance()->getRestoreId(terminalSession);
    thisTerminal.insert(QStringLiteral("SessionRestoreId"), sessionRestoreId);
    return thisTerminal;
}

QJsonObject saveSessionsRecurse(QSplitter *splitter)
{
    QJsonObject thisSplitter;
    thisSplitter.insert(QStringLiteral("Orientation"), splitter->orientation() == Qt::Horizontal ? QStringLiteral("Horizontal") : QStringLiteral("Vertical"));

    QJsonArray internalWidgets;
    for (int i = 0; i < splitter->count(); i++) {
        auto *widget = splitter->widget(i);
        auto *maybeSplitter = qobject_cast<QSplitter *>(widget);
        auto *maybeTerminalDisplay = qobject_cast<TerminalDisplay *>(widget);

        if (maybeSplitter != nullptr) {
            internalWidgets.append(saveSessionsRecurse(maybeSplitter));
        } else if (maybeTerminalDisplay != nullptr) {
            internalWidgets.append(saveSessionTerminal(maybeTerminalDisplay));
        }
    }
    thisSplitter.insert(QStringLiteral("Widgets"), internalWidgets);
    return thisSplitter;
}

} // namespace

void ViewManager::saveLayoutFile()
{
    QString fileName(QFileDialog::getSaveFileName(this->widget(),
                                                  i18nc("@title:window", "Save Tab Layout"),
                                                  QStringLiteral("~/"),
                                                  i18nc("@item:inlistbox", "Konsole View Layout (*.json)")));

    // User pressed cancel in dialog
    if (fileName.isEmpty()) {
        return;
    }

    if (!fileName.endsWith(QStringLiteral(".json"))) {
        fileName.append(QStringLiteral(".json"));
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        KMessageBox::error(this->widget(), i18nc("@label:textbox", "A problem occurred when saving the Layout.\n%1", file.fileName()));
    }

    QJsonObject jsonSplit = saveSessionsRecurse(_viewContainer->activeViewSplitter());

    if (!jsonSplit.isEmpty()) {
        file.write(QJsonDocument(jsonSplit).toJson());
        qDebug() << "Maybe was saved";
    }
}

void ViewManager::saveSessions(KConfigGroup &group)
{
    QJsonArray rootArray;
    for (int i = 0; i < _viewContainer->count(); i++) {
        auto *splitter = qobject_cast<QSplitter *>(_viewContainer->widget(i));
        rootArray.append(saveSessionsRecurse(splitter));
    }

    group.writeEntry("Tabs", QJsonDocument(rootArray).toJson(QJsonDocument::Compact));
    group.writeEntry("Active", _viewContainer->currentIndex());
}

namespace
{
ViewSplitter *restoreSessionsSplitterRecurse(const QJsonObject &jsonSplitter, ViewManager *manager, bool useSessionId)
{
    const QJsonArray splitterWidgets = jsonSplitter[QStringLiteral("Widgets")].toArray();
    auto orientation = (jsonSplitter[QStringLiteral("Orientation")].toString() == QStringLiteral("Horizontal")) ? Qt::Horizontal : Qt::Vertical;

    auto *currentSplitter = new ViewSplitter();
    currentSplitter->setOrientation(orientation);

    for (const auto widgetJsonValue : splitterWidgets) {
        const auto widgetJsonObject = widgetJsonValue.toObject();
        const auto sessionIterator = widgetJsonObject.constFind(QStringLiteral("SessionRestoreId"));

        if (sessionIterator != widgetJsonObject.constEnd()) {
            Session *session = useSessionId ? SessionManager::instance()->idToSession(sessionIterator->toInt()) : SessionManager::instance()->createSession();

            auto newView = manager->createView(session);
            currentSplitter->addWidget(newView);
        } else {
            auto nextSplitter = restoreSessionsSplitterRecurse(widgetJsonObject, manager, useSessionId);
            currentSplitter->addWidget(nextSplitter);
        }
    }
    return currentSplitter;
}

} // namespace
void ViewManager::loadLayout(QString file)
{
    // User pressed cancel in dialog
    if (file.isEmpty()) {
        return;
    }

    QFile jsonFile(file);

    if (!jsonFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        KMessageBox::error(this->widget(), i18nc("@label:textbox", "A problem occurred when loading the Layout.\n%1", jsonFile.fileName()));
    }
    auto json = QJsonDocument::fromJson(jsonFile.readAll());
    if (!json.isEmpty()) {
        auto splitter = restoreSessionsSplitterRecurse(json.object(), this, false);
        _viewContainer->addSplitter(splitter, _viewContainer->count());
    }
}
void ViewManager::loadLayoutFile()
{
    loadLayout(QFileDialog::getOpenFileName(this->widget(),
                                            i18nc("@title:window", "Load Tab Layout"),
                                            QStringLiteral("~/"),
                                            i18nc("@item:inlistbox", "Konsole View Layout (*.json)")));
}

void ViewManager::restoreSessions(const KConfigGroup &group)
{
    const auto tabList = group.readEntry("Tabs", QByteArray("[]"));
    const auto jsonTabs = QJsonDocument::fromJson(tabList).array();
    for (const auto &jsonSplitter : jsonTabs) {
        auto topLevelSplitter = restoreSessionsSplitterRecurse(jsonSplitter.toObject(), this, true);
        _viewContainer->addSplitter(topLevelSplitter, _viewContainer->count());
    }

    if (!jsonTabs.isEmpty())
        return;

    // Session file is unusable, try older format
    QList<int> ids = group.readEntry("Sessions", QList<int>());
    int activeTab = group.readEntry("Active", 0);
    TerminalDisplay *display = nullptr;

    int tab = 1;
    for (auto it = ids.cbegin(); it != ids.cend(); ++it) {
        const int &id = *it;
        Session *session = SessionManager::instance()->idToSession(id);

        if (session == nullptr) {
            qWarning() << "Unable to load session with id" << id;
            // Force a creation of a default session below
            ids.clear();
            break;
        }

        activeContainer()->addView(createView(session));
        if (!session->isRunning()) {
            session->run();
        }
        if (tab++ == activeTab) {
            display = qobject_cast<TerminalDisplay *>(activeView());
        }
    }

    if (display != nullptr) {
        activeContainer()->setCurrentWidget(display);
        display->setFocus(Qt::OtherFocusReason);
    }

    if (ids.isEmpty()) { // Session file is unusable, start default Profile
        Profile::Ptr profile = ProfileManager::instance()->defaultProfile();
        Session *session = SessionManager::instance()->createSession(profile);
        activeContainer()->addView(createView(session));
        if (!session->isRunning()) {
            session->run();
        }
    }
}

TabbedViewContainer *ViewManager::activeContainer()
{
    return _viewContainer;
}

int ViewManager::sessionCount()
{
    return _sessionMap.size();
}

QStringList ViewManager::sessionList()
{
    QStringList ids;

    for (int i = 0; i < _viewContainer->count(); i++) {
        auto terminaldisplayList = _viewContainer->widget(i)->findChildren<TerminalDisplay *>();
        for (auto *terminaldisplay : terminaldisplayList) {
            ids.append(QString::number(terminaldisplay->sessionController()->session()->sessionId()));
        }
    }

    return ids;
}

int ViewManager::currentSession()
{
    if (_pluggedController != nullptr) {
        Q_ASSERT(_pluggedController->session() != nullptr);
        return _pluggedController->session()->sessionId();
    }
    return -1;
}

void ViewManager::setCurrentSession(int sessionId)
{
    auto *session = SessionManager::instance()->idToSession(sessionId);
    if (session == nullptr || session->views().count() == 0) {
        return;
    }

    auto *display = session->views().at(0);
    if (display != nullptr) {
        display->setFocus(Qt::OtherFocusReason);

        auto *splitter = qobject_cast<ViewSplitter *>(display->parent());
        if (splitter != nullptr) {
            _viewContainer->setCurrentWidget(splitter->getToplevelSplitter());
        }
    }
}

int ViewManager::newSession()
{
    return newSession(QString(), QString());
}

int ViewManager::newSession(const QString &profile)
{
    return newSession(profile, QString());
}

int ViewManager::newSession(const QString &profile, const QString &directory)
{
    Profile::Ptr profileptr = ProfileManager::instance()->defaultProfile();
    if (!profile.isEmpty()) {
        const QList<Profile::Ptr> profilelist = ProfileManager::instance()->allProfiles();

        for (const auto &i : profilelist) {
            if (i->name() == profile) {
                profileptr = i;
                break;
            }
        }
    }

    Session *session = createSession(profileptr, directory);

    auto newView = createView(session);
    activeContainer()->addView(newView);
    session->run();

    return session->sessionId();
}

QString ViewManager::defaultProfile()
{
    return ProfileManager::instance()->defaultProfile()->name();
}

void ViewManager::setDefaultProfile(const QString &profileName)
{
    const QList<Profile::Ptr> profiles = ProfileManager::instance()->allProfiles();
    for (const Profile::Ptr &profile : profiles) {
        if (profile->name() == profileName) {
            ProfileManager::instance()->setDefaultProfile(profile);
        }
    }
}

QStringList ViewManager::profileList()
{
    return ProfileManager::instance()->availableProfileNames();
}

void ViewManager::nextSession()
{
    nextView();
}

void ViewManager::prevSession()
{
    previousView();
}

void ViewManager::moveSessionLeft()
{
    moveActiveViewLeft();
}

void ViewManager::moveSessionRight()
{
    moveActiveViewRight();
}

void ViewManager::setTabWidthToText(bool setTabWidthToText)
{
    _viewContainer->tabBar()->setExpanding(!setTabWidthToText);
    _viewContainer->tabBar()->update();
}

QStringList ViewManager::viewHierarchy()
{
    QStringList list;

    for (int i = 0; i < _viewContainer->count(); ++i) {
        list.append(_viewContainer->viewSplitterAt(i)->getChildWidgetsLayout());
    }

    return list;
}

QList<double> ViewManager::getSplitProportions(int splitterId)
{
    auto splitter = _viewContainer->findSplitter(splitterId);
    if (splitter == nullptr)
        return QList<double>();

    int totalSize = 0;
    QList<double> percentages;

    for (auto size : splitter->sizes()) {
        totalSize += size;
    }

    if (totalSize == 0)
        return QList<double>();

    for (auto size : splitter->sizes()) {
        percentages.append((size / static_cast<double>(totalSize)) * 100);
    }

    return percentages;
}

bool ViewManager::createSplit(int viewId, bool horizontalSplit)
{
    if (auto view = findTerminalDisplay(viewId)) {
        setCurrentView(view);
        splitView(horizontalSplit ? Qt::Horizontal : Qt::Vertical, false);
        return true;
    }

    return false;
}

bool ViewManager::createSplitWithExisting(int targetSplitterId, QStringList widgetInfos, int idx, bool horizontalSplit)
{
    auto targetSplitter = _viewContainer->findSplitter(targetSplitterId);
    if (targetSplitter == nullptr || idx < 0)
        return false;

    QVector<QWidget *> linearLayout;
    QList<int> forbiddenSplitters, forbiddenViews;

    // specify that top level splitters should not be used as children for created splittter
    for (int i = 0; i < _viewContainer->count(); ++i) {
        forbiddenSplitters.append(_viewContainer->viewSplitterAt(i)->id());
    }

    // specify that parent splitters of the splitter with targetSplitterId id should not be used
    // as children for created splitter
    for (auto splitter = targetSplitter; splitter != targetSplitter->getToplevelSplitter(); splitter = qobject_cast<ViewSplitter *>(splitter->parentWidget())) {
        forbiddenSplitters.append(splitter->id());
    }

    // to make positioning clearer by avoiding situations where
    // e.g. splitter to be created is at index x of targetSplitter
    // and some direct children of targetSplitter are used as
    // children of created splitter, causing the final position
    // of created splitter to may not be at x
    for (int i = 0; i < targetSplitter->count(); ++i) {
        auto w = targetSplitter->widget(i);

        if (auto s = qobject_cast<ViewSplitter *>(w))
            forbiddenSplitters.append(s->id());
        else
            forbiddenViews.append(qobject_cast<TerminalDisplay *>(w)->id());
    }

    for (auto &info : widgetInfos) {
        auto typeAndId = info.split(QLatin1Char('-'));
        if (typeAndId.size() != 2)
            return false;

        int id = typeAndId[1].toInt();
        QChar type = typeAndId[0][0];

        if (type == QLatin1Char('s') && !forbiddenSplitters.removeOne(id)) {
            if (auto s = _viewContainer->findSplitter(id)) {
                linearLayout.append(s);
                continue;
            }
        } else if (type == QLatin1Char('v') && !forbiddenViews.removeOne(id)) {
            if (auto v = findTerminalDisplay(id)) {
                linearLayout.append(v);
                continue;
            }
        }

        return false;
    }

    if (linearLayout.count() == 1) {
        if (auto onlyChildSplitter = qobject_cast<ViewSplitter *>(linearLayout[0])) {
            targetSplitter->addSplitter(onlyChildSplitter, idx);
        } else {
            auto onlyChildView = qobject_cast<TerminalDisplay *>(linearLayout[0]);
            targetSplitter->addTerminalDisplay(onlyChildView, idx);
        }
    } else {
        ViewSplitter *createdSplitter = new ViewSplitter();
        createdSplitter->setOrientation(horizontalSplit ? Qt::Horizontal : Qt::Vertical);

        for (auto widget : linearLayout) {
            if (auto s = qobject_cast<ViewSplitter *>(widget))
                createdSplitter->addSplitter(s);
            else
                createdSplitter->addTerminalDisplay(qobject_cast<TerminalDisplay *>(widget));
        }

        targetSplitter->addSplitter(createdSplitter, idx);
    }

    setCurrentView(targetSplitter->activeTerminalDisplay());
    return true;
}

bool ViewManager::setCurrentView(int viewId)
{
    if (auto view = findTerminalDisplay(viewId)) {
        setCurrentView(view);
        return true;
    }

    return false;
}

bool ViewManager::resizeSplits(int splitterId, QList<double> percentages)
{
    auto splitter = _viewContainer->findSplitter(splitterId);
    int totalP = 0;

    for (auto p : percentages) {
        if (p < 1)
            return false;

        totalP += p;
    }

    // make sure that the sum of percentages is very close
    // to but not exceeding 100. above 99% but less than 100 %
    // seems like good constraint
    if (splitter == nullptr || percentages.count() != splitter->sizes().count() || totalP > 100 || totalP < 99)
        return false;

    int sum = 0;
    QList<int> newSizes;

    for (int size : splitter->sizes()) {
        sum += size;
    }

    for (int i = 0; i < percentages.count(); ++i) {
        newSizes.append(static_cast<int>(sum * percentages.at(i)));
    }

    splitter->setSizes(newSizes);
    setCurrentView(splitter->activeTerminalDisplay());
    return true;
}

bool ViewManager::moveSplitter(int splitterId, int targetSplitterId, int idx)
{
    auto splitter = _viewContainer->findSplitter(splitterId);
    auto targetSplitter = _viewContainer->findSplitter(targetSplitterId);

    if (splitter == nullptr || targetSplitter == nullptr || idx < 0)
        return false;

    for (auto s = targetSplitter; s != s->getToplevelSplitter(); s = qobject_cast<ViewSplitter *>(s->parentWidget())) {
        if (s == splitter)
            return false;
    }

    for (int i = 0; i < _viewContainer->count(); ++i) {
        if (splitter == _viewContainer->viewSplitterAt(i))
            return false;
    }

    targetSplitter->addSplitter(splitter, idx);
    setCurrentView(splitter->activeTerminalDisplay());
    return true;
}

bool ViewManager::moveView(int viewId, int targetSplitterId, int idx)
{
    auto view = findTerminalDisplay(viewId);
    auto targetSplitter = _viewContainer->findSplitter(targetSplitterId);

    if (view == nullptr || targetSplitter == nullptr || idx < 0)
        return false;

    targetSplitter->addTerminalDisplay(view, idx);
    setCurrentView(view);
    return true;
}

void ViewManager::setNavigationVisibility(NavigationVisibility navigationVisibility)
{
    if (_navigationVisibility != navigationVisibility) {
        _navigationVisibility = navigationVisibility;
        _viewContainer->setNavigationVisibility(navigationVisibility);
    }
}

void ViewManager::updateTerminalDisplayHistory(TerminalDisplay *terminalDisplay, bool remove)
{
    if (terminalDisplay == nullptr) {
        if (_terminalDisplayHistoryIndex >= 0) {
            // This is the case when we finished walking through the history
            // (i.e. when Ctrl-Tab has been released)
            terminalDisplay = _terminalDisplayHistory[_terminalDisplayHistoryIndex];
            _terminalDisplayHistoryIndex = -1;
        } else {
            return;
        }
    }

    if (_terminalDisplayHistoryIndex >= 0 && !remove) {
        // Do not reorder the tab history while we are walking through it
        return;
    }

    for (int i = 0; i < _terminalDisplayHistory.count(); i++) {
        if (_terminalDisplayHistory[i] == terminalDisplay) {
            _terminalDisplayHistory.removeAt(i);
            if (!remove) {
                _terminalDisplayHistory.prepend(terminalDisplay);
            }
            break;
        }
    }
}

void ViewManager::registerTerminal(TerminalDisplay *terminal)
{
    connect(terminal, &TerminalDisplay::requestToggleExpansion, _viewContainer, &TabbedViewContainer::toggleMaximizeCurrentTerminal, Qt::UniqueConnection);
    connect(terminal, &TerminalDisplay::requestMoveToNewTab, _viewContainer, &TabbedViewContainer::moveToNewTab, Qt::UniqueConnection);
}

void ViewManager::unregisterTerminal(TerminalDisplay *terminal)
{
    disconnect(terminal, &TerminalDisplay::requestToggleExpansion, nullptr, nullptr);
    disconnect(terminal, &TerminalDisplay::requestMoveToNewTab, nullptr, nullptr);
}

#include "moc_ViewManager.cpp"

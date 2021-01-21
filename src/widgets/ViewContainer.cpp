/*
    This file is part of the Konsole Terminal.

    Copyright 2006-2008 Robert Knight <robertknight@gmail.com>

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
#include "widgets/ViewContainer.h"
#include "config-konsole.h"

// Qt
#include <QTabBar>
#include <QMenu>
#include <QFile>
#include <QKeyEvent>

// KDE
#include <KColorScheme>
#include <KColorUtils>
#include <KLocalizedString>
#include <KActionCollection>

// Konsole
#include "widgets/IncrementalSearchBar.h"
#include "ViewProperties.h"
#include "profile/ProfileList.h"
#include "KonsoleSettings.h"
#include "session/SessionController.h"
#include "DetachableTabBar.h"
#include "terminalDisplay/TerminalDisplay.h"
#include "widgets/ViewSplitter.h"
#include "MainWindow.h"

// TODO Perhaps move everything which is Konsole-specific into different files

using namespace Konsole;


TabbedViewContainer::TabbedViewContainer(ViewManager *connectedViewManager, QWidget *parent) :
    QTabWidget(parent),
    _connectedViewManager(connectedViewManager),
    _newTabButton(new QToolButton(this)),
    _closeTabButton(new QToolButton(this)),
    _contextMenuTabIndex(-1),
    _navigationVisibility(ViewManager::NavigationVisibility::NavigationNotSet),
    _newTabBehavior(PutNewTabAtTheEnd)
{
    setAcceptDrops(true);

    auto tabBarWidget = new DetachableTabBar();
    setTabBar(tabBarWidget);
    setDocumentMode(true);
    setMovable(true);
    connect(tabBarWidget, &DetachableTabBar::moveTabToWindow, this, &TabbedViewContainer::moveTabToWindow);
    tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    _newTabButton->setIcon(QIcon::fromTheme(QStringLiteral("tab-new")));
    _newTabButton->setAutoRaise(true);
    _newTabButton->setToolTip(i18nc("@info:tooltip", "Open a new tab"));
    connect(_newTabButton, &QToolButton::clicked, this, &TabbedViewContainer::newViewRequest);

    _closeTabButton->setIcon(QIcon::fromTheme(QStringLiteral("tab-close")));
    _closeTabButton->setAutoRaise(true);
    _closeTabButton->setToolTip(i18nc("@info:tooltip", "Close this tab"));
    connect(_closeTabButton, &QToolButton::clicked, this, [this]{
       closeCurrentTab();
    });

    connect(tabBar(), &QTabBar::tabBarDoubleClicked, this,
        &Konsole::TabbedViewContainer::tabDoubleClicked);
    connect(tabBar(), &QTabBar::customContextMenuRequested, this,
        &Konsole::TabbedViewContainer::openTabContextMenu);
    connect(tabBarWidget, &DetachableTabBar::detachTab, this, [this](int idx) {
        emit detachTab(idx);
    });
    connect(tabBarWidget, &DetachableTabBar::closeTab,
        this, &TabbedViewContainer::closeTerminalTab);
    connect(tabBarWidget, &DetachableTabBar::newTabRequest,
        this, [this]{ emit newViewRequest(); });
    connect(this, &TabbedViewContainer::currentChanged, this, &TabbedViewContainer::currentTabChanged);

    connect(this, &TabbedViewContainer::setColor, tabBarWidget, &DetachableTabBar::setColor);
    connect(this, &TabbedViewContainer::removeColor, tabBarWidget, &DetachableTabBar::removeColor);

    // The context menu of tab bar
    _contextPopupMenu = new QMenu(tabBar());
    connect(_contextPopupMenu, &QMenu::aboutToHide, this, [this]() {
        // Remove the read-only action when the popup closes
        for (auto &action : _contextPopupMenu->actions()) {
            if (action->objectName() == QStringLiteral("view-readonly")) {
                _contextPopupMenu->removeAction(action);
                break;
            }
        }
    });

    connect(tabBar(), &QTabBar::tabCloseRequested, this, &TabbedViewContainer::closeTerminalTab);

    auto detachAction = _contextPopupMenu->addAction(
        QIcon::fromTheme(QStringLiteral("tab-detach")),
        i18nc("@action:inmenu", "&Detach Tab"), this,
        [this] { emit detachTab(_contextMenuTabIndex); }
    );
    detachAction->setObjectName(QStringLiteral("tab-detach"));

    auto editAction = _contextPopupMenu->addAction(
        QIcon::fromTheme(QStringLiteral("edit-rename")),
        i18nc("@action:inmenu", "&Configure or Rename Tab..."), this,
        [this]{ renameTab(_contextMenuTabIndex); }
    );
    editAction->setObjectName(QStringLiteral("edit-rename"));

    auto closeAction = _contextPopupMenu->addAction(
        QIcon::fromTheme(QStringLiteral("tab-close")),
        i18nc("@action:inmenu", "Close Tab"), this,
        [this] { closeTerminalTab(_contextMenuTabIndex); }
    );
    closeAction->setObjectName(QStringLiteral("tab-close"));

    auto profileMenu = new QMenu(this);
    auto profileList = new ProfileList(false, profileMenu);
    profileList->syncWidgetActions(profileMenu, true);
    connect(profileList, &Konsole::ProfileList::profileSelected, this, &TabbedViewContainer::newViewWithProfileRequest);
    _newTabButton->setMenu(profileMenu);

    konsoleConfigChanged();
    connect(KonsoleSettings::self(), &KonsoleSettings::configChanged, this, &TabbedViewContainer::konsoleConfigChanged);
}

TabbedViewContainer::~TabbedViewContainer()
{
    for(int i = 0, end = count(); i < end; i++) {
        auto view = widget(i);
        disconnect(view, &QWidget::destroyed, this, &Konsole::TabbedViewContainer::viewDestroyed);
    }
}

ViewSplitter *TabbedViewContainer::activeViewSplitter()
{
    return viewSplitterAt(currentIndex());
}

ViewSplitter *TabbedViewContainer::viewSplitterAt(int index)
{
    return qobject_cast<ViewSplitter*>(widget(index));
}

void TabbedViewContainer::moveTabToWindow(int index, QWidget *window)
{
    auto splitter = viewSplitterAt(index);
    auto manager = window->findChild<ViewManager*>();

    QHash<TerminalDisplay*, Session*> sessionsMap = _connectedViewManager->forgetAll(splitter);

    const QList<TerminalDisplay *> displays = splitter->findChildren<TerminalDisplay*>();
    for (TerminalDisplay *terminal : displays) {
        manager->attachView(terminal, sessionsMap[terminal]);
    }
    auto container = manager->activeContainer();
    container->addSplitter(splitter);

    auto controller = splitter->activeTerminalDisplay()->sessionController();
    container->currentSessionControllerChanged(controller);

    forgetView();
}

void TabbedViewContainer::konsoleConfigChanged()
{
    // don't show tabs if we are in KParts mode.
    // This is a hack, and this needs to be rewritten.
    // The container should not be part of the KParts, perhaps just the
    // TerminalDisplay should.

    // ASAN issue if using sessionController->isKonsolePart(), just
    // duplicate code for now
    if (qApp->applicationName() != QLatin1String("konsole")) {
        tabBar()->setVisible(false);
    } else {
        // if we start with --show-tabbar or --hide-tabbar we ignore the preferences.
        setTabBarAutoHide(KonsoleSettings::tabBarVisibility() == KonsoleSettings::EnumTabBarVisibility::ShowTabBarWhenNeeded);
        if (KonsoleSettings::tabBarVisibility() == KonsoleSettings::EnumTabBarVisibility::AlwaysShowTabBar) {
            tabBar()->setVisible(true);
        } else if (KonsoleSettings::tabBarVisibility() == KonsoleSettings::EnumTabBarVisibility::AlwaysHideTabBar) {
            tabBar()->setVisible(false);
        }
    }

    setTabPosition((QTabWidget::TabPosition) KonsoleSettings::tabBarPosition());

    setCornerWidget(KonsoleSettings::newTabButton() ? _newTabButton : nullptr, Qt::TopLeftCorner);
    _newTabButton->setVisible(KonsoleSettings::newTabButton());

    setCornerWidget(KonsoleSettings::closeTabButton() == 1 ? _closeTabButton : nullptr, Qt::TopRightCorner);
    _closeTabButton->setVisible(KonsoleSettings::closeTabButton() == 1);

    tabBar()->setTabsClosable(KonsoleSettings::closeTabButton() == 0);

    tabBar()->setExpanding(KonsoleSettings::expandTabWidth());
    tabBar()->update();

    if (KonsoleSettings::tabBarUseUserStyleSheet()) {
        setCssFromFile(KonsoleSettings::tabBarUserStyleSheetFile());
    } else {
        setCss();
    }
}

void TabbedViewContainer::setCss(const QString& styleSheet)
{
    static const QString defaultCss = QStringLiteral("QTabWidget::tab-bar, QTabWidget::pane { margin: 0; }\n");
    setStyleSheet(defaultCss + styleSheet);
}

void TabbedViewContainer::setCssFromFile(const QUrl &url)
{
    // Let's only deal w/ local files for now
    if (!url.isLocalFile()) {
        setStyleSheet(KonsoleSettings::tabBarStyleSheet());
    }

    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(KonsoleSettings::tabBarStyleSheet());
    }

    QTextStream in(&file);
    setCss(in.readAll());
}

void TabbedViewContainer::moveActiveView(MoveDirection direction)
{
    if (count() < 2) { // return if only one view
        return;
    }
    const int currentIndex = indexOf(currentWidget());
    int newIndex = direction  == MoveViewLeft ? qMax(currentIndex - 1, 0) : qMin(currentIndex + 1, count() - 1);

    auto swappedWidget = viewSplitterAt(newIndex);
    auto swappedTitle = tabBar()->tabText(newIndex);
    auto swappedIcon = tabBar()->tabIcon(newIndex);

    auto currentWidget = viewSplitterAt(currentIndex);
    auto currentTitle = tabBar()->tabText(currentIndex);
    auto currentIcon = tabBar()->tabIcon(currentIndex);

    if (newIndex < currentIndex) {
        insertTab(newIndex, currentWidget, currentIcon, currentTitle);
        insertTab(currentIndex, swappedWidget, swappedIcon, swappedTitle);
    } else {
        insertTab(currentIndex, swappedWidget, swappedIcon, swappedTitle);
        insertTab(newIndex, currentWidget, currentIcon, currentTitle);
    }
    setCurrentIndex(newIndex);
}

void TabbedViewContainer::terminalDisplayDropped(TerminalDisplay *terminalDisplay) {
    if (terminalDisplay->sessionController()->parent() != connectedViewManager()) {
        // Terminal from another window - recreate SessionController for current ViewManager
        disconnectTerminalDisplay(terminalDisplay);
        Session* terminalSession = terminalDisplay->sessionController()->session();
        terminalDisplay->sessionController()->deleteLater();
        connectedViewManager()->attachView(terminalDisplay, terminalSession);
        connectTerminalDisplay(terminalDisplay);
    }
}

QSize TabbedViewContainer::sizeHint() const
{
    // QTabWidget::sizeHint() contains some margins added by widgets
    // style, which were making the initial window size too big.
    const auto tabsSize = tabBar()->sizeHint();
    const auto *leftWidget = cornerWidget(Qt::TopLeftCorner);
    const auto *rightWidget = cornerWidget(Qt::TopRightCorner);
    const auto leftSize = leftWidget != nullptr ? leftWidget->sizeHint() : QSize(0, 0);
    const auto rightSize = rightWidget != nullptr ? rightWidget->sizeHint() : QSize(0, 0);

    auto tabBarSize = QSize(0, 0);
    // isVisible() won't work; this is called when the window is not yet visible
    if (tabBar()->isVisibleTo(this)) {
        tabBarSize.setWidth(leftSize.width() + tabsSize.width() + rightSize.width());
        tabBarSize.setHeight(qMax(tabsSize.height(), qMax(leftSize.height(), rightSize.height())));
    }

    const auto terminalSize = currentWidget() != nullptr ? currentWidget()->sizeHint() : QSize(0, 0);

    //        width
    // ├──────────────────┤
    //
    // ┌──────────────────┐  ┬
    // │                  │  │
    // │     Terminal     │  │
    // │                  │  │ height
    // ├───┬──────────┬───┤  │  ┬
    // │ L │   Tabs   │ R │  │  │ tab bar height
    // └───┴──────────┴───┘  ┴  ┴
    //
    // L/R = left/right widget

    return {qMax(terminalSize.width(), tabBarSize.width()),
                 tabBarSize.height() + terminalSize.height()};
}

void TabbedViewContainer::addSplitter(ViewSplitter *viewSplitter, int index) {
    index = insertTab(index, viewSplitter, QString());
    connect(viewSplitter, &ViewSplitter::destroyed, this, &TabbedViewContainer::viewDestroyed);

    disconnect(viewSplitter, &ViewSplitter::terminalDisplayDropped, nullptr, nullptr);
    connect(viewSplitter, &ViewSplitter::terminalDisplayDropped, this, &TabbedViewContainer::terminalDisplayDropped);

    const auto terminalDisplays = viewSplitter->findChildren<TerminalDisplay*>();
    for (TerminalDisplay *terminal : terminalDisplays) {
        connectTerminalDisplay(terminal);
    }
    if (terminalDisplays.count() > 0) {
        updateTitle(qobject_cast<ViewProperties*>(terminalDisplays.at(0)->sessionController()));
        updateColor(qobject_cast<ViewProperties*>(terminalDisplays.at(0)->sessionController()));
    }
    setCurrentIndex(index);
}

void TabbedViewContainer::addView(TerminalDisplay *view)
{
    auto viewSplitter = new ViewSplitter();
    viewSplitter->addTerminalDisplay(view, Qt::Horizontal);
    auto item = view->sessionController();
    int index = _newTabBehavior == PutNewTabAfterCurrentTab ? currentIndex() + 1 : -1;
    index = insertTab(index, viewSplitter, item->icon(), item->title());

    connectTerminalDisplay(view);
    connect(viewSplitter, &ViewSplitter::destroyed, this, &TabbedViewContainer::viewDestroyed);
    connect(viewSplitter, &ViewSplitter::terminalDisplayDropped, this, &TabbedViewContainer::terminalDisplayDropped);

    setCurrentIndex(index);
    emit viewAdded(view);
}

void TabbedViewContainer::splitView(TerminalDisplay *view, Qt::Orientation orientation)
{
    auto viewSplitter = qobject_cast<ViewSplitter*>(currentWidget());
    viewSplitter->addTerminalDisplay(view, orientation);
    connectTerminalDisplay(view);
}

void TabbedViewContainer::connectTerminalDisplay(TerminalDisplay *display)
{
    auto item = display->sessionController();
    connect(item, &Konsole::SessionController::viewFocused, this,
            &Konsole::TabbedViewContainer::currentSessionControllerChanged);

    connect(item, &Konsole::ViewProperties::titleChanged, this,
            &Konsole::TabbedViewContainer::updateTitle);

    connect(item, &Konsole::ViewProperties::colorChanged, this,
            &Konsole::TabbedViewContainer::updateColor);

    connect(item, &Konsole::ViewProperties::iconChanged, this,
            &Konsole::TabbedViewContainer::updateIcon);

    connect(item, &Konsole::ViewProperties::activity, this,
            &Konsole::TabbedViewContainer::updateActivity);

    connect(item, &Konsole::ViewProperties::notificationChanged, this,
            &Konsole::TabbedViewContainer::updateNotification);

    connect(item, &Konsole::ViewProperties::readOnlyChanged, this,
            &Konsole::TabbedViewContainer::updateSpecialState);

    connect(item, &Konsole::ViewProperties::copyInputChanged, this,
            &Konsole::TabbedViewContainer::updateSpecialState);
}

void TabbedViewContainer::disconnectTerminalDisplay(TerminalDisplay *display)
{
    auto item = display->sessionController();
    item->disconnect(this);
}

void TabbedViewContainer::viewDestroyed(QObject *view)
{
    QWidget *widget = qobject_cast<QWidget*>(view);
    Q_ASSERT(widget);
    const int idx = indexOf(widget);

    removeTab(idx);
    forgetView();
    _tabIconState.remove(widget);
}

void TabbedViewContainer::forgetView()
{
    if (count() == 0) {
        emit empty(this);
    }
}

void TabbedViewContainer::activateNextView()
{
    QWidget *active = currentWidget();
    int index = indexOf(active);
    setCurrentIndex(index == count() - 1 ? 0 : index + 1);
}

void TabbedViewContainer::activateLastView()
{
    setCurrentIndex(count() - 1);
}

void TabbedViewContainer::activatePreviousView()
{
    QWidget *active = currentWidget();
    int index = indexOf(active);
    setCurrentIndex(index == 0 ? count() - 1 : index - 1);
}

void TabbedViewContainer::keyReleaseEvent(QKeyEvent* event)
{
    if (event->modifiers() == Qt::NoModifier) {
        _connectedViewManager->updateTerminalDisplayHistory();
    }
}

void TabbedViewContainer::closeCurrentTab()
{
    if (currentIndex() != -1) {
        closeTerminalTab(currentIndex());
    }
}

void TabbedViewContainer::tabDoubleClicked(int index)
{
    if (index >= 0) {
        renameTab(index);
    } else {
        emit newViewRequest();
    }
}

void TabbedViewContainer::renameTab(int index)
{
    if (index != -1) {
        setCurrentIndex(index);
        viewSplitterAt(index)
            -> activeTerminalDisplay()
            -> sessionController()
            -> rename();
    }
}

void TabbedViewContainer::openTabContextMenu(const QPoint &point)
{
    if (point.isNull()) {
        return;
    }

    _contextMenuTabIndex = tabBar()->tabAt(point);
    if (_contextMenuTabIndex < 0) {
        return;
    }

    //TODO: add a countChanged signal so we can remove this for.
    // Detaching in mac causes crashes.
    for(auto action : _contextPopupMenu->actions()) {
        if (action->objectName() == QStringLiteral("tab-detach")) {
            action->setEnabled(count() > 1);
        }
    }

    _contextPopupMenu->exec(tabBar()->mapToGlobal(point));
}

void TabbedViewContainer::currentTabChanged(int index)
{
    if (index != -1) {
        auto splitview = qobject_cast<ViewSplitter*>(widget(index));
        auto view = splitview->activeTerminalDisplay();
        setTabActivity(index, false);
        _tabIconState[splitview].notification = Session::NoNotification;
        if (view != nullptr) {
            emit activeViewChanged(view);
            updateIcon(view->sessionController());
        }
    } else {
        deleteLater();
    }
}

void TabbedViewContainer::wheelScrolled(int delta)
{
    if (delta < 0) {
        activateNextView();
    } else {
        activatePreviousView();
    }
}

void TabbedViewContainer::setTabActivity(int index, bool activity)
{
    const QPalette &palette = tabBar()->palette();
    KColorScheme colorScheme(palette.currentColorGroup());
    const QColor colorSchemeActive = colorScheme.foreground(KColorScheme::ActiveText).color();

    const QColor normalColor = palette.text().color();
    const QColor activityColor = KColorUtils::mix(normalColor, colorSchemeActive);

    QColor color = activity ? activityColor : QColor();

    if (color != tabBar()->tabTextColor(index)) {
        tabBar()->setTabTextColor(index, color);
    }
}

void TabbedViewContainer::updateTitle(ViewProperties *item)
{
    auto controller = qobject_cast<SessionController*>(item);
    auto topLevelSplitter = qobject_cast<ViewSplitter*>(controller->view()->parentWidget())->getToplevelSplitter();
    if (controller->view() != topLevelSplitter->activeTerminalDisplay()) {
        return;
    }
    const int index = indexOf(topLevelSplitter);
    QString tabText = item->title();

    setTabToolTip(index, tabText);

    // To avoid having & replaced with _ (shortcut indicator)
    tabText.replace(QLatin1Char('&'), QLatin1String("&&"));
    setTabText(index, tabText);
}

void TabbedViewContainer::updateColor(ViewProperties *item)
{
    auto controller = qobject_cast<SessionController *>(item);
    auto topLevelSplitter = qobject_cast<ViewSplitter*>(controller->view()->parentWidget())->getToplevelSplitter();
    const int index = indexOf(topLevelSplitter);

    emit setColor(index, item->color());
}

void TabbedViewContainer::updateIcon(ViewProperties *item)
{
    auto controller = qobject_cast<SessionController *>(item);
    auto topLevelSplitter = qobject_cast<ViewSplitter*>(controller->view()->parentWidget())->getToplevelSplitter();
    const int index = indexOf(topLevelSplitter);
    const auto &state = _tabIconState[topLevelSplitter];

    // Tab icon priority (from highest to lowest):
    //
    // 1. Latest Notification
    //    - Inactive tab: Latest notification from any view in a tab. Removed
    //      when tab is activated.
    //    - Active tab: Latest notification from focused view. Removed when
    //      focus changes or when the Session clears its notifications
    // 2. Copy input or read-only indicator when all views in the tab have
    //    the status
    // 3. Active view icon

    QIcon icon;
    if (state.notification != Session::NoNotification) {
        switch(state.notification) {
        case Session::Bell:
            icon = QIcon::fromTheme(QLatin1String("notifications"));
            break;
        case Session::Activity:
            icon = QIcon::fromTheme(QLatin1String("dialog-information"));
            break;
        case Session::Silence:
            icon = QIcon::fromTheme(QLatin1String("system-suspend"));
            break;
        default:
            break;
        }
    } else if (state.broadcast) {
        icon = QIcon::fromTheme(QLatin1String("irc-voice"));
    } else if (state.readOnly) {
        icon = QIcon::fromTheme(QLatin1String("object-locked"));
    } else {
        icon = item->icon();
    }

    if (tabIcon(index).name() != icon.name()) {
        setTabIcon(index, icon);
    }
}

void TabbedViewContainer::updateActivity(ViewProperties *item)
{
    auto controller = qobject_cast<SessionController*>(item);
    auto topLevelSplitter = qobject_cast<ViewSplitter*>(controller->view()->parentWidget())->getToplevelSplitter();

    const int index = indexOf(topLevelSplitter);
    if (index != currentIndex()) {
        setTabActivity(index, true);
    }
}

void TabbedViewContainer::updateNotification(ViewProperties *item, Session::Notification notification, bool enabled)
{
    auto controller = qobject_cast<SessionController*>(item);
    auto topLevelSplitter = qobject_cast<ViewSplitter*>(controller->view()->parentWidget())->getToplevelSplitter();
    const int index = indexOf(topLevelSplitter);
    auto &state = _tabIconState[topLevelSplitter];

    if (enabled && (index != currentIndex() || controller->view()->hasCompositeFocus())) {
        state.notification = notification;
        updateIcon(item);
    } else if (!enabled && controller->view()->hasCompositeFocus()) {
        state.notification = Session::NoNotification;
        updateIcon(item);
    }
}

void TabbedViewContainer::updateSpecialState(ViewProperties *item)
{
    auto controller = qobject_cast<SessionController*>(item);
    auto topLevelSplitter = qobject_cast<ViewSplitter*>(controller->view()->parentWidget())->getToplevelSplitter();

    auto &state = _tabIconState[topLevelSplitter];
    state.readOnly = true;
    state.broadcast = true;
    const auto displays = topLevelSplitter->findChildren<TerminalDisplay*>();
    for (const auto display : displays) {
        if (!display->sessionController()->isReadOnly()) {
            state.readOnly = false;
        }
        if (!display->sessionController()->isCopyInputActive()) {
            state.broadcast = false;
        }
    }
    updateIcon(item);
}

void TabbedViewContainer::currentSessionControllerChanged(SessionController *controller)
{
    auto topLevelSplitter = qobject_cast<ViewSplitter*>(controller->view()->parentWidget())->getToplevelSplitter();
    const int index = indexOf(topLevelSplitter);

    if (index == currentIndex()) {
        // Active view changed in current tab - clear notifications
        auto &state = _tabIconState[topLevelSplitter];
        state.notification = Session::NoNotification;
    }

    updateTitle(qobject_cast<ViewProperties*>(controller));
    updateColor(qobject_cast<ViewProperties*>(controller));
    updateActivity(qobject_cast<ViewProperties*>(controller));
    updateSpecialState(qobject_cast<ViewProperties*>(controller));
}

void TabbedViewContainer::closeTerminalTab(int idx) {
    emit removeColor(idx);
    //TODO: This for should probably go to the ViewSplitter
    for (auto terminal : viewSplitterAt(idx)->findChildren<TerminalDisplay*>()) {
        terminal->sessionController()->closeSession();
    }
}

ViewManager *TabbedViewContainer::connectedViewManager()
{
    return _connectedViewManager;
}

void TabbedViewContainer::setNavigationVisibility(ViewManager::NavigationVisibility navigationVisibility) {
    if (navigationVisibility == ViewManager::NavigationNotSet) {
        return;
    }

    setTabBarAutoHide(navigationVisibility == ViewManager::ShowNavigationAsNeeded);
    if (navigationVisibility == ViewManager::AlwaysShowNavigation) {
        tabBar()->setVisible(true);
    } else if (navigationVisibility == ViewManager::AlwaysHideNavigation) {
        tabBar()->setVisible(false);
    }
}

void TabbedViewContainer::toggleMaximizeCurrentTerminal()
{
    if (auto *terminal = qobject_cast<TerminalDisplay*>(sender())) {
        terminal->setFocus(Qt::FocusReason::OtherFocusReason);
    }

    activeViewSplitter()->toggleMaximizeCurrentTerminal();
}

void TabbedViewContainer::moveTabLeft()
{
    if (currentIndex() == 0) {
        return;
    }
    tabBar()->moveTab(currentIndex(), currentIndex() -1);
}

void TabbedViewContainer::moveTabRight()
{
    if (currentIndex() == count() -1) {
        return;
    }
    tabBar()->moveTab(currentIndex(), currentIndex() + 1);
}

void TabbedViewContainer::setNavigationBehavior(int behavior)
{
    _newTabBehavior = static_cast<NewTabBehavior>(behavior);
}

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
#include "ViewContainer.h"
#include <config-konsole.h>

// Qt
#include <QTabBar>
#include <QMenu>
#include <QFile>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDrag>

// KDE
#include <KColorScheme>
#include <KColorUtils>
#include <KLocalizedString>
#include <KActionCollection>

// Konsole
#include "IncrementalSearchBar.h"
#include "ViewProperties.h"
#include "ProfileList.h"
#include "ViewManager.h"
#include "KonsoleSettings.h"
#include "SessionController.h"
#include "DetachableTabBar.h"
#include "TerminalDisplay.h"
#include "ViewSplitter.h"

// TODO Perhaps move everything which is Konsole-specific into different files

using namespace Konsole;


TabbedViewContainer::TabbedViewContainer(ViewManager *connectedViewManager, QWidget *parent) :
    QTabWidget(parent),
    _connectedViewManager(connectedViewManager),
    _newTabButton(new QToolButton()),
    _closeTabButton(new QToolButton()),
    _contextMenuTabIndex(-1),
    _navigationVisibility(ViewManager::NavigationVisibility::NavigationNotSet),
    _tabHistoryIndex(-1)
{
    setAcceptDrops(true);

    auto tabBarWidget = new DetachableTabBar();
    setTabBar(tabBarWidget);
    setDocumentMode(true);
    setMovable(true);
    connect(tabBarWidget, &DetachableTabBar::moveTabToWindow, this, &TabbedViewContainer::moveTabToWindow);
    tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    _newTabButton->setIcon(QIcon::fromTheme(QStringLiteral("document-new")));
    _newTabButton->setAutoRaise(true);
    connect(_newTabButton, &QToolButton::clicked, this, [this]{
        emit newViewRequest(this);
    });

    _closeTabButton->setIcon(QIcon::fromTheme(QStringLiteral("tab-close")));
    _closeTabButton->setAutoRaise(true);
    connect(_closeTabButton, &QToolButton::clicked, this, [this]{
       closeCurrentTab();
    });

    connect(tabBar(), &QTabBar::tabBarDoubleClicked, this,
        &Konsole::TabbedViewContainer::tabDoubleClicked);
    connect(tabBar(), &QTabBar::customContextMenuRequested, this,
        &Konsole::TabbedViewContainer::openTabContextMenu);

    //TODO: Fix Detach.
    // connect(tabBarWidget, &DetachableTabBar::detachTab, this, [this](int idx) {
    //    emit detachTab(this, terminalAt(idx));
    //});

    connect(this, &TabbedViewContainer::currentChanged, this, &TabbedViewContainer::currentTabChanged);

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

#if defined(ENABLE_DETACHING)
    /* TODO FIX DETACH
    auto detachAction = _contextPopupMenu->addAction(
        QIcon::fromTheme(QStringLiteral("tab-detach")),
        i18nc("@action:inmenu", "&Detach Tab"), this,
        [this] { emit detachTab(this, terminalAt(_contextMenuTabIndex)); }
    );
    detachAction->setObjectName(QStringLiteral("tab-detach"));
    */
#endif

    auto editAction = _contextPopupMenu->addAction(
        QIcon::fromTheme(QStringLiteral("edit-rename")),
        i18nc("@action:inmenu", "&Rename Tab..."), this,
        [this]{ renameTab(_contextMenuTabIndex); }
    );
    editAction->setObjectName(QStringLiteral("edit-rename"));

    auto closeAction = _contextPopupMenu->addAction(
        QIcon::fromTheme(QStringLiteral("tab-close")),
        i18nc("@action:inmenu", "Close Tab"), this,
        [this] { closeTerminalTab(_contextMenuTabIndex); }
    );
    closeAction->setObjectName(QStringLiteral("tab-close"));

    auto profileMenu = new QMenu();
    auto profileList = new ProfileList(false, profileMenu);
    profileList->syncWidgetActions(profileMenu, true);
    connect(profileList, &Konsole::ProfileList::profileSelected, this,
            [this](Profile::Ptr profile) { emit newViewWithProfileRequest(this, profile); });
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
    // TODO: Fix Detaching.
    /*
    const int id = terminalAt(index)->sessionController()->identifier();
    // This one line here will be removed as soon as I finish my new split handling.
    // it's hacky but it works.
    const auto widgets = window->findChildren<TabbedViewContainer*>();
    const auto currentPos = QCursor::pos();
    for(const auto dropWidget : widgets) {
        if (dropWidget->rect().contains(dropWidget->mapFromGlobal(currentPos))) {
            emit dropWidget->moveViewRequest(-1, id);
            removeView(terminalAt(index));
        }
    }
    */
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

    setCornerWidget( KonsoleSettings::showQuickButtons() ? _newTabButton : nullptr, Qt::TopLeftCorner);
    setCornerWidget( KonsoleSettings::showQuickButtons() ? _closeTabButton : nullptr, Qt::TopRightCorner);

    tabBar()->setExpanding(KonsoleSettings::expandTabWidth());
    tabBar()->update();
    if (isVisible() && KonsoleSettings::showQuickButtons()) {
        _newTabButton->setVisible(true);
        _closeTabButton->setVisible(true);
    }

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

void TabbedViewContainer::addView(TerminalDisplay *view, int index)
{
    auto viewSplitter = new ViewSplitter();
    viewSplitter->addTerminalDisplay(view, Qt::Horizontal);
    auto item = view->sessionController();
    if (index == -1) {
        addTab(viewSplitter, item->icon(), item->title());
    } else {
        insertTab(index, viewSplitter, item->icon(), item->title());
    }

    _tabHistory.append(view);
    connect(item, &Konsole::ViewProperties::titleChanged, this,
            &Konsole::TabbedViewContainer::updateTitle);
    connect(item, &Konsole::ViewProperties::iconChanged, this,
            &Konsole::TabbedViewContainer::updateIcon);
    connect(item, &Konsole::ViewProperties::activity, this,
            &Konsole::TabbedViewContainer::updateActivity);

    connect(viewSplitter, &ViewSplitter::destroyed, this, &TabbedViewContainer::viewDestroyed);
    emit viewAdded(view);
}

void TabbedViewContainer::viewDestroyed(QObject *view)
{
    auto widget = static_cast<ViewSplitter*>(view);
    const auto idx = indexOf(widget);

    removeTab(idx);
    forgetView(widget);
}

void TabbedViewContainer::forgetView(ViewSplitter *view)
{
    //TODO: Fix updateTabHistory
    // updateTabHistory(view, true);
    // emit viewRemoved(view);
    if (count() == 0) {
        emit empty(this);
    }
}

void TabbedViewContainer::removeView(TerminalDisplay *view)
{
    /* TODO: This is absolutely the wrong place.
     * We are removing a terminal display from a ViewSplitter,
     * this should be inside of the view splitter or something.
    */
    view->setParent(nullptr);
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

void TabbedViewContainer::activateLastUsedView(bool reverse)
{
    if (_tabHistory.count() <= 1) {
        return;
    }

    if (_tabHistoryIndex == -1) {
        _tabHistoryIndex = reverse ? _tabHistory.count() - 1 : 1;
    } else if (reverse) {
        if (_tabHistoryIndex == 0) {
            _tabHistoryIndex = _tabHistory.count() - 1;
        } else {
            _tabHistoryIndex--;
        }
    } else {
        if (_tabHistoryIndex >= _tabHistory.count() - 1) {
            _tabHistoryIndex = 0;
        } else {
            _tabHistoryIndex++;
        }
    }

    int index = indexOf(_tabHistory[_tabHistoryIndex]);
    setCurrentIndex(index);
}

// Jump to last view - this allows toggling between two views
// Using "Last Used Tabs" shortcut will cause "Toggle between two tabs"
// shortcut to be incorrect the first time.
void TabbedViewContainer::toggleLastUsedView()
{
    if (_tabHistory.count() <= 1) {
        return;
    }

    setCurrentIndex(indexOf(_tabHistory.at(1)));
}

void TabbedViewContainer::keyReleaseEvent(QKeyEvent* event)
{
    if (_tabHistoryIndex != -1 && event->modifiers() == Qt::NoModifier) {
        _tabHistoryIndex = -1;
        auto *active = qobject_cast<TerminalDisplay*>(currentWidget());
        if (active != _tabHistory[0]) {
            // Update the tab history now that we have ended the walk-through
            updateTabHistory(active);
        }
    }
}

void TabbedViewContainer::updateTabHistory(TerminalDisplay* view, bool remove)
{
    if (_tabHistoryIndex != -1 && !remove) {
        // Do not reorder the tab history while we are walking through it
        return;
    }

    for (int i = 0; i < _tabHistory.count(); ++i ) {
        if (_tabHistory[i] == view) {
            _tabHistory.removeAt(i);
            if (!remove) {
                _tabHistory.prepend(view);
            }
            break;
        }
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
        emit newViewRequest(this);
    }
}

void TabbedViewContainer::renameTab(int index)
{
    /* TODO: Fix renaming.
        The problem with the renaming right now is that many Terminals can be at a tab.
    */

    if (index != -1) {
//        terminalAt(index)->sessionController()->rename();
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
#if defined(ENABLE_DETACHING)
    for(auto action : _contextPopupMenu->actions()) {
        if (action->objectName() == QStringLiteral("tab-detach")) {
            action->setEnabled(count() > 1);
        }
    }
#endif

    /* This needs to nove away fro the tab or to lock every thing inside of it.
     * for now, disable.
     * */
    //
    // Add the read-only action
#if 0
    auto sessionController = terminalAt(_contextMenuTabIndex)->sessionController();

    if (sessionController != nullptr) {
        auto collection = sessionController->actionCollection();
        auto readonlyAction = collection->action(QStringLiteral("view-readonly"));
        if (readonlyAction != nullptr) {
            const auto readonlyActions = _contextPopupMenu->actions();
            _contextPopupMenu->insertAction(readonlyActions.last(), readonlyAction);
        }

        // Disable tab rename
        for (auto &action : _contextPopupMenu->actions()) {
            if (action->objectName() == QStringLiteral("edit-rename")) {
                action->setEnabled(!sessionController->isReadOnly());
                break;
            }
        }
    }
#endif
    _contextPopupMenu->exec(tabBar()->mapToGlobal(point));
}

void TabbedViewContainer::currentTabChanged(int index)
{
    if (index != -1) {
        auto view = widget(index)->findChild<TerminalDisplay*>();
        view->setFocus();
        updateTabHistory(view);
        emit activeViewChanged(view);
        setTabActivity(index, false);
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

void TabbedViewContainer::updateActivity(ViewProperties *item)
{
    auto controller = qobject_cast<SessionController*>(item);
    auto index = indexOf(controller->view());
    if (index != currentIndex()) {
        setTabActivity(index, true);
    }
}

void TabbedViewContainer::updateTitle(ViewProperties *item)
{
    auto controller = qobject_cast<SessionController*>(item);

    const int index = indexOf(controller->view());
    QString tabText = item->title();

    setTabToolTip(index, tabText);

    // To avoid having & replaced with _ (shortcut indicator)
    tabText.replace(QLatin1Char('&'), QLatin1String("&&"));
    setTabText(index, tabText);
}

void TabbedViewContainer::updateIcon(ViewProperties *item)
{
    auto controller = qobject_cast<SessionController*>(item);
    const int index = indexOf(controller->view());
    setTabIcon(index, item->icon());
}

void TabbedViewContainer::closeTerminalTab(int idx) {
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

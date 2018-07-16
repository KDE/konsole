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

// TODO Perhaps move everything which is Konsole-specific into different files

using namespace Konsole;

TabbedViewContainer::TabbedViewContainer(ViewManager *connectedViewManager, QWidget *parent) :
    QTabWidget(parent),
    _connectedViewManager(connectedViewManager),
    _newTabButton(new QToolButton())
{
    setDocumentMode(true);
    tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);

    _newTabButton->setIcon(QIcon::fromTheme(QStringLiteral("document-new")));
    _newTabButton->setAutoRaise(true);

    connect(_newTabButton, &QToolButton::clicked, this, [this]{
        emit newViewRequest();
    });

    connect(tabBar(), &QTabBar::tabCloseRequested,
            this, &TabbedViewContainer::closeTerminalTab);

    connect(tabBar(), &QTabBar::tabBarDoubleClicked, this,
            &Konsole::TabbedViewContainer::tabDoubleClicked);
    connect(tabBar(), &QTabBar::customContextMenuRequested, this,
            &Konsole::TabbedViewContainer::openTabContextMenu);

    connect(this, &TabbedViewContainer::currentChanged, this, [this](int index) {
        if (index != -1) {
            widget(index)->setFocus();
        }
    });


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
//    _contextPopupMenu->addAction(QIcon::fromTheme(QStringLiteral("tab-detach")),
//                                 i18nc("@action:inmenu", "&Detach Tab"), this,
//                                 SLOT(tabContextMenuDetachTab()));
#endif

    auto editAction = _contextPopupMenu->addAction(QIcon::fromTheme(QStringLiteral("edit-rename")),
                                 i18nc("@action:inmenu", "&Rename Tab..."), this,
                                 SLOT(tabContextMenuRenameTab()));
    editAction->setObjectName(QStringLiteral("edit-rename"));

    auto profileMenu = new QMenu();
    auto profileList = new ProfileList(false, profileMenu);
    profileList->syncWidgetActions(profileMenu, true);
    connect(profileList, &Konsole::ProfileList::profileSelected, this,
            static_cast<void (TabbedViewContainer::*)(Profile::Ptr)>(&Konsole::TabbedViewContainer::newViewRequest));
  //  setNewViewMenu(profileMenu);

    konsoleConfigChanged();
    connect(KonsoleSettings::self(), &KonsoleSettings::configChanged, this, &TabbedViewContainer::konsoleConfigChanged);
}

TabbedViewContainer::~TabbedViewContainer()
{
    for(int i = 0, end = count(); i < end; i++) {
        auto view = widget(i);
        disconnect(view, &QWidget::destroyed, this, &Konsole::TabbedViewContainer::viewDestroyed);
    }

    emit destroyed(this);
}

void TabbedViewContainer::konsoleConfigChanged()
{
    setTabBarAutoHide((bool) KonsoleSettings::tabBarVisibility());
    setTabPosition((QTabWidget::TabPosition) KonsoleSettings::tabBarPosition());
    setTabsClosable(KonsoleSettings::showQuickButtons());
    setCornerWidget( KonsoleSettings::showQuickButtons() ? _newTabButton : nullptr, Qt::TopLeftCorner);
    if (KonsoleSettings::tabBarUseUserStyleSheet()) {
        setCssFromFile(KonsoleSettings::tabBarUserStyleSheetFile());
    }
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

    QString styleSheetText;
    QTextStream in(&file);
    while (!in.atEnd()) {
        styleSheetText.append(in.readLine());
    }
    setStyleSheet(styleSheetText);
}

void TabbedViewContainer::moveActiveView(MoveDirection direction)
{
    const int currentIndex = indexOf(currentWidget());
    int newIndex = -1;

    switch (direction) {
    case MoveViewLeft:
        newIndex = qMax(currentIndex - 1, 0);
        break;
    case MoveViewRight:
        newIndex = qMin(currentIndex + 1, count() - 1);
        break;
    }

    Q_ASSERT(newIndex != -1);
    auto swappedWidget = widget(newIndex);
    auto currentWidget = widget(currentIndex);
    removeTab(newIndex);
    removeTab(currentIndex);
}

void TabbedViewContainer::addView(QWidget *view, ViewProperties *item, int index)
{
    if (index == -1) {
        addTab(view, item->icon(), item->title());
    } else {
        insertTab(index, view, item->icon(), item->title());
    }

    _navigation[view] = item;
    connect(view, &QWidget::destroyed, this, &Konsole::TabbedViewContainer::viewDestroyed);
    emit viewAdded(view, item);
}

void TabbedViewContainer::viewDestroyed(QObject *view)
{
    auto widget = static_cast<QWidget*>(view);
    const auto idx = indexOf(widget);

    removeTab(idx);
    forgetView(widget);
}

void TabbedViewContainer::forgetView(QWidget *view)
{
    _navigation.remove(view);
    emit viewRemoved(view);
    if (count() == 0) {
        emit empty(this);
    }
}

void TabbedViewContainer::removeView(QWidget *view)
{
    const int idx = indexOf(view);
    disconnect(view, &QWidget::destroyed, this, &Konsole::TabbedViewContainer::viewDestroyed);
    removeTab(idx);
    forgetView(view);
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

ViewProperties *TabbedViewContainer::viewProperties(QWidget *view) const
{
    Q_ASSERT(_navigation.contains(view));
    return _navigation[view];
}

QList<QWidget *> TabbedViewContainer::widgetsForItem(ViewProperties *item) const
{
    return _navigation.keys(item);
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
        _navigation[widget(index)]->rename();
    }
}

void TabbedViewContainer::openTabContextMenu(const QPoint &point)
{
    if (point.isNull()) {
        return;
    }

    const int contextMenuTabIndex = tabBar()->tabAt(point);
    if (contextMenuTabIndex < 0) {
        return;
    }

#if defined(ENABLE_DETACHING)
    // Enable 'Detach Tab' menu item only if there is more than 1 tab
    // Note: the code is coupled with that action's position within the menu
    QAction *detachAction = _contextPopupMenu->actions().at(0);
    detachAction->setEnabled(count() > 1);
#endif

    // Add the read-only action
    auto controller = _navigation[widget(contextMenuTabIndex)];
    auto sessionController = qobject_cast<SessionController*>(controller);

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

    _contextPopupMenu->exec(tabBar()->mapToGlobal(point));
}

void TabbedViewContainer::currentTabChanged(int index)
{
    setCurrentIndex(index);
    if (widget(index) != nullptr) {
        emit activeViewChanged(widget(index));
    }

    // clear activity indicators
    setTabActivity(index, false);
}

void TabbedViewContainer::wheelScrolled(int delta)
{
    if (delta < 0) {
        activateNextView();
    } else {
        activatePreviousView();
    }
}

void TabbedViewContainer::addViewWidget(QWidget *view, int index)
{
    ViewProperties *item = viewProperties(view);
    connect(item, &Konsole::ViewProperties::titleChanged, this,
            &Konsole::TabbedViewContainer::updateTitle);
    connect(item, &Konsole::ViewProperties::iconChanged, this,
            &Konsole::TabbedViewContainer::updateIcon);
    connect(item, &Konsole::ViewProperties::activity, this,
            &Konsole::TabbedViewContainer::updateActivity);

    insertTab(index, view, item->icon(), item->title());
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
    foreach (QWidget *widget, widgetsForItem(item)) {
        const int index = indexOf(widget);

        if (index != currentIndex()) {
            setTabActivity(index, true);
        }
    }
}

void TabbedViewContainer::updateTitle(ViewProperties *item)
{
    foreach (QWidget *widget, widgetsForItem(item)) {
        const int index = indexOf(widget);
        QString tabText = item->title();

        setTabToolTip(index, tabText);

        // To avoid having & replaced with _ (shortcut indicator)
        tabText.replace(QLatin1Char('&'), QLatin1String("&&"));
        setTabText(index, tabText);
    }
}

void TabbedViewContainer::updateIcon(ViewProperties *item)
{
    foreach (QWidget *widget, widgetsForItem(item)) {
        const int index = indexOf(widget);
        setTabIcon(index, item->icon());
    }
}

void TabbedViewContainer::closeTerminalTab(int idx) {
    auto currWidget = widget(idx);
    auto controller = qobject_cast<SessionController *>(_navigation[currWidget]);
    controller->closeSession();
}

ViewManager *TabbedViewContainer::connectedViewManager()
{
    return _connectedViewManager;
}

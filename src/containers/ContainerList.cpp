/*
    SPDX-FileCopyrightText: 2026 Konsole Developers

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "ContainerList.h"

// Qt
#include <QActionGroup>
#include <QMenu>

// KDE
#include <KLocalizedString>

// Konsole
#include "ContainerInfo.h"
#include "ContainerRegistry.h"

using Konsole::ContainerList;

ContainerList::ContainerList(QObject *parent)
    : QObject(parent)
    , _menu(new QMenu(i18nc("@action:inmenu Submenu title for container list", "New Tab in Container")))
    , _group(new QActionGroup(this))
{
    _menu->setIcon(QIcon::fromTheme(QStringLiteral("container")));

    connect(_menu, &QMenu::aboutToShow, this, &ContainerList::refreshContainers);
    connect(_group, &QActionGroup::triggered, this, &ContainerList::triggered);

    // Do an initial check so we can hide the menu entirely when empty
    refreshContainers();
}

void ContainerList::refreshContainers()
{
    // Remove old actions
    const auto oldActions = _group->actions();
    for (QAction *action : oldActions) {
        _group->removeAction(action);
        delete action;
    }
    _menu->clear();

    auto *registry = ContainerRegistry::instance();
    if (!registry->isEnabled()) {
        _menu->menuAction()->setVisible(false);
        return;
    }

    const QList<ContainerInfo> containers = registry->listAllContainers();
    if (containers.isEmpty()) {
        _menu->menuAction()->setVisible(false);
        return;
    }

    _menu->menuAction()->setVisible(true);

    for (const ContainerInfo &info : containers) {
        auto *action = new QAction(_group);
        action->setText(info.displayName);

        if (!info.iconName.isEmpty()) {
            action->setIcon(QIcon::fromTheme(info.iconName));
        }

        // Store the ContainerInfo in the action's data
        action->setData(QVariant::fromValue(info));

        _menu->addAction(action);
    }
}

void ContainerList::triggered(QAction *action)
{
    const ContainerInfo info = action->data().value<ContainerInfo>();
    if (info.isValid()) {
        Q_EMIT containerSelected(info);
    }
}

QMenu *ContainerList::menu() const
{
    return _menu;
}

#include "moc_ContainerList.cpp"

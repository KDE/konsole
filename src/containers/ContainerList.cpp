/*
    SPDX-FileCopyrightText: 2026 Konsole Developers

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "ContainerList.h"

// Qt
#include <QActionGroup>
#include <QMenu>

// Konsole
#include "ContainerInfo.h"
#include "ContainerRegistry.h"
#include "IContainerDetector.h"

using Konsole::ContainerList;

ContainerList::ContainerList(QObject *parent)
    : QObject(parent)
    , _group(new QActionGroup(this))
{
    connect(_group, &QActionGroup::triggered, this, &ContainerList::triggered);
    refreshContainers();
}

bool ContainerList::hasContainers() const
{
    return !_containers.isEmpty();
}

void ContainerList::refreshContainers()
{
    // Remove old actions
    const auto oldActions = _group->actions();
    for (QAction *action : oldActions) {
        _group->removeAction(action);
        delete action;
    }
    _containers.clear();

    auto *registry = ContainerRegistry::instance();
    if (!registry->isEnabled()) {
        return;
    }

    _containers = registry->listAllContainers();

    for (const ContainerInfo &info : _containers) {
        auto *action = new QAction(_group);
        action->setText(info.name);

        if (!info.iconName.isEmpty()) {
            action->setIcon(QIcon::fromTheme(info.iconName));
        }

        action->setData(QVariant::fromValue(info));
    }
}

void ContainerList::addContainerSections(QMenu *menu)
{
    if (_containers.isEmpty()) {
        return;
    }

    const IContainerDetector *currentDetector = nullptr;
    const auto actions = _group->actions();

    for (QAction *action : actions) {
        const ContainerInfo info = action->data().value<ContainerInfo>();
        if (info.detector != currentDetector) {
            currentDetector = info.detector;
            menu->addSection(currentDetector->displayName());
        }
        menu->addAction(action);
    }
}

void ContainerList::triggered(QAction *action)
{
    const ContainerInfo info = action->data().value<ContainerInfo>();
    if (info.isValid()) {
        Q_EMIT containerSelected(info);
    }
}

#include "moc_ContainerList.cpp"

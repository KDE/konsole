// This file was part of the KDE libraries
// SPDX-FileCopyrightText: 2022 Tao Guo <guotao945@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "quickcommandsmodel.h"
#include "quickcommanddata.h"

#include <KConfig>
#include <KConfigGroup>

QuickCommandsModel::QuickCommandsModel(QObject *parent)
    : QStandardItemModel(parent)
{
    load();
}

QuickCommandsModel::~QuickCommandsModel() noexcept
{
    save();
}

void QuickCommandsModel::load()
{
    auto config = KConfig(QStringLiteral("konsolequickcommandsconfig"), KConfig::OpenFlag::SimpleConfig);
    for (const QString &groupName : config.groupList()) {
        KConfigGroup group = config.group(groupName);
        addTopLevelItem(groupName);
        for (const QString &commandGroup : group.groupList()) {
            QuickCommandData data;
            KConfigGroup element = group.group(commandGroup);
            data.name = element.readEntry("name");
            data.tooltip = element.readEntry("tooltip");
            data.command = element.readEntry("command");
            addChildItem(data, groupName);
        }
    }
}

void QuickCommandsModel::save()
{
    auto config = KConfig(QStringLiteral("konsolequickcommandsconfig"), KConfig::OpenFlag::SimpleConfig);
    for (const QString &groupName : config.groupList()) {
        config.deleteGroup(groupName);
    }
    for (int i = 0, end = invisibleRootItem()->rowCount(); i < end; i++) {
        QStandardItem *groupItem = invisibleRootItem()->child(i);
        const QString groupName = groupItem->text();
        KConfigGroup baseGroup = config.group(groupName);
        for (int j = 0, end2 = groupItem->rowCount(); j < end2; j++) {
            QStandardItem *item = groupItem->child(j);
            const auto data = item->data(QuickCommandRole).value<QuickCommandData>();
            KConfigGroup element = baseGroup.group(data.name);
            element.writeEntry("name", data.name);
            element.writeEntry("tooltip", data.tooltip);
            element.writeEntry("command", data.command);
        }
    }

    config.sync();
}

QStringList QuickCommandsModel::groups() const
{
    QStringList retList;
    for (int i = 0, end = invisibleRootItem()->rowCount(); i < end; i++) {
        retList.push_back(invisibleRootItem()->child(i)->text());
    }
    return retList;
}

QStandardItem *QuickCommandsModel::addTopLevelItem(const QString &groupName)
{
    for (int i = 0, end = invisibleRootItem()->rowCount(); i < end; i++) {
        if (invisibleRootItem()->child(i)->text() == groupName) {
            return nullptr;
        }
    }
    auto *newItem = new QStandardItem();
    newItem->setText(groupName);
    invisibleRootItem()->appendRow(newItem);
    invisibleRootItem()->sortChildren(0);

    return newItem;
}

bool QuickCommandsModel::addChildItem(const QuickCommandData &data, const QString &groupName)
{
    QStandardItem *parentItem = nullptr;
    for (int i = 0, end = invisibleRootItem()->rowCount(); i < end; i++) {
        if (invisibleRootItem()->child(i)->text() == groupName) {
            parentItem = invisibleRootItem()->child(i);
            break;
        }
    }

    if (!parentItem)
        parentItem = addTopLevelItem(groupName);
    for (int i = 0, end = parentItem->rowCount(); i < end; i++) {
        if (parentItem->child(i)->text() == data.name)
            return false;
    }

    auto item = new QStandardItem();
    updateItem(item, data);
    parentItem->appendRow(item);
    parentItem->sortChildren(0);
    return true;
}

bool QuickCommandsModel::editChildItem(const QuickCommandData &data, const QModelIndex &idx, const QString &groupName)
{
    QStandardItem *item = itemFromIndex(idx);
    QStandardItem *parentItem = item->parent();
    for (int i = 0, end = parentItem->rowCount(); i < end; i++) {
        if (parentItem->child(i)->text() == data.name && parentItem->child(i) != item)
            return false;
    }
    if (groupName != parentItem->text()) {
        if (!addChildItem(data, groupName))
            return false;
        parentItem->removeRow(item->row());
    } else {
        updateItem(item, data);
        item->parent()->sortChildren(0);
    }

    return true;
}

void QuickCommandsModel::updateItem(QStandardItem *item, const QuickCommandData &data)
{
    item->setData(QVariant::fromValue(data), QuickCommandRole);
    item->setText(data.name);
    if (data.tooltip.trimmed().isEmpty())
        item->setToolTip(data.command);
    else
        item->setToolTip(data.tooltip);
}

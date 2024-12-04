/*
    SPDX-FileCopyrightText: 2024 Troy Hoover <troytjh98@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "SearchTabsModel.h"

// Konsole
#include "ViewProperties.h"

using namespace Konsole;

SearchTabsModel::SearchTabsModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int SearchTabsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return (int)m_tabEntries.size();
}

int SearchTabsModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant SearchTabsModel::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid()) {
        return {};
    }

    const TabEntry &tab = m_tabEntries.at(idx.row());
    switch (role) {
    case Qt::DisplayRole:
    case Role::Name:
        return tab.name;
    case Role::Score:
        return tab.score;
    case Role::View:
        return tab.view;
    default:
        return {};
    }

    return {};
}

void SearchTabsModel::refresh(ViewManager *viewManager)
{
    QList<ViewProperties *> viewProperties = viewManager->viewProperties();

    QVector<TabEntry> tabs;
    tabs.reserve(viewProperties.size());
    for (const auto view : std::as_const(viewProperties)) {
        tabs.push_back(TabEntry{view->title(), view->identifier(), -1});
    }

    beginResetModel();
    m_tabEntries = std::move(tabs);
    endResetModel();
}

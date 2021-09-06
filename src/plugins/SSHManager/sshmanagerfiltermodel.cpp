/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sshmanagerfiltermodel.h"

SSHManagerFilterModel::SSHManagerFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

bool SSHManagerFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    auto text = filterRegularExpression().pattern();
    if (text.isEmpty()) {
        return true;
    }

    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    if (sourceModel()->rowCount(idx) != 0) {
        return true;
    }

    bool result = idx.data(Qt::DisplayRole).toString().toLower().contains(text.toLower());

    return m_invertFilter == false ? result : !result;
}

void SSHManagerFilterModel::setInvertFilter(bool invert)
{
    m_invertFilter = invert;
    invalidateFilter();
}

/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SSHMANAGERFILTERMODEL_H
#define SSHMANAGERFILTERMODEL_H

#include <QSortFilterProxyModel>

class SSHManagerFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    SSHManagerFilterModel(QObject *parent);
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    void setInvertFilter(bool invert);

private:
    bool m_invertFilter = false;
};

#endif

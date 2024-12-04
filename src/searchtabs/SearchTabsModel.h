/*
    SPDX-FileCopyrightText: 2024 Troy Hoover <troytjh98@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// Qt
#include <QAbstractTableModel>
#include <QStringList>
#include <QVector>

// Konsole
#include "ViewManager.h"

namespace Konsole
{

struct TabEntry {
    QString name;
    int view;
    int score = -1;
};

class SearchTabsModel : public QAbstractTableModel
{
public:
    enum Role {
        Name = Qt::UserRole + 1,
        View,
        Score,
    };
    explicit SearchTabsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &idx, int role) const override;
    void refresh(ViewManager *viewManager);

    bool isValid(int row) const
    {
        return row >= 0 && row < m_tabEntries.size();
    }

    void setScoreForIndex(int row, int score)
    {
        m_tabEntries[row].score = score;
    }

    const QString &idxToName(int row) const
    {
        return m_tabEntries.at(row).name;
    }

    int idxScore(const QModelIndex &idx) const
    {
        if (!idx.isValid()) {
            return {};
        }
        return m_tabEntries.at(idx.row()).score;
    }

private:
    QVector<TabEntry> m_tabEntries;
};

}

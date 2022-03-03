// This file was part of the KDE libraries
// SPDX-FileCopyrightText: 2022 Tao Guo <guotao945@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef QUICKCOMMANDSMODEL_H
#define QUICKCOMMANDSMODEL_H

#include <qstandarditemmodel.h>

class QuickCommandData;

class QuickCommandsModel : public QStandardItemModel
{
    Q_OBJECT
public:
    enum Roles { QuickCommandRole = Qt::UserRole + 1 };
    explicit QuickCommandsModel(QObject *parent = nullptr);
    ~QuickCommandsModel() override;

    QStringList groups() const;
    bool addChildItem(const QuickCommandData &data, const QString &groupName);
    bool editChildItem(const QuickCommandData &data, const QModelIndex &idx, const QString &groupName);

private:
    void load();
    void save();
    void updateItem(QStandardItem *item, const QuickCommandData &data);
    QStandardItem *addTopLevelItem(const QString &groupName);
};

#endif // QUICKCOMMANDSMODEL_H

/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SSHMANAGERMODEL_H
#define SSHMANAGERMODEL_H

#include <QStandardItemModel>

#include <memory>

class SSHConfigurationData;

class SSHManagerModel : public QStandardItemModel
{
    Q_OBJECT
public:
    enum Roles {
        SSHRole = Qt::UserRole + 1,
    };

    SSHManagerModel(QObject *parent = nullptr);
    ~SSHManagerModel() override;

    QStandardItem *addTopLevelItem(const QString &toplevel);
    void addChildItem(const SSHConfigurationData &config, const QString &parentName);
    void editChildItem(const SSHConfigurationData &config, const QModelIndex &idx);
    void removeIndex(const QModelIndex &idx);
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QStringList folders() const;

    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void startImportFromSshConfig();
    void importFromSshConfigFile(const QString &file);
    void load();
    void save();
};

#endif

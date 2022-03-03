/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SSHMANAGERMODEL_H
#define SSHMANAGERMODEL_H

#include <QFileSystemWatcher>
#include <QMap>
#include <QStandardItemModel>

#include <memory>
#include <optional>

namespace Konsole
{
class SessionController;
class Session;
}

class SSHConfigurationData;

class SSHManagerModel : public QStandardItemModel
{
    Q_OBJECT
public:
    enum Roles {
        SSHRole = Qt::UserRole + 1,
    };

    explicit SSHManagerModel(QObject *parent = nullptr);
    ~SSHManagerModel() override;

    void setSessionController(Konsole::SessionController *controller);

    /** Connected to Session::hostnameChanged, tries to set the profile to
     * the current configured profile for the specified SSH host
     */
    Q_SLOT void triggerProfileChange(const QString &sshHost);

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

    bool hasHost(const QString &hostName) const;
    std::optional<QString> profileForHost(const QString &host) const;

private:
    QStandardItem *m_sshConfigTopLevelItem = nullptr;
    QFileSystemWatcher m_sshConfigWatcher;
    Konsole::Session *m_session = nullptr;

    QHash<Konsole::Session *, QString> m_sessionToProfileName;
};

#endif

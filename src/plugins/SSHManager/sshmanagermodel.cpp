/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sshmanagermodel.h"

#include <QStandardItem>

#include <KLocalizedString>

#include <KConfig>
#include <KConfigGroup>

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QTextStream>

#include "profile/ProfileManager.h"
#include "session/Session.h"
#include "session/SessionController.h"
#include "session/SessionManager.h"

#include "profile/ProfileManager.h"
#include "profile/ProfileModel.h"

#include "sshconfigurationdata.h"

Q_LOGGING_CATEGORY(SshManagerPlugin, "org.kde.konsole.plugin.sshmanager")

namespace
{
const QString sshDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + QStringLiteral("/.ssh/");
}

SSHManagerModel::SSHManagerModel(QObject *parent)
    : QStandardItemModel(parent)
{
    load();
    if (!m_sshConfigTopLevelItem) {
        // this also sets the m_sshConfigTopLevelItem if the text is `SSH Config`.
        addTopLevelItem(i18n("SSH Config"));
    }
    if (invisibleRootItem()->rowCount() == 0) {
        addTopLevelItem(i18n("Default"));
    }
    if (QFileInfo::exists(sshDir + QStringLiteral("config"))) {
        m_sshConfigWatcher.addPath(sshDir + QStringLiteral("config"));
        connect(&m_sshConfigWatcher, &QFileSystemWatcher::fileChanged, this, [this] {
            startImportFromSshConfig();
        });
        startImportFromSshConfig();
    }
}

SSHManagerModel::~SSHManagerModel() noexcept
{
    save();
}

QStandardItem *SSHManagerModel::addTopLevelItem(const QString &name)
{
    for (int i = 0, end = invisibleRootItem()->rowCount(); i < end; i++) {
        if (invisibleRootItem()->child(i)->text() == name) {
            return nullptr;
        }
    }

    auto *newItem = new QStandardItem();
    newItem->setText(name);
    newItem->setToolTip(i18n("%1 is a folder for SSH entries", name));
    invisibleRootItem()->appendRow(newItem);
    invisibleRootItem()->sortChildren(0);

    if (name == i18n("SSH Config")) {
        m_sshConfigTopLevelItem = newItem;
    }

    return newItem;
}

void SSHManagerModel::addChildItem(const SSHConfigurationData &config, const QString &parentName)
{
    QStandardItem *parentItem = nullptr;
    for (int i = 0, end = invisibleRootItem()->rowCount(); i < end; i++) {
        if (invisibleRootItem()->child(i)->text() == parentName) {
            parentItem = invisibleRootItem()->child(i);
            break;
        }
    }

    if (!parentItem) {
        parentItem = addTopLevelItem(parentName);
    }

    auto newChild = new QStandardItem();
    newChild->setData(QVariant::fromValue(config), SSHRole);
    newChild->setText(config.name);
    newChild->setToolTip(i18n("Host: %1", config.host));
    parentItem->appendRow(newChild);
    parentItem->sortChildren(0);
}

std::optional<QString> SSHManagerModel::profileForHost(const QString &host) const
{
    auto *root = invisibleRootItem();

    // iterate through folders:
    for (int i = 0, end = root->rowCount(); i < end; ++i) {
        // iterate throguh the items on folders;
        auto folder = root->child(i);
        for (int e = 0, inner_end = folder->rowCount(); e < inner_end; ++e) {
            QStandardItem *ssh_item = folder->child(e);
            auto data = ssh_item->data(SSHRole).value<SSHConfigurationData>();

            // Return the profile name if the host matches.
            if (data.host == host) {
                return data.profileName;
            }
        }
    }

    return {};
}

bool SSHManagerModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    const bool ret = QStandardItemModel::setData(index, value, role);
    invisibleRootItem()->sortChildren(0);
    return ret;
}

void SSHManagerModel::editChildItem(const SSHConfigurationData &config, const QModelIndex &idx)
{
    QStandardItem *item = itemFromIndex(idx);
    item->setData(QVariant::fromValue(config), SSHRole);
    item->setData(config.name, Qt::DisplayRole);
    item->parent()->sortChildren(0);
}

QStringList SSHManagerModel::folders() const
{
    QStringList retList;
    for (int i = 0, end = invisibleRootItem()->rowCount(); i < end; i++) {
        retList.push_back(invisibleRootItem()->child(i)->text());
    }
    return retList;
}

bool SSHManagerModel::hasHost(const QString &host) const
{
    // runs in O(N), should be ok for the amount of data peophe have.
    for (int i = 0, end = invisibleRootItem()->rowCount(); i < end; i++) {
        QStandardItem *iChild = invisibleRootItem()->child(i);
        for (int e = 0, crend = iChild->rowCount(); e < crend; e++) {
            QStandardItem *eChild = iChild->child(e);
            const auto data = eChild->data(SSHManagerModel::Roles::SSHRole).value<SSHConfigurationData>();
            if (data.host == host) {
                return true;
            }
        }
    }
    return false;
}

void SSHManagerModel::setSessionController(Konsole::SessionController *controller)
{
    if (m_session) {
        disconnect(m_session, nullptr, this, nullptr);
    }
    m_session = controller->session();
    Q_ASSERT(m_session);

    connect(m_session, &QObject::destroyed, this, [this] {
        m_session = nullptr;
    });

    connect(m_session, &Konsole::Session::hostnameChanged, this, &SSHManagerModel::triggerProfileChange);
}

void SSHManagerModel::triggerProfileChange(const QString &sshHost)
{
    auto *sm = Konsole::SessionManager::instance();
    QString profileToLoad;

    // This code is messy, Let's see if this can explain a bit.
    // This if sequence tries to do two things:
    // Stores the current profile, when we trigger a change - but only
    // if our hostname is the localhost.
    // and when we change to another profile (or go back to the local host)
    // we need to restore the previous profile, not go to the default one.
    // so this whole mess of m_sessionToProfile is just to load it correctly
    // later on.
    if (sshHost == QSysInfo::machineHostName()) {
        // It's the first time that we call this, using the hostname as host.
        // just prepare the session as a empty profile and set it as initialized to false.
        if (!m_sessionToProfileName.contains(m_session)) {
            m_sessionToProfileName[m_session] = QString();
            return;
        }

        // We just loaded the localhost again, after a probable different profile.
        // mark the profile to load as the one we stored previously.
        else if (m_sessionToProfileName[m_session].count()) {
            profileToLoad = m_sessionToProfileName[m_session];
            m_sessionToProfileName.remove(m_session);
        }
    } else {
        // We just loaded a hostname that's not the localhost. save the current profile
        // so we can restore it later on, and load the profile for it.
        if (m_sessionToProfileName[m_session].isEmpty()) {
            m_sessionToProfileName[m_session] = m_session->profile();
        }
    }

    // end of really bad code. can someone think of a better algorithm for this?

    if (profileToLoad.isEmpty()) {
        std::optional<QString> profileName = profileForHost(sshHost);
        if (profileName) {
            profileToLoad = *profileName;
        }
    }

    auto profiles = Konsole::ProfileManager::instance()->allProfiles();
    auto findIt = std::find_if(std::begin(profiles), std::end(profiles), [&profileToLoad](const Konsole::Profile::Ptr &pr) {
        return pr->name() == profileToLoad;
    });
    if (findIt == std::end(profiles)) {
        return;
    }

    sm->setSessionProfile(m_session, *findIt);
}

void SSHManagerModel::load()
{
    auto config = KConfig(QStringLiteral("konsolesshconfig"), KConfig::OpenFlag::SimpleConfig);
    for (const QString &groupName : config.groupList()) {
        KConfigGroup group = config.group(groupName);
        addTopLevelItem(groupName);
        for (const QString &sessionName : group.groupList()) {
            SSHConfigurationData data;
            KConfigGroup sessionGroup = group.group(sessionName);
            data.host = sessionGroup.readEntry("hostname");
            data.name = sessionGroup.readEntry("identifier");
            data.port = sessionGroup.readEntry("port");
            data.profileName = sessionGroup.readEntry("profileName");
            data.username = sessionGroup.readEntry("username");
            data.sshKey = sessionGroup.readEntry("sshkey");
            data.useSshConfig = sessionGroup.readEntry<bool>("useSshConfig", false);
            data.importedFromSshConfig = sessionGroup.readEntry<bool>("importedFromSshConfig", false);
            addChildItem(data, groupName);
        }
    }
}

void SSHManagerModel::save()
{
    auto config = KConfig(QStringLiteral("konsolesshconfig"), KConfig::OpenFlag::SimpleConfig);
    for (const QString &groupName : config.groupList()) {
        config.deleteGroup(groupName);
    }

    for (int i = 0, end = invisibleRootItem()->rowCount(); i < end; i++) {
        QStandardItem *groupItem = invisibleRootItem()->child(i);
        const QString groupName = groupItem->text();
        KConfigGroup baseGroup = config.group(groupName);
        for (int e = 0, rend = groupItem->rowCount(); e < rend; e++) {
            QStandardItem *sshElement = groupItem->child(e);
            const auto data = sshElement->data(SSHRole).value<SSHConfigurationData>();
            KConfigGroup sshGroup = baseGroup.group(data.name.trimmed());
            sshGroup.writeEntry("hostname", data.host.trimmed());
            sshGroup.writeEntry("identifier", data.name.trimmed());
            sshGroup.writeEntry("port", data.port.trimmed());
            sshGroup.writeEntry("profileName", data.profileName.trimmed());
            sshGroup.writeEntry("sshkey", data.sshKey.trimmed());
            sshGroup.writeEntry("useSshConfig", data.useSshConfig);
            sshGroup.writeEntry("username", data.username);
            sshGroup.writeEntry("importedFromSshConfig", data.importedFromSshConfig);
        }
    }

    config.sync();
}

Qt::ItemFlags SSHManagerModel::flags(const QModelIndex &index) const
{
    if (indexFromItem(invisibleRootItem()) == index.parent()) {
        return QStandardItemModel::flags(index);
    } else {
        return QStandardItemModel::flags(index) & ~Qt::ItemIsEditable;
    }
}

void SSHManagerModel::removeIndex(const QModelIndex &idx)
{
    if (idx.data(Qt::DisplayRole) == i18n("SSH Config")) {
        m_sshConfigTopLevelItem = nullptr;
    }

    removeRow(idx.row(), idx.parent());
}

void SSHManagerModel::startImportFromSshConfig()
{
    importFromSshConfigFile(sshDir + QStringLiteral("config"));
}

void SSHManagerModel::importFromSshConfigFile(const QString &file)
{
    QFile sshConfig(file);
    if (!sshConfig.open(QIODevice::ReadOnly)) {
        qCDebug(SshManagerPlugin) << "Can't open config file";
    }
    QTextStream stream(&sshConfig);
    QString line;

    SSHConfigurationData data;

    // If we hit a *, we ignore till the next Host.
    bool ignoreEntry = false;
    while (stream.readLineInto(&line)) {
        // ignore comments
        if (line.startsWith(QStringLiteral("#"))) {
            continue;
        }

        QStringList lists = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        // ignore lines that are not "Type Value"
        if (lists.count() != 2) {
            continue;
        }

        if (lists.at(0) == QStringLiteral("Import")) {
            if (lists.at(1).contains(QLatin1Char('*'))) {
                // TODO: We don't handle globbing yet.
                continue;
            }

            importFromSshConfigFile(sshDir + lists.at(1));
            continue;
        }

        if (lists.at(0) == QStringLiteral("Host")) {
            if (line.contains(QLatin1Char('*'))) {
                // Panic, ignore everything until the next Host appears.
                ignoreEntry = true;
                continue;
            } else {
                ignoreEntry = false;
            }

            // When we hit this, that means that we just finished reading the
            // *previous* host. so we need to add it to the list, if we can,
            // and read the next value.
            if (!data.host.isEmpty() && !hasHost(data.host)) {
                // We already registered this entity.

                if (data.name.isEmpty()) {
                    data.name = data.host;
                }
                data.useSshConfig = true;
                data.importedFromSshConfig = true;
                data.profileName = Konsole::ProfileManager::instance()->defaultProfile()->name();
                addChildItem(data, i18n("SSH Config"));
            }

            data = {};
            data.host = lists.at(1);
        }

        if (ignoreEntry) {
            continue;
        }

        if (lists.at(0) == QStringLiteral("HostName")) {
            // hostname is always after Host, so this will be true.
            const QString currentHost = data.host;
            data.host = lists.at(1).trimmed();
            data.name = currentHost.trimmed();
        } else if (lists.at(0) == QStringLiteral("IdentityFile")) {
            data.sshKey = lists.at(1).trimmed();
        } else if (lists.at(0) == QStringLiteral("Port")) {
            data.port = lists.at(1).trimmed();
        } else if (lists.at(0) == QStringLiteral("User")) {
            data.username = lists.at(1).trimmed();
        }
    }

    // the last possible read
    if (data.host.count()) {
        if (!hasHost(data.host)) {
            if (data.name.isEmpty()) {
                data.name = data.host.trimmed();
            }
            data.useSshConfig = true;
            data.importedFromSshConfig = true;
            addChildItem(data, i18n("SSH Config"));
        }
    }
}

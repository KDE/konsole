/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sshmanagermodel.h"

#include <QStandardItem>

#include <KLocalizedString>

#include <KConfig>
#include <KConfigGroup>

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
#include "sshcryptohelper.h"

#include "sshmanagerplugindebug.h"

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
        addTopLevelItem(i18nc("@item:inlistbox Hosts from ssh/config file", "SSH Config"));
    }
    if (invisibleRootItem()->rowCount() == 0) {
        addTopLevelItem(i18nc("@item:inlistbox The default list of ssh hosts", "Default"));
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
    if (!manageProfile) {
        return;
    }

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
        else if (m_sessionToProfileName[m_session].length()) {
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
        if (pr) {
            return pr->name() == profileToLoad;
        }
        return false;
    });

    if (findIt == std::end(profiles)) {
        return;
    }

    sm->setSessionProfile(m_session, *findIt);
}

void SSHManagerModel::load()
{
    m_sshConfigTopLevelItem = nullptr;

    auto config = KConfig(QStringLiteral("konsolesshconfig"), KConfig::OpenFlag::SimpleConfig);

    // Load encryption settings first
    if (config.hasGroup(QStringLiteral("Encryption"))) {
        KConfigGroup encGroup = config.group(QStringLiteral("Encryption"));
        m_encryptionEnabled = encGroup.readEntry<bool>("enabled", false);
        m_encryptionSalt = encGroup.readEntry("salt");
        m_encryptionVerifier = encGroup.readEntry("verifier");
    }

    const auto groupList = config.groupList();
    for (const QString &groupName : groupList) {
        if (groupName == QStringLiteral("Encryption")) {
            continue;
        }
        KConfigGroup group = config.group(groupName);
        if (groupName == QStringLiteral("Global plugin config")) {
            manageProfile = group.readEntry<bool>("manageProfile", false);
            continue;
        }
        addTopLevelItem(groupName);
        const auto groupGroupList = group.groupList();
        for (const QString &sessionName : groupGroupList) {
            SSHConfigurationData data;
            KConfigGroup sessionGroup = group.group(sessionName);
            data.host = sessionGroup.readEntry("hostname");
            data.name = sessionGroup.readEntry("identifier");
            data.port = sessionGroup.readEntry("port");
            data.profileName = sessionGroup.readEntry("profileName");
            data.username = sessionGroup.readEntry("username");
            data.password = maybeDecrypt(sessionGroup.readEntry("password"));
            data.sshKey = sessionGroup.readEntry("sshkey");
            data.sshKeyPassphrase = maybeDecrypt(sessionGroup.readEntry("sshKeyPassphrase"));
            data.autoAcceptKeys = sessionGroup.readEntry<bool>("autoAcceptKeys", false);
            data.useSshConfig = sessionGroup.readEntry<bool>("useSshConfig", false);
            data.importedFromSshConfig = sessionGroup.readEntry<bool>("importedFromSshConfig", false);
            
            data.useProxy = sessionGroup.readEntry<bool>("useProxy", false);
            data.proxyIp = sessionGroup.readEntry("proxyIp");
            data.proxyPort = sessionGroup.readEntry("proxyPort");
            data.proxyUsername = sessionGroup.readEntry("proxyUsername");
            data.proxyPassword = maybeDecrypt(sessionGroup.readEntry("proxyPassword"));

            data.enableSshfs = sessionGroup.readEntry<bool>("enableSshfs", false);

            addChildItem(data, groupName);
        }
    }
}

void SSHManagerModel::save()
{
    auto config = KConfig(QStringLiteral("konsolesshconfig"), KConfig::OpenFlag::SimpleConfig);
    const auto groupList = config.groupList();
    for (const QString &groupName : groupList) {
        config.deleteGroup(groupName);
    }

    KConfigGroup globalGroup = config.group(QStringLiteral("Global plugin config"));
    globalGroup.writeEntry("manageProfile", manageProfile);

    // Save encryption settings
    KConfigGroup encGroup = config.group(QStringLiteral("Encryption"));
    encGroup.writeEntry("enabled", m_encryptionEnabled);
    encGroup.writeEntry("salt", m_encryptionSalt);
    encGroup.writeEntry("verifier", m_encryptionVerifier);

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
            sshGroup.writeEntry("sshKeyPassphrase", maybeEncrypt(data.sshKeyPassphrase));
            sshGroup.writeEntry("autoAcceptKeys", data.autoAcceptKeys);
            sshGroup.writeEntry("useSshConfig", data.useSshConfig);
            sshGroup.writeEntry("username", data.username);
            sshGroup.writeEntry("password", maybeEncrypt(data.password));
            
            sshGroup.writeEntry("useProxy", data.useProxy);
            sshGroup.writeEntry("proxyIp", data.proxyIp);
            sshGroup.writeEntry("proxyPort", data.proxyPort);
            sshGroup.writeEntry("proxyUsername", data.proxyUsername);
            sshGroup.writeEntry("proxyPassword", maybeEncrypt(data.proxyPassword));
            
            sshGroup.writeEntry("enableSshfs", data.enableSshfs);

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
        qCDebug(SshManagerPluginDebug) << "Can't open config file";
        return;
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
    if (data.host.length()) {
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

void SSHManagerModel::setManageProfile(bool manage)
{
    manageProfile = manage;
}

bool SSHManagerModel::getManageProfile()
{
    return manageProfile;
}

// --- Encryption ---

QString SSHManagerModel::maybeEncrypt(const QString &value) const
{
    if (!m_encryptionEnabled || m_masterPassword.isEmpty() || value.isEmpty()) {
        return value;
    }
    return SSHCryptoHelper::encrypt(value, m_masterPassword);
}

QString SSHManagerModel::maybeDecrypt(const QString &value) const
{
    if (!SSHCryptoHelper::isEncrypted(value)) {
        return value;
    }
    if (m_masterPassword.isEmpty()) {
        // No password yet — return the raw encrypted value;
        // it will be decrypted after the user provides the password.
        return value;
    }
    const QString decrypted = SSHCryptoHelper::decrypt(value, m_masterPassword);
    return decrypted.isEmpty() ? value : decrypted;
}

void SSHManagerModel::setMasterPassword(const QString &password)
{
    m_masterPassword = password;
}

bool SSHManagerModel::hasMasterPassword() const
{
    return !m_masterPassword.isEmpty();
}

bool SSHManagerModel::verifyMasterPassword(const QString &password) const
{
    if (m_encryptionVerifier.isEmpty()) {
        return false;
    }
    const QString decrypted = SSHCryptoHelper::decrypt(m_encryptionVerifier, password);
    return decrypted == QStringLiteral("KONSOLE_SSH_VERIFY");
}

void SSHManagerModel::enableEncryption(const QString &password)
{
    m_masterPassword = password;
    m_encryptionEnabled = true;
    m_encryptionVerifier = SSHCryptoHelper::encrypt(QStringLiteral("KONSOLE_SSH_VERIFY"), password);
    save();
    // Reload so in-memory data is consistent
    clear();
    load();
}

void SSHManagerModel::disableEncryption()
{
    m_encryptionEnabled = false;
    m_encryptionVerifier.clear();
    m_encryptionSalt.clear();
    // Re-save with plaintext
    save();
}

bool SSHManagerModel::isEncryptionEnabled() const
{
    return m_encryptionEnabled;
}

void SSHManagerModel::decryptAll()
{
    if (m_masterPassword.isEmpty()) {
        return;
    }

    for (int i = 0, end = invisibleRootItem()->rowCount(); i < end; i++) {
        QStandardItem *groupItem = invisibleRootItem()->child(i);
        for (int e = 0, rend = groupItem->rowCount(); e < rend; e++) {
            QStandardItem *sshElement = groupItem->child(e);
            auto data = sshElement->data(SSHRole).value<SSHConfigurationData>();
            bool changed = false;

            if (SSHCryptoHelper::isEncrypted(data.password)) {
                data.password = maybeDecrypt(data.password);
                changed = true;
            }
            if (SSHCryptoHelper::isEncrypted(data.sshKeyPassphrase)) {
                data.sshKeyPassphrase = maybeDecrypt(data.sshKeyPassphrase);
                changed = true;
            }
            if (SSHCryptoHelper::isEncrypted(data.proxyPassword)) {
                data.proxyPassword = maybeDecrypt(data.proxyPassword);
                changed = true;
            }

            if (changed) {
                sshElement->setData(QVariant::fromValue(data), SSHRole);
            }
        }
    }
}

// --- Import/Export ---

static QJsonObject dataToJson(const SSHConfigurationData &data)
{
    QJsonObject obj;
    obj[QStringLiteral("name")] = data.name;
    obj[QStringLiteral("host")] = data.host;
    obj[QStringLiteral("port")] = data.port;
    obj[QStringLiteral("sshKey")] = data.sshKey;
    obj[QStringLiteral("sshKeyPassphrase")] = data.sshKeyPassphrase;
    obj[QStringLiteral("username")] = data.username;
    obj[QStringLiteral("profileName")] = data.profileName;
    obj[QStringLiteral("password")] = data.password;
    obj[QStringLiteral("autoAcceptKeys")] = data.autoAcceptKeys;
    obj[QStringLiteral("useSshConfig")] = data.useSshConfig;
    obj[QStringLiteral("importedFromSshConfig")] = data.importedFromSshConfig;
    obj[QStringLiteral("useProxy")] = data.useProxy;
    obj[QStringLiteral("proxyIp")] = data.proxyIp;
    obj[QStringLiteral("proxyPort")] = data.proxyPort;
    obj[QStringLiteral("proxyUsername")] = data.proxyUsername;
    obj[QStringLiteral("proxyPassword")] = data.proxyPassword;
    obj[QStringLiteral("enableSshfs")] = data.enableSshfs;
    return obj;
}

static SSHConfigurationData jsonToData(const QJsonObject &obj)
{
    SSHConfigurationData data;
    data.name = obj[QStringLiteral("name")].toString();
    data.host = obj[QStringLiteral("host")].toString();
    data.port = obj[QStringLiteral("port")].toString();
    data.sshKey = obj[QStringLiteral("sshKey")].toString();
    data.sshKeyPassphrase = obj[QStringLiteral("sshKeyPassphrase")].toString();
    data.username = obj[QStringLiteral("username")].toString();
    data.profileName = obj[QStringLiteral("profileName")].toString();
    data.password = obj[QStringLiteral("password")].toString();
    data.autoAcceptKeys = obj[QStringLiteral("autoAcceptKeys")].toBool();
    data.useSshConfig = obj[QStringLiteral("useSshConfig")].toBool();
    data.importedFromSshConfig = obj[QStringLiteral("importedFromSshConfig")].toBool();
    data.useProxy = obj[QStringLiteral("useProxy")].toBool();
    data.proxyIp = obj[QStringLiteral("proxyIp")].toString();
    data.proxyPort = obj[QStringLiteral("proxyPort")].toString();
    data.proxyUsername = obj[QStringLiteral("proxyUsername")].toString();
    data.proxyPassword = obj[QStringLiteral("proxyPassword")].toString();
    data.enableSshfs = obj[QStringLiteral("enableSshfs")].toBool();
    return data;
}

QJsonDocument SSHManagerModel::exportToJson(const QString &exportPassword) const
{
    QJsonArray foldersArray;

    for (int i = 0, end = invisibleRootItem()->rowCount(); i < end; i++) {
        QStandardItem *groupItem = invisibleRootItem()->child(i);
        QJsonObject folderObj;
        folderObj[QStringLiteral("name")] = groupItem->text();

        QJsonArray entriesArray;
        for (int e = 0, rend = groupItem->rowCount(); e < rend; e++) {
            QStandardItem *sshElement = groupItem->child(e);
            const auto data = sshElement->data(SSHRole).value<SSHConfigurationData>();
            entriesArray.append(dataToJson(data));
        }
        folderObj[QStringLiteral("entries")] = entriesArray;
        foldersArray.append(folderObj);
    }

    if (!exportPassword.isEmpty()) {
        // Encrypted export: encrypt the folders payload
        QJsonDocument foldersDoc(foldersArray);
        const QByteArray foldersBytes = foldersDoc.toJson(QJsonDocument::Compact);
        const QByteArray encrypted = SSHCryptoHelper::encryptBlob(foldersBytes, exportPassword);
        if (encrypted.isEmpty()) {
            return {};
        }

        const int saltEnd = SSHCryptoHelper::SALT_SIZE;
        const int ivEnd = saltEnd + SSHCryptoHelper::IV_SIZE;
        const int tagEnd = ivEnd + SSHCryptoHelper::TAG_SIZE;

        QJsonObject root;
        root[QStringLiteral("version")] = 1;
        root[QStringLiteral("encrypted")] = true;
        root[QStringLiteral("salt")] = QString::fromLatin1(encrypted.mid(0, saltEnd).toBase64());
        root[QStringLiteral("iv")] = QString::fromLatin1(encrypted.mid(saltEnd, SSHCryptoHelper::IV_SIZE).toBase64());
        root[QStringLiteral("tag")] = QString::fromLatin1(encrypted.mid(ivEnd, SSHCryptoHelper::TAG_SIZE).toBase64());
        root[QStringLiteral("data")] = QString::fromLatin1(encrypted.mid(tagEnd).toBase64());
        return QJsonDocument(root);
    }

    // Unencrypted export
    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("encrypted")] = false;
    root[QStringLiteral("folders")] = foldersArray;
    return QJsonDocument(root);
}

bool SSHManagerModel::importFromJson(const QJsonDocument &doc, const QString &importPassword)
{
    if (doc.isNull() || !doc.isObject()) {
        return false;
    }

    const QJsonObject root = doc.object();
    const bool encrypted = root[QStringLiteral("encrypted")].toBool();

    QJsonArray foldersArray;

    if (encrypted) {
        if (importPassword.isEmpty()) {
            return false;
        }

        // Reconstruct the blob: salt || iv || tag || ciphertext
        const QByteArray salt = QByteArray::fromBase64(root[QStringLiteral("salt")].toString().toLatin1());
        const QByteArray iv = QByteArray::fromBase64(root[QStringLiteral("iv")].toString().toLatin1());
        const QByteArray tag = QByteArray::fromBase64(root[QStringLiteral("tag")].toString().toLatin1());
        const QByteArray cipherData = QByteArray::fromBase64(root[QStringLiteral("data")].toString().toLatin1());

        QByteArray blob;
        blob.append(salt);
        blob.append(iv);
        blob.append(tag);
        blob.append(cipherData);

        const QByteArray decrypted = SSHCryptoHelper::decryptBlob(blob, importPassword);
        if (decrypted.isEmpty()) {
            return false;
        }

        QJsonParseError parseError;
        QJsonDocument foldersDoc = QJsonDocument::fromJson(decrypted, &parseError);
        if (parseError.error != QJsonParseError::NoError || !foldersDoc.isArray()) {
            return false;
        }
        foldersArray = foldersDoc.array();
    } else {
        foldersArray = root[QStringLiteral("folders")].toArray();
    }

    for (const QJsonValue &folderVal : foldersArray) {
        const QJsonObject folderObj = folderVal.toObject();
        const QString folderName = folderObj[QStringLiteral("name")].toString();
        if (folderName.isEmpty()) {
            continue;
        }

        const QJsonArray entries = folderObj[QStringLiteral("entries")].toArray();
        for (const QJsonValue &entryVal : entries) {
            SSHConfigurationData data = jsonToData(entryVal.toObject());
            if (data.name.isEmpty() || data.host.isEmpty()) {
                continue;
            }
            addChildItem(data, folderName);
        }
    }

    save();
    return true;
}

#include "moc_sshmanagermodel.cpp"

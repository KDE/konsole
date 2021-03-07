#include "sshmanagermodel.h"

#include <QStandardItem>

#include <KLocalizedString>

#include <KConfigGroup>
#include <KConfig>

#include <QDebug>

#include "sshconfigurationdata.h"

constexpr const int SSHRole = Qt::UserRole + 1;

SSHManagerModel::SSHManagerModel(QObject *parent)
: QStandardItemModel(parent)
{
    load();
    if (invisibleRootItem()->rowCount() == 0) {
        addTopLevelItem(i18n("Default"));
    }
}

SSHManagerModel::~SSHManagerModel() noexcept
{
    qDebug() << "Triggered?";
    save();
}

void SSHManagerModel::addTopLevelItem(const QString& name)
{
    auto *newItem = new QStandardItem();
    newItem->setText(name);
    invisibleRootItem()->appendRow(newItem);
}

void SSHManagerModel::addChildItem(const SSHConfigurationData &config, const QString& parentName)
{
    QStandardItem *item = nullptr;
    for (int i = 0, end = invisibleRootItem()->rowCount(); i < end; i++) {
        if (invisibleRootItem()->child(i)->text() == parentName) {
            item = invisibleRootItem()->child(i);
            break;
        }
    }

    if (!item) {
        return;
    }

    auto newChild = new QStandardItem();
    newChild->setData(QVariant::fromValue(config), SSHRole);
    newChild->setData(config.name, Qt::DisplayRole);
    item->appendRow(newChild);
}

void SSHManagerModel::load()
{
    auto config = KConfig(QStringLiteral("konsolesshconfig"), KConfig::OpenFlag::SimpleConfig);
    for (const QString &groupName : config.groupList()) {
        KConfigGroup group = config.group(groupName);
        addTopLevelItem(groupName);
        for(const QString& sessionName : group.groupList()) {
            SSHConfigurationData data;
            KConfigGroup sessionGroup = group.group(sessionName);
            data.host = sessionGroup.readEntry("hostname");
            data.name = sessionGroup.readEntry("identifier");
            data.port = sessionGroup.readEntry("port");
            data.profileName = sessionGroup.readEntry("profilename");
            data.sshKey = sessionGroup.readEntry("sshkey");
            addChildItem(data, groupName);
        }
    }
    qDebug() << "Loading" << config.groupList();
}

void SSHManagerModel::save()
{
    auto config = KConfig(QStringLiteral("konsolesshconfig"), KConfig::OpenFlag::SimpleConfig);
    for (const QString &groupName : config.groupList()) {
        qDebug() << "Deleting groups";
        config.deleteGroup(groupName);
    }

    for (int i = 0, end = invisibleRootItem()->rowCount(); i < end; i++) {
        QStandardItem *groupItem = invisibleRootItem()->child(i);
        const QString groupName = groupItem->text();
        qDebug() << "Creating group" << groupName;
        KConfigGroup baseGroup = config.group(groupName);
        for (int e = 0, end = groupItem->rowCount(); e < end; e++) {
            QStandardItem *sshElement = groupItem->child(e);
            const auto data = sshElement->data(SSHRole).value<SSHConfigurationData>();
            KConfigGroup sshGroup = baseGroup.group(data.name);
            sshGroup.writeEntry("hostname", data.host);
            sshGroup.writeEntry("identifier", data.name);
            sshGroup.writeEntry("port", data.port);
            sshGroup.writeEntry("profileName", data.profileName);
            sshGroup.writeEntry("sshkey", data.sshKey);
            sshGroup.sync();
        }
        baseGroup.sync();
    }
    config.sync();
}

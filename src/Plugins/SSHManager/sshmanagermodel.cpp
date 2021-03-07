#include "sshmanagermodel.h"

#include <QStandardItem>

#include <KLocalizedString>

#include "sshconfigurationdata.h"

constexpr const int SSHRole = Qt::UserRole + 1;

SSHManagerModel::SSHManagerModel(QObject *parent)
: QStandardItemModel(parent)
{
    addTopLevelItem(i18n("Default"));
}

void SSHManagerModel::addTopLevelItem(const QString& name)
{
    auto *newItem = new QStandardItem();
    newItem->setText(name);
    invisibleRootItem()->appendRow(newItem);
}

void SSHManagerModel::addChildItem(const SSHConfigurationData &config, const QModelIndex& parent)
{
    QStandardItem *currentItem = parent.isValid() ? itemFromIndex(parent) : invisibleRootItem()->child(0);
    auto newChild = new QStandardItem();
    newChild->setData(QVariant::fromValue(config), SSHRole);
    newChild->setData(config.name, Qt::DisplayRole);
    currentItem->appendRow(newChild);
}

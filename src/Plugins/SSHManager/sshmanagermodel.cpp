#include "sshmanagermodel.h"

#include <QStandardItem>
#include "sshconfigurationdata.h"

constexpr const int SSHRole = Qt::UserRole + 1;

SSHManagerModel::SSHManagerModel(QObject *parent)
: QStandardItemModel(parent)
{

}

void SSHManagerModel::addTopLevelItem(const QString& name)
{
    auto *newItem = new QStandardItem();
    newItem->setText(name);
    invisibleRootItem()->appendRow(newItem);
}

void SSHManagerModel::addChildItem(const QModelIndex& parent, const SSHConfigurationData &config)
{
    QStandardItem *currentItem = itemFromIndex(parent);
    auto newChild = new QStandardItem();
    newChild->setData(QVariant::fromValue(config), SSHRole);
    currentItem->appendRow(newChild);
}


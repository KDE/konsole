#ifndef SSHMANAGERMODEL_H
#define SSHMANAGERMODEL_H

#include <QStandardItemModel>

class SSHConfigurationData;

class SSHManagerModel : public QStandardItemModel {
    Q_OBJECT
public:
    SSHManagerModel(QObject *parent = nullptr);
    void setModel(SSHManagerModel *model);
    void addTopLevelItem(const QString& toplevel);
    void addChildItem(const SSHConfigurationData &config, const QModelIndex& parent = QModelIndex());
};

#endif

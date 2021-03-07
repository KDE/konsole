#ifndef SSHMANAGERMODEL_H
#define SSHMANAGERMODEL_H

#include <QStandardItemModel>

class SSHConfigurationData;

class SSHManagerModel : public QStandardItemModel {
    Q_OBJECT
public:
    SSHManagerModel(QObject *parent = nullptr);
    void addTopLevelItem(const QString& toplevel);
    void addChildItem(const QModelIndex& parent, const SSHConfigurationData &config);
};

#endif

#ifndef SSHMANAGERMODEL_H
#define SSHMANAGERMODEL_H

#include <QStandardItemModel>

class SSHConfigurationData;

class SSHManagerModel : public QStandardItemModel {
    Q_OBJECT
public:
    SSHManagerModel(QObject *parent = nullptr);
    ~SSHManagerModel();

    void setModel(SSHManagerModel *model);
    void addTopLevelItem(const QString& toplevel);
    void addChildItem(const SSHConfigurationData &config, const QString &parentName);

    void load();
    void save();
};

#endif

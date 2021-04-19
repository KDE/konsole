#ifndef SSHMANAGERMODEL_H
#define SSHMANAGERMODEL_H

#include <QStandardItemModel>

class SSHConfigurationData;

class SSHManagerModel : public QStandardItemModel {
    Q_OBJECT
public:
    enum Roles {
        SSHRole = Qt::UserRole + 1
    };

    SSHManagerModel(QObject *parent = nullptr);
    ~SSHManagerModel();

    void setModel(SSHManagerModel *model);
    QStandardItem *addTopLevelItem(const QString& toplevel);
    void addChildItem(const SSHConfigurationData &config, const QString &parentName);
    void removeIndex(const QModelIndex& idx);

    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void load();
    void save();
};

#endif

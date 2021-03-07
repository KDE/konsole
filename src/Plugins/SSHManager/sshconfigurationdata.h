#ifndef SSHCONFIGURATIONDATA_H
#define SSHCONFIGURATIONDATA_H

struct SSHConfigurationData {
    QString name;
    QString host;
    QString port;
    QString sshKey;
    QString profileName;
};

Q_DECLARE_METATYPE(SSHConfigurationData)

#endif

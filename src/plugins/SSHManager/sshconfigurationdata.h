/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SSHCONFIGURATIONDATA_H
#define SSHCONFIGURATIONDATA_H

#include <QString>

class SSHConfigurationData
{
public:
    QString name;
    QString host;
    QString port;
    QString sshKey;
    QString sshKeyPassphrase;
    QString username;
    QString profileName;
    QString password;
    bool autoAcceptKeys = false;
    bool useSshConfig = false;
    bool importedFromSshConfig = false;
    
    // Proxy Configuration
    bool useProxy = false;
    QString proxyIp;
    QString proxyPort;
    QString proxyUsername;
    QString proxyPassword;

    // SSHFS Configuration
    bool enableSshfs = false;
};

Q_DECLARE_METATYPE(SSHConfigurationData)

#endif

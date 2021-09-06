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
    QString username;
    QString profileName;
    bool useSshConfig = false;
    bool importedFromSshConfig = false;
};

Q_DECLARE_METATYPE(SSHConfigurationData)

#endif

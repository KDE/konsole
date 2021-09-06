/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SSHPROCESSINFO_H
#define SSHPROCESSINFO_H

#include "ProcessInfo.h"

namespace Konsole
{
/**
 * Lightweight class which provides additional information about SSH processes.
 */
class SSHProcessInfo
{
public:
    /**
     * Constructs a new SSHProcessInfo instance which provides additional
     * information about the specified SSH process.
     *
     * @param process A ProcessInfo instance for a SSH process.
     */
    explicit SSHProcessInfo(const ProcessInfo &process);

    /**
     * Returns the user name which the user initially logged into on
     * the remote computer.
     */
    QString userName() const;

    /**
     * Returns the host which the user has connected to.
     */
    QString host() const;

    /**
     * Returns the port on host which the user has connected to.
     */
    QString port() const;

    /**
     * Returns the command which the user specified to execute on the
     * remote computer when starting the SSH process.
     */
    QString command() const;

    /**
     * Operates in the same way as ProcessInfo::format(), except
     * that the set of markers understood is different:
     *
     * %u - Replaced with user name which the user initially logged
     *      into on the remote computer.
     * %h - Replaced with the first part of the host name which
     *      is connected to.
     * %H - Replaced with the full host name of the computer which
     *      is connected to.
     * %c - Replaced with the command which the user specified
     *      to execute when starting the SSH process.
     */
    QString format(const QString &input) const;

private:
    const ProcessInfo &_process;
    QString _user;
    QString _host;
    QString _port;
    QString _command;
};

}

#endif

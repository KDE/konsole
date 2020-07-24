/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
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

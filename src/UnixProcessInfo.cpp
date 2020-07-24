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

// Own
#if !defined(Q_OS_WIN)
    #include "UnixProcessInfo.h"
#endif

// Unix
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/param.h>
#include <cerrno>

// Qt
#include <QDebug>

using namespace Konsole;

#if !defined(Q_OS_WIN)

UnixProcessInfo::UnixProcessInfo(int pid)
    : ProcessInfo(pid)
{
    setUserNameRequired(true);
}

void UnixProcessInfo::readProcessInfo(int pid)
{
    // prevent _arguments from growing longer and longer each time this
    // method is called.
    clearArguments();

    if (readProcInfo(pid)) {
        readArguments(pid);
        readCurrentDir(pid);

        bool ok = false;
        const QString &processNameString = name(&ok);

        if (ok && processNameString == QLatin1String("sudo")) {
            //Append process name along with sudo
            const QVector<QString> &args = arguments(&ok);

            if (ok && args.size() > 1) {
                setName(processNameString + QStringLiteral(" ") + args[1]);
            }
        }
    }
}

void UnixProcessInfo::readUserName()
{
    bool ok = false;
    const int uid = userId(&ok);
    if (!ok) {
        return;
    }

    struct passwd passwdStruct;
    struct passwd *getpwResult;
    char *getpwBuffer;
    long getpwBufferSize;
    int getpwStatus;

    getpwBufferSize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (getpwBufferSize == -1) {
        getpwBufferSize = 16384;
    }

    getpwBuffer = new char[getpwBufferSize];
    if (getpwBuffer == nullptr) {
        return;
    }
    getpwStatus = getpwuid_r(uid, &passwdStruct, getpwBuffer, getpwBufferSize, &getpwResult);
    if ((getpwStatus == 0) && (getpwResult != nullptr)) {
        setUserName(QLatin1String(passwdStruct.pw_name));
    } else {
        setUserName(QString());
        qWarning() << "getpwuid_r returned error : " << getpwStatus;
    }
    delete [] getpwBuffer;
}

#endif

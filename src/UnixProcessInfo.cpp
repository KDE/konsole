/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#if !defined(Q_OS_WIN)
#include "UnixProcessInfo.h"
#endif

// Unix
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <unistd.h>

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
            // Append process name along with sudo
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
    delete[] getpwBuffer;
}

#endif

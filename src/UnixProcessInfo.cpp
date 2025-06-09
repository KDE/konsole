/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "UnixProcessInfo.h"

#ifndef Q_OS_WIN
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
#include <QtGlobal>

#if defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD) || defined(Q_OS_MACOS)
#include <QSharedPointer>
#include <sys/sysctl.h>
#endif

using namespace Konsole;

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

bool UnixProcessInfo::readArguments(int pid)
{
    // used for LinuxProcessInfo and SolarisProcessInfo
    // read command-line arguments file found at /proc/<pid>/cmdline
    // the expected format is a list of strings delimited by null characters,
    // and ending in a double null character pair.

    QFile argumentsFile(QStringLiteral("/proc/%1/cmdline").arg(pid));
    if (argumentsFile.open(QIODevice::ReadOnly)) {
        QTextStream stream(&argumentsFile);
        const QString &data = stream.readAll();

        const QStringList &argList = data.split(QLatin1Char('\0'));

        for (const QString &entry : argList) {
            if (!entry.isEmpty()) {
                addArgument(entry);
            }
        }
    } else {
        setFileError(argumentsFile.error());
    }

    return true;
}

#if defined(Q_OS_FREEBSD) || defined(Q_OS_OPEN_BSD) || defined(Q_OS_MACOS)
QSharedPointer<struct kinfo_proc> UnixProcessInfo::getProcInfoStruct(int *managementInfoBase, int mibCount)
{
    size_t structLength;

    if (::sysctl(managementInfoBase, mibCount, NULL, &structLength, NULL, 0) == -1) {
        qWarning() << "first sysctl() call failed with code " << errno;
        return nullptr;
    }

    QSharedPointer<struct kinfo_proc> kInfoProc(new struct kinfo_proc[structLength]);

    if (::sysctl(managementInfoBase, mibCount, kInfoProc.get(), &structLength, NULL, 0) == -1) {
        qWarning() << "second sysctl() call failed with code " << errno;
        return nullptr;
    }

    return kInfoProc;
}
#endif

#endif // Q_OS_WIN

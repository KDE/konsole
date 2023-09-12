/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef UNIXPROCESSINFO_H
#define UNIXPROCESSINFO_H

#include "ProcessInfo.h"

#if defined(Q_OS_MACOS) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
#include <QSharedPointer>
#include <sys/sysctl.h>
#if defined(Q_OS_FREEBSD) || defined(Q_OS_OPEN_BSD)
#include <sys/user.h>
#endif
#endif

namespace Konsole
{
#ifndef Q_OS_WIN
/**
 * Implementation of ProcessInfo for Unix platforms which uses
 * the /proc filesystem
 */
class UnixProcessInfo : public ProcessInfo
{
public:
    /**
     * Constructs a new instance of UnixProcessInfo.
     * See ProcessInfo::newInstance()
     */
    explicit UnixProcessInfo(int pid);

protected:
    /**
     * Implementation of ProcessInfo::readProcessInfo(); calls the
     * four private methods below in turn.
     */
    void readProcessInfo(int pid) override;

    void readUserName(void) override;

    bool readArguments(int pid) override;

#if defined(Q_OS_MACOS) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
    /**
     * Allocates an array of struct kinfo_proc and calls sysctl internally to fill it up
     * @param managementInfoBase management information base to pass to sysctl
     * @param mibCount number of elements in managementInfoBase, also passed as argument to sysctl
     * @return smart pointer to struct kinfo_proc[] and returns nullptr on failure
     */
    QSharedPointer<struct kinfo_proc> getProcInfoStruct(int *managementInfoBase, int mibCount);
#endif

private:
    /**
     * Read the standard process information -- PID, parent PID, foreground PID.
     * @param pid process ID to use
     * @return true on success
     */
    virtual bool readProcInfo(int pid) = 0;
};
#endif
}

#endif

/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef UNIXPROCESSINFO_H
#define UNIXPROCESSINFO_H

#include "ProcessInfo.h"

namespace Konsole
{
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

private:
    /**
     * Read the standard process information -- PID, parent PID, foreground PID.
     * @param pid process ID to use
     * @return true on success
     */
    virtual bool readProcInfo(int pid) = 0;

    /**
     * Determine what arguments were passed to the process. Sets _arguments.
     * @param pid process ID to use
     * @return true on success
     */
    virtual bool readArguments(int pid) = 0;
};

}

#endif

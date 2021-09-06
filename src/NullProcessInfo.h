/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.countm>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef NULLPROCESSINFO_H
#define NULLPROCESSINFO_H

#include "ProcessInfo.h"

namespace Konsole
{
/**
 * Implementation of ProcessInfo which does nothing.
 * Used on platforms where a suitable ProcessInfo subclass is not
 * available.
 *
 * isValid() will always return false for instances of NullProcessInfo
 */
class NullProcessInfo : public ProcessInfo
{
public:
    /**
     * Constructs a new NullProcessInfo instance.
     * See ProcessInfo::newInstance()
     */
    explicit NullProcessInfo(int pid);

protected:
    void readProcessInfo(int pid) override;
    bool readCurrentDir(int pid) override;
    void readUserName(void) override;
};

}

#endif

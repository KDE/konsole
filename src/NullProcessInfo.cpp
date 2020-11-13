/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.countm>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "NullProcessInfo.h"

using namespace Konsole;

NullProcessInfo::NullProcessInfo(int pid)
    : ProcessInfo(pid)
{
}

void NullProcessInfo::readProcessInfo(int /*pid*/)
{
}

bool NullProcessInfo::readCurrentDir(int /*pid*/)
{
    return false;
}

void NullProcessInfo::readUserName()
{
}

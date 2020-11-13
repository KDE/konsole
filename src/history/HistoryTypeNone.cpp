/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "HistoryTypeNone.h"
#include "HistoryScrollNone.h"

using namespace Konsole;

HistoryTypeNone::HistoryTypeNone() = default;

bool HistoryTypeNone::isEnabled() const
{
    return false;
}

HistoryScroll *HistoryTypeNone::scroll(HistoryScroll *old) const
{
    delete old;
    return new HistoryScrollNone();
}

int HistoryTypeNone::maximumLineCount() const
{
    return 0;
}

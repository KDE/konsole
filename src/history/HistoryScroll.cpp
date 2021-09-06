/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "HistoryScroll.h"

#include "HistoryType.h"

using namespace Konsole;

HistoryScroll::HistoryScroll(HistoryType *t)
    : _historyType(t)
{
}

HistoryScroll::~HistoryScroll() = default;

bool HistoryScroll::hasScroll() const
{
    return true;
}

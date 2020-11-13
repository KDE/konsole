/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "HistoryType.h"

using namespace Konsole;

HistoryType::HistoryType() = default;

HistoryType::~HistoryType() = default;

bool HistoryType::isUnlimited() const
{
    return maximumLineCount() == -1;
}

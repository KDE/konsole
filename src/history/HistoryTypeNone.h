/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef HISTORYTYPENONE_H
#define HISTORYTYPENONE_H

#include "HistoryType.h"
#include "konsoleprivate_export.h"

namespace Konsole
{
class KONSOLEPRIVATE_EXPORT HistoryTypeNone : public HistoryType
{
public:
    HistoryTypeNone();

    bool isEnabled() const override;
    int maximumLineCount() const override;

    void scroll(std::unique_ptr<HistoryScroll> &) const override;
};

}

#endif

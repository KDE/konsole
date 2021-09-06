/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COMPACTHISTORYTYPE_H
#define COMPACTHISTORYTYPE_H

#include "history/HistoryType.h"
#include "konsoleprivate_export.h"

namespace Konsole
{
class KONSOLEPRIVATE_EXPORT CompactHistoryType : public HistoryType
{
public:
    explicit CompactHistoryType(unsigned int nbLines);

    bool isEnabled() const override;
    int maximumLineCount() const override;

    void scroll(std::unique_ptr<HistoryScroll> &) const override;

protected:
    unsigned int _maxLines;
};

}

#endif

/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef HISTORYTYPE_H
#define HISTORYTYPE_H

#include "HistoryScroll.h"
#include "konsoleprivate_export.h"

namespace Konsole
{
class KONSOLEPRIVATE_EXPORT HistoryType
{
public:
    HistoryType();
    virtual ~HistoryType();

    /**
     * Returns true if the history is enabled ( can store lines of output )
     * or false otherwise.
     */
    virtual bool isEnabled() const = 0;
    /**
     * Returns the maximum number of lines which this history type
     * can store or -1 if the history can store an unlimited number of lines.
     */
    virtual int maximumLineCount() const = 0;
    /**
     * Converts from one type of HistoryScroll to another or if given the
     * same type, returns it.
     */
    virtual void scroll(std::unique_ptr<HistoryScroll> &) const = 0;
    /**
     * Returns true if the history size is unlimited.
     */
    bool isUnlimited() const;
};

}

#endif

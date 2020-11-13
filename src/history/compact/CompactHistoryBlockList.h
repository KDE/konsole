/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COMPACTHISTORYBLOCKLIST_H
#define COMPACTHISTORYBLOCKLIST_H

#include <QList>
#include "CompactHistoryBlock.h"

namespace Konsole
{

class CompactHistoryBlockList
{
public:
    CompactHistoryBlockList();
    ~CompactHistoryBlockList();

    void *allocate(size_t size);
    void deallocate(void *);
    int length();

private:
    QList<CompactHistoryBlock *> list;
};

}

#endif

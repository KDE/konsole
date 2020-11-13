/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "CompactHistoryBlockList.h"

// Qt

using namespace Konsole;

CompactHistoryBlockList::CompactHistoryBlockList()
    : list(QList<CompactHistoryBlock *>())
{
}

void *CompactHistoryBlockList::allocate(size_t size)
{
    CompactHistoryBlock *block;
    if (list.isEmpty() || list.last()->remaining() < size) {
        block = new CompactHistoryBlock();
        list.append(block);
        ////qDebug() << "new block created, remaining " << block->remaining() << "number of blocks=" << list.size();
    } else {
        block = list.last();
        ////qDebug() << "old block used, remaining " << block->remaining();
    }
    return block->allocate(size);
}

void CompactHistoryBlockList::deallocate(void *ptr)
{
    Q_ASSERT(!list.isEmpty());

    int i = 0;
    CompactHistoryBlock *block = list.at(i);
    while (i < list.size() && !block->contains(ptr)) {
        i++;
        block = list.at(i);
    }

    Q_ASSERT(i < list.size());

    block->deallocate();

    if (!block->isInUse()) {
        list.removeAt(i);
        delete block;
        ////qDebug() << "block deleted, new size = " << list.size();
    }
}

int CompactHistoryBlockList::length()
{
    return list.size();
}

CompactHistoryBlockList::~CompactHistoryBlockList()
{
    qDeleteAll(list.begin(), list.end());
    list.clear();
}

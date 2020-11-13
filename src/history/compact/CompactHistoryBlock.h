/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COMPACTHISTORYBLOCK_H
#define COMPACTHISTORYBLOCK_H

// Konsole
#include "../characters/Character.h"

// System
#include <sys/mman.h>

namespace Konsole
{

class CompactHistoryBlock
{
public:
    CompactHistoryBlock();

    virtual ~CompactHistoryBlock();

    virtual unsigned int remaining();

    virtual unsigned  length();

    virtual void *allocate(size_t size);
    virtual bool contains(void *addr);

    virtual void deallocate();
    virtual bool isInUse();

private:
    size_t _blockLength;
    quint8 *_head;
    quint8 *_tail;
    quint8 *_blockStart;
    int _allocCount;
};

}

#endif

/*
    This file is part of Konsole, an X terminal.
    Copyright 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
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

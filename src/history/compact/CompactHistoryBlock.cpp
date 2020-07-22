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

// Own
#include "CompactHistoryBlock.h"

using namespace Konsole;

CompactHistoryBlock::CompactHistoryBlock() :
    _blockLength(4096 * 64), // 256kb
    _head(static_cast<quint8 *>(mmap(nullptr, _blockLength, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0))),
    _tail(nullptr),
    _blockStart(nullptr),
    _allocCount(0)
{
    Q_ASSERT(_head != MAP_FAILED);
    _tail = _blockStart = _head;
}

CompactHistoryBlock::~CompactHistoryBlock()
{
    //free(_blockStart);
    munmap(_blockStart, _blockLength);
}

unsigned int CompactHistoryBlock::remaining()
{
    return _blockStart + _blockLength - _tail;
}

unsigned CompactHistoryBlock::length()
{
    return _blockLength;
}

bool CompactHistoryBlock::contains(void *addr)
{
    return addr >= _blockStart && addr < (_blockStart + _blockLength);
}

bool CompactHistoryBlock::isInUse()
{
    return _allocCount != 0;
}

void *CompactHistoryBlock::allocate(size_t size)
{
    Q_ASSERT(size > 0);
    if (_tail - _blockStart + size > _blockLength) {
        return nullptr;
    }

    void *block = _tail;
    _tail += size;
    ////qDebug() << "allocated " << length << " bytes at address " << block;
    _allocCount++;
    return block;
}

void CompactHistoryBlock::deallocate()
{
    _allocCount--;
    Q_ASSERT(_allocCount >= 0);
}

/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
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

/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "CompactHistoryBlockList.h"

// Qt

using namespace Konsole;

CompactHistoryBlockList::CompactHistoryBlockList()
    : _blocks()
{
}

void *CompactHistoryBlockList::allocate(size_t size)
{
    if (_blocks.empty() || _blocks.back()->remaining() < size) {
        _blocks.push_back(std::make_unique<CompactHistoryBlock>());
        ////qDebug() << "new block created, remaining " << block->remaining() << "number of blocks=" << _blocks.size();
    }
    return _blocks.back()->allocate(size);
}

void CompactHistoryBlockList::deallocate(void *ptr)
{
    Q_ASSERT(!_blocks.empty());

    auto block = _blocks.begin();
    while (block != _blocks.end() && !(*block)->contains(ptr)) {
        block++;
    }

    Q_ASSERT(block != _blocks.end());

    (*block)->deallocate();

    if (!(*block)->isInUse()) {
        _blocks.erase(block);
        ////qDebug() << "block deleted, new size = " << list.size();
    }
}

int CompactHistoryBlockList::length()
{
    return _blocks.size();
}

CompactHistoryBlockList::~CompactHistoryBlockList()
{
    _blocks.clear();
}

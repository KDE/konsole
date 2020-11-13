/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COMPACTHISTORYLINE_H
#define COMPACTHISTORYLINE_H

#include <QVector>

#include "../characters/CharacterFormat.h"
#include "CompactHistoryBlockList.h"

namespace Konsole
{

typedef QVector<Character> TextLine;

class CompactHistoryLine
{
public:
    CompactHistoryLine(const TextLine &, CompactHistoryBlockList &blockList);
    virtual ~CompactHistoryLine();

    // custom new operator to allocate memory from custom pool instead of heap
    static void *operator new(size_t size, CompactHistoryBlockList &blockList);
    static void operator delete(void *);

    virtual void getCharacters(Character *array, int size, int startColumn);
    virtual void getCharacter(int index, Character &r);
    virtual bool isWrapped() const;
    virtual void setWrapped(bool value);
    virtual unsigned int getLength() const;

protected:
    CompactHistoryBlockList &_blockListRef;
    CharacterFormat *_formatArray;
    quint16 _length;
    uint    *_text;
    quint16 _formatLength;
    bool _wrapped;
};

}

#endif

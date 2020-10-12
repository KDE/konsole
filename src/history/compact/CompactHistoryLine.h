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

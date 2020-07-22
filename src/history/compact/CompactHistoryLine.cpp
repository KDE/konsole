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

#include "CompactHistoryLine.h"

using namespace Konsole;

void *CompactHistoryLine::operator new(size_t size, CompactHistoryBlockList &blockList)
{
    return blockList.allocate(size);
}

void CompactHistoryLine::operator delete(void *)
{
    /* do nothing, deallocation from pool is done in destructor*/
}

CompactHistoryLine::CompactHistoryLine(const TextLine &line, CompactHistoryBlockList &bList) :
    _blockListRef(bList),
    _formatArray(nullptr),
    _text(nullptr),
    _formatLength(0),
    _wrapped(false)
{
    _length = line.size();

    if (!line.isEmpty()) {
        _formatLength = 1;
        int k = 1;

        // count number of different formats in this text line
        Character c = line[0];
        while (k < _length) {
            if (!(line[k].equalsFormat(c))) {
                _formatLength++; // format change detected
                c = line[k];
            }
            k++;
        }

        ////qDebug() << "number of different formats in string: " << _formatLength;
        _formatArray = static_cast<CharacterFormat *>(_blockListRef.allocate(sizeof(CharacterFormat) * _formatLength));
        Q_ASSERT(_formatArray != nullptr);
        _text = static_cast<uint *>(_blockListRef.allocate(sizeof(uint) * line.size()));
        Q_ASSERT(_text != nullptr);

        _length = line.size();
        _wrapped = false;

        // record formats and their positions in the format array
        c = line[0];
        _formatArray[0].setFormat(c);
        _formatArray[0].startPos = 0;                      // there's always at least 1 format (for the entire line, unless a change happens)

        k = 1;                                            // look for possible format changes
        int j = 1;
        while (k < _length && j < _formatLength) {
            if (!(line[k].equalsFormat(c))) {
                c = line[k];
                _formatArray[j].setFormat(c);
                _formatArray[j].startPos = k;
                ////qDebug() << "format entry " << j << " at pos " << _formatArray[j].startPos << " " << &(_formatArray[j].startPos) ;
                j++;
            }
            k++;
        }

        // copy character values
        for (int i = 0; i < line.size(); i++) {
            _text[i] = line[i].character;
            ////qDebug() << "char " << i << " at mem " << &(text[i]);
        }
    }
    ////qDebug() << "line created, length " << length << " at " << &(length);
}

CompactHistoryLine::~CompactHistoryLine()
{
    if (_length > 0) {
        _blockListRef.deallocate(_text);
        _blockListRef.deallocate(_formatArray);
    }
    _blockListRef.deallocate(this);
}

void CompactHistoryLine::getCharacter(int index, Character &r)
{
    Q_ASSERT(index < _length);
    int formatPos = 0;
    while ((formatPos + 1) < _formatLength && index >= _formatArray[formatPos + 1].startPos) {
        formatPos++;
    }

    r.character = _text[index];
    r.rendition = _formatArray[formatPos].rendition;
    r.foregroundColor = _formatArray[formatPos].fgColor;
    r.backgroundColor = _formatArray[formatPos].bgColor;
    r.isRealCharacter = _formatArray[formatPos].isRealCharacter;
}

void CompactHistoryLine::getCharacters(Character *array, int size, int startColumn)
{
    Q_ASSERT(startColumn >= 0 && size >= 0);
    Q_ASSERT(startColumn + size <= static_cast<int>(getLength()));

    for (int i = startColumn; i < size + startColumn; i++) {
        getCharacter(i, array[i - startColumn]);
    }
}

bool CompactHistoryLine::isWrapped() const
{
    return _wrapped;
}

void CompactHistoryLine::setWrapped(bool value)
{
    _wrapped = value;
}

unsigned int CompactHistoryLine::getLength() const
{
    return _length;
}

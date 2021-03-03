/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
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
    if (!line.isEmpty()) {
        _length = line.size();

        // count number of different formats in this text line
        Character c = line[0];
        _formatLength = 1;
        for (auto &k : line) {
            if (!(k.equalsFormat(c))) {
                _formatLength++; // format change detected
                c = k;
            }
        }

        _formatArray = static_cast<CharacterFormat *>(_blockListRef.allocate(sizeof(CharacterFormat) * _formatLength));
        Q_ASSERT(_formatArray != nullptr);
        _text = static_cast<uint *>(_blockListRef.allocate(sizeof(uint) * line.size()));
        Q_ASSERT(_text != nullptr);

        c = line[0];
        _formatArray[0].setFormat(c);
        _formatArray[0].startPos = 0;
        for (int i = 0, k = 1; i < _length; i++) {
            if (!line[i].equalsFormat(c)) {
                c = line[i];
                _formatArray[k].setFormat(c);
                _formatArray[k].startPos = i;
                k++;
            }
            _text[i] = line[i].character;
        }

        _wrapped = false;
    }
}

CompactHistoryLine::~CompactHistoryLine()
{
    if (_length > 0) {
        _blockListRef.deallocate(_text);
        _blockListRef.deallocate(_formatArray);
    }
    _blockListRef.deallocate(this);
}

void CompactHistoryLine::getCharacters(Character *array, int size, int startColumn)
{
    Q_ASSERT(startColumn >= 0 && size >= 0);
    Q_ASSERT(startColumn + size <= static_cast<int>(getLength()));

    int formatPos = 0;
    while ((formatPos + 1) < _formatLength && startColumn >= _formatArray[formatPos + 1].startPos) {
        formatPos++;
    }

    for (int i = startColumn; i < size + startColumn; i++) {
        if ((formatPos + 1) < _formatLength && i == _formatArray[formatPos + 1].startPos) {
            formatPos++;
        }
        array[i - startColumn] = Character(_text[i], 
                                           _formatArray[formatPos].fgColor, 
                                           _formatArray[formatPos].bgColor, 
                                           _formatArray[formatPos].rendition, 
                                           _formatArray[formatPos].isRealCharacter);
    }
}

void CompactHistoryLine::setCharacters(const TextLine &line)
{
    if (line.isEmpty()) {
        if (_formatArray) {
            _blockListRef.deallocate(_formatArray);
        }
        if (_text) {
            _blockListRef.deallocate(_text);
        }
        _formatArray = nullptr;
        _text = nullptr;
        _formatLength = 0;
        _length = 0;
        return;
    }
    int new_length = line.size();

    // count number of different formats in this text line
    Character c = line[0];
    int new_formatLength = 1;
    for (auto &k : line) {
        if (!(k.equalsFormat(c))) {
            new_formatLength++; // format change detected
            c = k;
        }
    }

    if (_formatLength < new_formatLength) {
        _blockListRef.deallocate(_formatArray);
        _formatArray = static_cast<CharacterFormat *>(_blockListRef.allocate(sizeof(CharacterFormat) * new_formatLength));
        Q_ASSERT(_formatArray != nullptr);
    }

    if (_length < new_length) {
        _blockListRef.deallocate(_text);
        _text = static_cast<uint *>(_blockListRef.allocate(sizeof(uint) * new_length));
        Q_ASSERT(_text != nullptr);
    }
    _length = new_length;
    _formatLength = new_formatLength;

    c = line[0];
    _formatArray[0].setFormat(c);
    _formatArray[0].startPos = 0;
    for (int i = 0, k = 1; i < _length; i++) {
        if (!line[i].equalsFormat(c)) {
            c = line[i];
            _formatArray[k].setFormat(c);
            _formatArray[k].startPos = i;
            k++;
        }
        _text[i] = line[i].character;
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

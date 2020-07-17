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
#include "History.h"

#include "HistoryScrollFile.h"

#include "konsoledebug.h"
#include "KonsoleSettings.h"

// System
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <sys/types.h>
#include <unistd.h>

// KDE
#include <QDir>
#include <qplatformdefs.h>
#include <QStandardPaths>
#include <KConfigGroup>
#include <KSharedConfig>

// Reasonable line size
static const int LINE_SIZE = 1024;

using namespace Konsole;

/*
   An arbitrary long scroll.

   One can modify the scroll only by adding either cells
   or newlines, but access it randomly.

   The model is that of an arbitrary wide typewriter scroll
   in that the scroll is a series of lines and each line is
   a series of cells with no overwriting permitted.

   The implementation provides arbitrary length and numbers
   of cells and line/column indexed read access to the scroll
   at constant costs.
*/

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

CompactHistoryBlockList::~CompactHistoryBlockList()
{
    qDeleteAll(list.begin(), list.end());
    list.clear();
}

void *CompactHistoryLine::operator new(size_t size, CompactHistoryBlockList &blockList)
{
    return blockList.allocate(size);
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

CompactHistoryScroll::CompactHistoryScroll(unsigned int maxLineCount) :
    HistoryScroll(new CompactHistoryType(maxLineCount)),
    _lines(),
    _blockList(),
    _maxLineCount(0)
{
    ////qDebug() << "scroll of length " << maxLineCount << " created";
    setMaxNbLines(maxLineCount);
}

CompactHistoryScroll::~CompactHistoryScroll()
{
    qDeleteAll(_lines.begin(), _lines.end());
    _lines.clear();
}

void CompactHistoryScroll::addCellsVector(const TextLine &cells)
{
    CompactHistoryLine *line;
    line = new(_blockList) CompactHistoryLine(cells, _blockList);

    if (_lines.size() > static_cast<int>(_maxLineCount)) {
        delete _lines.takeAt(0);
    }
    _lines.append(line);
}

void CompactHistoryScroll::addCells(const Character a[], int count)
{
    TextLine newLine(count);
    std::copy(a, a + count, newLine.begin());
    addCellsVector(newLine);
}

void CompactHistoryScroll::addLine(bool previousWrapped)
{
    CompactHistoryLine *line = _lines.last();
    ////qDebug() << "last line at address " << line;
    line->setWrapped(previousWrapped);
}

int CompactHistoryScroll::getLines()
{
    return _lines.size();
}

int CompactHistoryScroll::getLineLen(int lineNumber)
{
    if ((lineNumber < 0) || (lineNumber >= _lines.size())) {
        //qDebug() << "requested line invalid: 0 < " << lineNumber << " < " <<_lines.size();
        //Q_ASSERT(lineNumber >= 0 && lineNumber < _lines.size());
        return 0;
    }
    CompactHistoryLine *line = _lines[lineNumber];
    ////qDebug() << "request for line at address " << line;
    return line->getLength();
}

void CompactHistoryScroll::getCells(int lineNumber, int startColumn, int count, Character buffer[])
{
    if (count == 0) {
        return;
    }
    Q_ASSERT(lineNumber < _lines.size());
    CompactHistoryLine *line = _lines[lineNumber];
    Q_ASSERT(startColumn >= 0);
    Q_ASSERT(static_cast<unsigned int>(startColumn) <= line->getLength() - count);
    line->getCharacters(buffer, count, startColumn);
}

void CompactHistoryScroll::setMaxNbLines(unsigned int lineCount)
{
    _maxLineCount = lineCount;

    while (_lines.size() > static_cast<int>(lineCount)) {
        delete _lines.takeAt(0);
    }
    ////qDebug() << "set max lines to: " << _maxLineCount;
}

bool CompactHistoryScroll::isWrappedLine(int lineNumber)
{
    Q_ASSERT(lineNumber < _lines.size());
    return _lines[lineNumber]->isWrapped();
}

//////////////////////////////////////////////////////////////////////
// History Types
//////////////////////////////////////////////////////////////////////

HistoryType::HistoryType() = default;
HistoryType::~HistoryType() = default;

//////////////////////////////

HistoryTypeNone::HistoryTypeNone() = default;

bool HistoryTypeNone::isEnabled() const
{
    return false;
}

HistoryScroll *HistoryTypeNone::scroll(HistoryScroll *old) const
{
    delete old;
    return new HistoryScrollNone();
}

int HistoryTypeNone::maximumLineCount() const
{
    return 0;
}

//////////////////////////////

HistoryTypeFile::HistoryTypeFile() = default;

bool HistoryTypeFile::isEnabled() const
{
    return true;
}

HistoryScroll *HistoryTypeFile::scroll(HistoryScroll *old) const
{
    if (dynamic_cast<HistoryFile *>(old) != nullptr) {
        return old; // Unchanged.
    }
    HistoryScroll *newScroll = new HistoryScrollFile();

    Character line[LINE_SIZE];
    int lines = (old != nullptr) ? old->getLines() : 0;
    for (int i = 0; i < lines; i++) {
        int size = old->getLineLen(i);
        if (size > LINE_SIZE) {
            auto tmp_line = new Character[size];
            old->getCells(i, 0, size, tmp_line);
            newScroll->addCells(tmp_line, size);
            newScroll->addLine(old->isWrappedLine(i));
            delete [] tmp_line;
        } else {
            old->getCells(i, 0, size, line);
            newScroll->addCells(line, size);
            newScroll->addLine(old->isWrappedLine(i));
        }
    }

    delete old;
    return newScroll;
}

int HistoryTypeFile::maximumLineCount() const
{
    return -1;
}

//////////////////////////////

CompactHistoryType::CompactHistoryType(unsigned int nbLines) :
    _maxLines(nbLines)
{
}

bool CompactHistoryType::isEnabled() const
{
    return true;
}

int CompactHistoryType::maximumLineCount() const
{
    return _maxLines;
}

HistoryScroll *CompactHistoryType::scroll(HistoryScroll *old) const
{
    if (old != nullptr) {
        auto *oldBuffer = dynamic_cast<CompactHistoryScroll *>(old);
        if (oldBuffer != nullptr) {
            oldBuffer->setMaxNbLines(_maxLines);
            return oldBuffer;
        }
        delete old;
    }
    return new CompactHistoryScroll(_maxLines);
}

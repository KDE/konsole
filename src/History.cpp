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

// System
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

// KDE
#include <kde_file.h>
#include <KDebug>
#include <KStandardDirs>

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

// History File ///////////////////////////////////////////
HistoryFile::HistoryFile()
    : _fd(-1),
      _length(0),
      _fileMap(0),
      _readWriteBalance(0)
{
    const QString tmpFormat = KStandardDirs::locateLocal("tmp", QString())
                              + "konsole-XXXXXX.history";
    _tmpFile.setFileTemplate(tmpFormat);
    if (_tmpFile.open()) {
        _tmpFile.setAutoRemove(true);
        _fd = _tmpFile.handle();
    }
}

HistoryFile::~HistoryFile()
{
    if (_fileMap)
        unmap();
}

//TODO:  Mapping the entire file in will cause problems if the history file becomes exceedingly large,
//(ie. larger than available memory).  HistoryFile::map() should only map in sections of the file at a time,
//to avoid this.
void HistoryFile::map()
{
    Q_ASSERT(_fileMap == 0);

    _fileMap = (char*)mmap(0 , _length , PROT_READ , MAP_PRIVATE , _fd , 0);

    //if mmap'ing fails, fall back to the read-lseek combination
    if (_fileMap == MAP_FAILED) {
        _readWriteBalance = 0;
        _fileMap = 0;
        kWarning() << "mmap'ing history failed.  errno = " << errno;
    }
}

void HistoryFile::unmap()
{
    int result = munmap(_fileMap , _length);
    Q_ASSERT(result == 0);
    Q_UNUSED(result);

    _fileMap = 0;
}

bool HistoryFile::isMapped() const
{
    return (_fileMap != 0);
}

void HistoryFile::add(const unsigned char* buffer, int count)
{
    if (_fileMap)
        unmap();

    _readWriteBalance++;

    int rc = 0;

    rc = KDE_lseek(_fd, _length, SEEK_SET);
    if (rc < 0) {
        perror("HistoryFile::add.seek");
        return;
    }
    rc = write(_fd, buffer, count);
    if (rc < 0) {
        perror("HistoryFile::add.write");
        return;
    }
    _length += rc;
}

void HistoryFile::get(unsigned char* buffer, int size, int loc)
{
    //count number of get() calls vs. number of add() calls.
    //If there are many more get() calls compared with add()
    //calls (decided by using MAP_THRESHOLD) then mmap the log
    //file to improve performance.
    _readWriteBalance--;
    if (!_fileMap && _readWriteBalance < MAP_THRESHOLD)
        map();

    if (_fileMap) {
        for (int i = 0; i < size; i++)
            buffer[i] = _fileMap[loc + i];
    } else {
        int rc = 0;

        if (loc < 0 || size < 0 || loc + size > _length)
            fprintf(stderr, "getHist(...,%d,%d): invalid args.\n", size, loc);
        rc = KDE_lseek(_fd, loc, SEEK_SET);
        if (rc < 0) {
            perror("HistoryFile::get.seek");
            return;
        }
        rc = read(_fd, buffer, size);
        if (rc < 0) {
            perror("HistoryFile::get.read");
            return;
        }
    }
}

int HistoryFile::len() const
{
    return _length;
}

// History Scroll abstract base class //////////////////////////////////////

HistoryScroll::HistoryScroll(HistoryType* t)
    : _historyType(t)
{
}

HistoryScroll::~HistoryScroll()
{
    delete _historyType;
}

bool HistoryScroll::hasScroll()
{
    return true;
}

// History Scroll File //////////////////////////////////////

/*
   The history scroll makes a Row(Row(Cell)) from
   two history buffers. The index buffer contains
   start of line positions which refer to the cells
   buffer.

   Note that index[0] addresses the second line
   (line #1), while the first line (line #0) starts
   at 0 in cells.
*/

HistoryScrollFile::HistoryScrollFile(const QString& logFileName)
    : HistoryScroll(new HistoryTypeFile(logFileName))
{
}

HistoryScrollFile::~HistoryScrollFile()
{
}

int HistoryScrollFile::getLines()
{
    return _index.len() / sizeof(int);
}

int HistoryScrollFile::getLineLen(int lineno)
{
    return (startOfLine(lineno + 1) - startOfLine(lineno)) / sizeof(Character);
}

bool HistoryScrollFile::isWrappedLine(int lineno)
{
    if (lineno >= 0 && lineno <= getLines()) {
        unsigned char flag;
        _lineflags.get((unsigned char*)&flag, sizeof(unsigned char), (lineno)*sizeof(unsigned char));
        return flag;
    }
    return false;
}

int HistoryScrollFile::startOfLine(int lineno)
{
    if (lineno <= 0) return 0;
    if (lineno <= getLines()) {
        if (!_index.isMapped())
            _index.map();

        int res;
        _index.get((unsigned char*)&res, sizeof(int), (lineno - 1)*sizeof(int));
        return res;
    }
    return _cells.len();
}

void HistoryScrollFile::getCells(int lineno, int colno, int count, Character res[])
{
    _cells.get((unsigned char*)res, count * sizeof(Character), startOfLine(lineno) + colno * sizeof(Character));
}

void HistoryScrollFile::addCells(const Character text[], int count)
{
    _cells.add((unsigned char*)text, count * sizeof(Character));
}

void HistoryScrollFile::addLine(bool previousWrapped)
{
    if (_index.isMapped())
        _index.unmap();

    int locn = _cells.len();
    _index.add((unsigned char*)&locn, sizeof(int));
    unsigned char flags = previousWrapped ? 0x01 : 0x00;
    _lineflags.add((unsigned char*)&flags, sizeof(unsigned char));
}

// History Scroll None //////////////////////////////////////

HistoryScrollNone::HistoryScrollNone()
    : HistoryScroll(new HistoryTypeNone())
{
}

HistoryScrollNone::~HistoryScrollNone()
{
}

bool HistoryScrollNone::hasScroll()
{
    return false;
}

int  HistoryScrollNone::getLines()
{
    return 0;
}

int  HistoryScrollNone::getLineLen(int)
{
    return 0;
}

bool HistoryScrollNone::isWrappedLine(int /*lineno*/)
{
    return false;
}

void HistoryScrollNone::getCells(int, int, int, Character [])
{
}

void HistoryScrollNone::addCells(const Character [], int)
{
}

void HistoryScrollNone::addLine(bool)
{
}

////////////////////////////////////////////////////////////////
// Compact History Scroll //////////////////////////////////////
////////////////////////////////////////////////////////////////
void* CompactHistoryBlock::allocate(size_t size)
{
    Q_ASSERT(size > 0);
    if (_tail - _blockStart + size > _blockLength)
        return 0;

    void* block = _tail;
    _tail += size;
    //kDebug() << "allocated " << length << " bytes at address " << block;
    _allocCount++;
    return block;
}

void CompactHistoryBlock::deallocate()
{
    _allocCount--;
    Q_ASSERT(_allocCount >= 0);
}

void* CompactHistoryBlockList::allocate(size_t size)
{
    CompactHistoryBlock* block;
    if (list.isEmpty() || list.last()->remaining() < size) {
        block = new CompactHistoryBlock();
        list.append(block);
        //kDebug() << "new block created, remaining " << block->remaining() << "number of blocks=" << list.size();
    } else {
        block = list.last();
        //kDebug() << "old block used, remaining " << block->remaining();
    }
    return block->allocate(size);
}

void CompactHistoryBlockList::deallocate(void* ptr)
{
    Q_ASSERT(!list.isEmpty());

    int i = 0;
    CompactHistoryBlock* block = list.at(i);
    while (i < list.size() && !block->contains(ptr)) {
        i++;
        block = list.at(i);
    }

    Q_ASSERT(i < list.size());

    block->deallocate();

    if (!block->isInUse()) {
        list.removeAt(i);
        delete block;
        //kDebug() << "block deleted, new size = " << list.size();
    }
}

CompactHistoryBlockList::~CompactHistoryBlockList()
{
    qDeleteAll(list.begin(), list.end());
    list.clear();
}

void* CompactHistoryLine::operator new(size_t size, CompactHistoryBlockList& blockList)
{
    return blockList.allocate(size);
}

CompactHistoryLine::CompactHistoryLine(const TextLine& line, CompactHistoryBlockList& bList)
    : _blockListRef(bList),
      _formatLength(0)
{
    _length = line.size();

    if (line.size() > 0) {
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

        //kDebug() << "number of different formats in string: " << _formatLength;
        _formatArray = (CharacterFormat*) _blockListRef.allocate(sizeof(CharacterFormat) * _formatLength);
        Q_ASSERT(_formatArray != 0);
        _text = (quint16*) _blockListRef.allocate(sizeof(quint16) * line.size());
        Q_ASSERT(_text != 0);

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
                //kDebug() << "format entry " << j << " at pos " << _formatArray[j].startPos << " " << &(_formatArray[j].startPos) ;
                j++;
            }
            k++;
        }

        // copy character values
        for (int i = 0; i < line.size(); i++) {
            _text[i] = line[i].character;
            //kDebug() << "char " << i << " at mem " << &(text[i]);
        }
    }
    //kDebug() << "line created, length " << length << " at " << &(length);
}

CompactHistoryLine::~CompactHistoryLine()
{
    if (_length > 0) {
        _blockListRef.deallocate(_text);
        _blockListRef.deallocate(_formatArray);
    }
    _blockListRef.deallocate(this);
}

void CompactHistoryLine::getCharacter(int index, Character& r)
{
    Q_ASSERT(index < _length);
    int formatPos = 0;
    while ((formatPos + 1) < _formatLength && index >= _formatArray[formatPos + 1].startPos)
        formatPos++;

    r.character = _text[index];
    r.rendition = _formatArray[formatPos].rendition;
    r.foregroundColor = _formatArray[formatPos].fgColor;
    r.backgroundColor = _formatArray[formatPos].bgColor;
    r.isRealCharacter = _formatArray[formatPos].isRealCharacter;
}

void CompactHistoryLine::getCharacters(Character* array, int size, int startColumn)
{
    Q_ASSERT(startColumn >= 0 && size >= 0);
    Q_ASSERT(startColumn + size <= (int) getLength());

    for (int i = startColumn; i < size + startColumn; i++) {
        getCharacter(i, array[i - startColumn]);
    }
}

CompactHistoryScroll::CompactHistoryScroll(unsigned int maxLineCount)
    : HistoryScroll(new CompactHistoryType(maxLineCount))
    , _lines()
    , _blockList()
{
    //kDebug() << "scroll of length " << maxLineCount << " created";
    setMaxNbLines(maxLineCount);
}

CompactHistoryScroll::~CompactHistoryScroll()
{
    qDeleteAll(_lines.begin(), _lines.end());
    _lines.clear();
}

void CompactHistoryScroll::addCellsVector(const TextLine& cells)
{
    CompactHistoryLine* line;
    line = new(_blockList) CompactHistoryLine(cells, _blockList);

    if (_lines.size() > (int) _maxLineCount) {
        delete _lines.takeAt(0);
    }
    _lines.append(line);
}

void CompactHistoryScroll::addCells(const Character a[], int count)
{
    TextLine newLine(count);
    qCopy(a, a + count, newLine.begin());
    addCellsVector(newLine);
}

void CompactHistoryScroll::addLine(bool previousWrapped)
{
    CompactHistoryLine* line = _lines.last();
    //kDebug() << "last line at address " << line;
    line->setWrapped(previousWrapped);
}

int CompactHistoryScroll::getLines()
{
    return _lines.size();
}

int CompactHistoryScroll::getLineLen(int lineNumber)
{
    Q_ASSERT(lineNumber >= 0 && lineNumber < _lines.size());
    CompactHistoryLine* line = _lines[lineNumber];
    //kDebug() << "request for line at address " << line;
    return line->getLength();
}

void CompactHistoryScroll::getCells(int lineNumber, int startColumn, int count, Character buffer[])
{
    if (count == 0) return;
    Q_ASSERT(lineNumber < _lines.size());
    CompactHistoryLine* line = _lines[lineNumber];
    Q_ASSERT(startColumn >= 0);
    Q_ASSERT((unsigned int)startColumn <= line->getLength() - count);
    line->getCharacters(buffer, count, startColumn);
}

void CompactHistoryScroll::setMaxNbLines(unsigned int lineCount)
{
    _maxLineCount = lineCount;

    while (_lines.size() > (int) lineCount) {
        delete _lines.takeAt(0);
    }
    //kDebug() << "set max lines to: " << _maxLineCount;
}

bool CompactHistoryScroll::isWrappedLine(int lineNumber)
{
    Q_ASSERT(lineNumber < _lines.size());
    return _lines[lineNumber]->isWrapped();
}

//////////////////////////////////////////////////////////////////////
// History Types
//////////////////////////////////////////////////////////////////////

HistoryType::HistoryType()
{
}

HistoryType::~HistoryType()
{
}

//////////////////////////////

HistoryTypeNone::HistoryTypeNone()
{
}

bool HistoryTypeNone::isEnabled() const
{
    return false;
}

HistoryScroll* HistoryTypeNone::scroll(HistoryScroll* old) const
{
    delete old;
    return new HistoryScrollNone();
}

int HistoryTypeNone::maximumLineCount() const
{
    return 0;
}

//////////////////////////////

HistoryTypeFile::HistoryTypeFile(const QString& fileName)
    : _fileName(fileName)
{
}

bool HistoryTypeFile::isEnabled() const
{
    return true;
}

HistoryScroll* HistoryTypeFile::scroll(HistoryScroll* old) const
{
    if (dynamic_cast<HistoryFile *>(old))
        return old; // Unchanged.

    HistoryScroll* newScroll = new HistoryScrollFile(_fileName);

    Character line[LINE_SIZE];
    int lines = (old != 0) ? old->getLines() : 0;
    for (int i = 0; i < lines; i++) {
        int size = old->getLineLen(i);
        if (size > LINE_SIZE) {
            Character* tmp_line = new Character[size];
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

CompactHistoryType::CompactHistoryType(unsigned int nbLines)
    : _maxLines(nbLines)
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

HistoryScroll* CompactHistoryType::scroll(HistoryScroll* old) const
{
    if (old) {
        CompactHistoryScroll* oldBuffer = dynamic_cast<CompactHistoryScroll*>(old);
        if (oldBuffer) {
            oldBuffer->setMaxNbLines(_maxLines);
            return oldBuffer;
        }
        delete old;
    }
    return new CompactHistoryScroll(_maxLines);
}

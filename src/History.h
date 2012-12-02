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

#ifndef HISTORY_H
#define HISTORY_H

// System
#include <sys/mman.h>

// Qt
#include <QtCore/QList>
#include <QtCore/QVector>
#include <QtCore/QTemporaryFile>

// Konsole
#include "Character.h"

namespace Konsole
{
/*
   An extendable tmpfile(1) based buffer.
*/

class HistoryFile
{
public:
    HistoryFile();
    virtual ~HistoryFile();

    virtual void add(const unsigned char* bytes, int len);
    virtual void get(unsigned char* bytes, int len, int loc);
    virtual int  len() const;

    //mmaps the file in read-only mode
    void map();
    //un-mmaps the file
    void unmap();
    //returns true if the file is mmap'ed
    bool isMapped() const;


private:
    int  _fd;
    int  _length;
    QTemporaryFile _tmpFile;

    //pointer to start of mmap'ed file data, or 0 if the file is not mmap'ed
    char* _fileMap;

    //incremented whenever 'add' is called and decremented whenever
    //'get' is called.
    //this is used to detect when a large number of lines are being read and processed from the history
    //and automatically mmap the file for better performance (saves the overhead of many lseek-read calls).
    int _readWriteBalance;

    //when _readWriteBalance goes below this threshold, the file will be mmap'ed automatically
    static const int MAP_THRESHOLD = -1000;
};

//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Abstract base class for file and buffer versions
//////////////////////////////////////////////////////////////////////
class HistoryType;

class HistoryScroll
{
public:
    explicit HistoryScroll(HistoryType*);
    virtual ~HistoryScroll();

    virtual bool hasScroll();

    // access to history
    virtual int  getLines() = 0;
    virtual int  getLineLen(int lineno) = 0;
    virtual void getCells(int lineno, int colno, int count, Character res[]) = 0;
    virtual bool isWrappedLine(int lineno) = 0;

    // adding lines.
    virtual void addCells(const Character a[], int count) = 0;
    // convenience method - this is virtual so that subclasses can take advantage
    // of QVector's implicit copying
    virtual void addCellsVector(const QVector<Character>& cells) {
        addCells(cells.data(), cells.size());
    }

    virtual void addLine(bool previousWrapped = false) = 0;

    //
    // FIXME:  Passing around constant references to HistoryType instances
    // is very unsafe, because those references will no longer
    // be valid if the history scroll is deleted.
    //
    const HistoryType& getType() const {
        return *_historyType;
    }

protected:
    HistoryType* _historyType;
};

//////////////////////////////////////////////////////////////////////
// File-based history (e.g. file log, no limitation in length)
//////////////////////////////////////////////////////////////////////

class HistoryScrollFile : public HistoryScroll
{
public:
    explicit HistoryScrollFile(const QString& logFileName);
    virtual ~HistoryScrollFile();

    virtual int  getLines();
    virtual int  getLineLen(int lineno);
    virtual void getCells(int lineno, int colno, int count, Character res[]);
    virtual bool isWrappedLine(int lineno);

    virtual void addCells(const Character a[], int count);
    virtual void addLine(bool previousWrapped = false);

private:
    int startOfLine(int lineno);

    HistoryFile _index; // lines Row(int)
    HistoryFile _cells; // text  Row(Character)
    HistoryFile _lineflags; // flags Row(unsigned char)
};

//////////////////////////////////////////////////////////////////////
// Nothing-based history (no history :-)
//////////////////////////////////////////////////////////////////////
class HistoryScrollNone : public HistoryScroll
{
public:
    HistoryScrollNone();
    virtual ~HistoryScrollNone();

    virtual bool hasScroll();

    virtual int  getLines();
    virtual int  getLineLen(int lineno);
    virtual void getCells(int lineno, int colno, int count, Character res[]);
    virtual bool isWrappedLine(int lineno);

    virtual void addCells(const Character a[], int count);
    virtual void addLine(bool previousWrapped = false);
};

//////////////////////////////////////////////////////////////////////
// History using compact storage
// This implementation uses a list of fixed-sized blocks
// where history lines are allocated in (avoids heap fragmentation)
//////////////////////////////////////////////////////////////////////
typedef QVector<Character> TextLine;

class CharacterFormat
{
public:
    bool equalsFormat(const CharacterFormat& other) const {
        return (other.rendition & ~RE_EXTENDED_CHAR) == (rendition & ~RE_EXTENDED_CHAR) && other.fgColor == fgColor && other.bgColor == bgColor;
    }

    bool equalsFormat(const Character& c) const {
        return (c.rendition & ~RE_EXTENDED_CHAR) == (rendition & ~RE_EXTENDED_CHAR) && c.foregroundColor == fgColor && c.backgroundColor == bgColor;
    }

    void setFormat(const Character& c) {
        rendition = c.rendition;
        fgColor = c.foregroundColor;
        bgColor = c.backgroundColor;
        isRealCharacter = c.isRealCharacter;
    }

    CharacterColor fgColor, bgColor;
    quint16 startPos;
    quint8 rendition;
    bool isRealCharacter;
};

class CompactHistoryBlock
{
public:
    CompactHistoryBlock() {
        _blockLength = 4096 * 64; // 256kb
        _head = (quint8*) mmap(0, _blockLength, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        //_head = (quint8*) malloc(_blockLength);
        Q_ASSERT(_head != MAP_FAILED);
        _tail = _blockStart = _head;
        _allocCount = 0;
    }

    virtual ~CompactHistoryBlock() {
        //free(_blockStart);
        munmap(_blockStart, _blockLength);
    }

    virtual unsigned int remaining() {
        return _blockStart + _blockLength - _tail;
    }
    virtual unsigned  length() {
        return _blockLength;
    }
    virtual void* allocate(size_t length);
    virtual bool contains(void* addr) {
        return addr >= _blockStart && addr < (_blockStart + _blockLength);
    }
    virtual void deallocate();
    virtual bool isInUse() {
        return _allocCount != 0;
    };

private:
    size_t _blockLength;
    quint8* _head;
    quint8* _tail;
    quint8* _blockStart;
    int _allocCount;
};

class CompactHistoryBlockList
{
public:
    CompactHistoryBlockList() {};
    ~CompactHistoryBlockList();

    void* allocate(size_t size);
    void deallocate(void *);
    int length() {
        return list.size();
    }
private:
    QList<CompactHistoryBlock*> list;
};

class CompactHistoryLine
{
public:
    CompactHistoryLine(const TextLine&, CompactHistoryBlockList& blockList);
    virtual ~CompactHistoryLine();

    // custom new operator to allocate memory from custom pool instead of heap
    static void* operator new(size_t size, CompactHistoryBlockList& blockList);
    static void operator delete(void *) {
        /* do nothing, deallocation from pool is done in destructor*/
    };

    virtual void getCharacters(Character* array, int length, int startColumn);
    virtual void getCharacter(int index, Character& r);
    virtual bool isWrapped() const {
        return _wrapped;
    };
    virtual void setWrapped(bool value) {
        _wrapped = value;
    };
    virtual unsigned int getLength() const {
        return _length;
    };

protected:
    CompactHistoryBlockList& _blockListRef;
    CharacterFormat* _formatArray;
    quint16 _length;
    quint16* _text;
    quint16 _formatLength;
    bool _wrapped;
};

class CompactHistoryScroll : public HistoryScroll
{
    typedef QList<CompactHistoryLine*> HistoryArray;

public:
    explicit CompactHistoryScroll(unsigned int maxNbLines = 1000);
    virtual ~CompactHistoryScroll();

    virtual int  getLines();
    virtual int  getLineLen(int lineno);
    virtual void getCells(int lineno, int colno, int count, Character res[]);
    virtual bool isWrappedLine(int lineno);

    virtual void addCells(const Character a[], int count);
    virtual void addCellsVector(const TextLine& cells);
    virtual void addLine(bool previousWrapped = false);

    void setMaxNbLines(unsigned int nbLines);

private:
    bool hasDifferentColors(const TextLine& line) const;
    HistoryArray _lines;
    CompactHistoryBlockList _blockList;

    unsigned int _maxLineCount;
};

//////////////////////////////////////////////////////////////////////
// History type
//////////////////////////////////////////////////////////////////////

class HistoryType
{
public:
    HistoryType();
    virtual ~HistoryType();

    /**
     * Returns true if the history is enabled ( can store lines of output )
     * or false otherwise.
     */
    virtual bool isEnabled() const = 0;
    /**
     * Returns the maximum number of lines which this history type
     * can store or -1 if the history can store an unlimited number of lines.
     */
    virtual int maximumLineCount() const = 0;
    /**
     * TODO: document me
     */
    virtual HistoryScroll* scroll(HistoryScroll *) const = 0;
    /**
     * Returns true if the history size is unlimited.
     */
    bool isUnlimited() const {
        return maximumLineCount() == -1;
    }
};

class HistoryTypeNone : public HistoryType
{
public:
    HistoryTypeNone();

    virtual bool isEnabled() const;
    virtual int maximumLineCount() const;

    virtual HistoryScroll* scroll(HistoryScroll *) const;
};

class HistoryTypeFile : public HistoryType
{
public:
    explicit HistoryTypeFile(const QString& fileName = QString());

    virtual bool isEnabled() const;
    virtual int maximumLineCount() const;

    virtual HistoryScroll* scroll(HistoryScroll *) const;

protected:
    QString _fileName;
};

class CompactHistoryType : public HistoryType
{
public:
    explicit CompactHistoryType(unsigned int size);

    virtual bool isEnabled() const;
    virtual int maximumLineCount() const;

    virtual HistoryScroll* scroll(HistoryScroll *) const;

protected:
    unsigned int _maxLines;
};
}

#endif // HISTORY_H

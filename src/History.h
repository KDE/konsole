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
#include <QList>
#include <QVector>
#include <QTemporaryFile>

#include "konsoleprivate_export.h"

// Konsole
#include "Character.h"

namespace Konsole {
/*
   An extendable tmpfile(1) based buffer.
*/

class HistoryFile
{
public:
    HistoryFile();
    virtual ~HistoryFile();

    virtual void add(const char *buffer, qint64 count);
    virtual void get(char *buffer, qint64 size, qint64 loc);
    virtual qint64 len() const;

    //mmaps the file in read-only mode
    void map();
    //un-mmaps the file
    void unmap();
    //returns true if the file is mmap'ed
    bool isMapped() const;

private:
    qint64 _length;
    QTemporaryFile _tmpFile;

    //pointer to start of mmap'ed file data, or 0 if the file is not mmap'ed
    uchar *_fileMap;

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

class KONSOLEPRIVATE_EXPORT HistoryScroll
{
public:
    explicit HistoryScroll(HistoryType *);
    virtual ~HistoryScroll();

    virtual bool hasScroll();

    // access to history
    virtual int  getLines() = 0;
    virtual int  getLineLen(int lineno) = 0;
    virtual void getCells(int lineno, int colno, int count, Character res[]) = 0;
    virtual bool isWrappedLine(int lineNumber) = 0;

    // adding lines.
    virtual void addCells(const Character a[], int count) = 0;
    // convenience method - this is virtual so that subclasses can take advantage
    // of QVector's implicit copying
    virtual void addCellsVector(const QVector<Character> &cells)
    {
        addCells(cells.data(), cells.size());
    }

    virtual void addLine(bool previousWrapped = false) = 0;

    //
    // FIXME:  Passing around constant references to HistoryType instances
    // is very unsafe, because those references will no longer
    // be valid if the history scroll is deleted.
    //
    const HistoryType &getType() const
    {
        return *_historyType;
    }

protected:
    HistoryType *_historyType;
};

//////////////////////////////////////////////////////////////////////
// File-based history (e.g. file log, no limitation in length)
//////////////////////////////////////////////////////////////////////

class KONSOLEPRIVATE_EXPORT HistoryScrollFile : public HistoryScroll
{
public:
    explicit HistoryScrollFile(const QString &logFileName);
    ~HistoryScrollFile() Q_DECL_OVERRIDE;

    int  getLines() Q_DECL_OVERRIDE;
    int  getLineLen(int lineno) Q_DECL_OVERRIDE;
    void getCells(int lineno, int colno, int count, Character res[]) Q_DECL_OVERRIDE;
    bool isWrappedLine(int lineno) Q_DECL_OVERRIDE;

    void addCells(const Character text[], int count) Q_DECL_OVERRIDE;
    void addLine(bool previousWrapped = false) Q_DECL_OVERRIDE;

private:
    qint64 startOfLine(int lineno);

    HistoryFile _index; // lines Row(qint64)
    HistoryFile _cells; // text  Row(Character)
    HistoryFile _lineflags; // flags Row(unsigned char)
};

//////////////////////////////////////////////////////////////////////
// Nothing-based history (no history :-)
//////////////////////////////////////////////////////////////////////
class KONSOLEPRIVATE_EXPORT HistoryScrollNone : public HistoryScroll
{
public:
    HistoryScrollNone();
    ~HistoryScrollNone() Q_DECL_OVERRIDE;

    bool hasScroll() Q_DECL_OVERRIDE;

    int  getLines() Q_DECL_OVERRIDE;
    int  getLineLen(int lineno) Q_DECL_OVERRIDE;
    void getCells(int lineno, int colno, int count, Character res[]) Q_DECL_OVERRIDE;
    bool isWrappedLine(int lineno) Q_DECL_OVERRIDE;

    void addCells(const Character a[], int count) Q_DECL_OVERRIDE;
    void addLine(bool previousWrapped = false) Q_DECL_OVERRIDE;
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
    bool equalsFormat(const CharacterFormat &other) const
    {
        return (other.rendition & ~RE_EXTENDED_CHAR) == (rendition & ~RE_EXTENDED_CHAR)
               && other.fgColor == fgColor && other.bgColor == bgColor;
    }

    bool equalsFormat(const Character &c) const
    {
        return (c.rendition & ~RE_EXTENDED_CHAR) == (rendition & ~RE_EXTENDED_CHAR)
               && c.foregroundColor == fgColor && c.backgroundColor == bgColor;
    }

    void setFormat(const Character &c)
    {
        rendition = c.rendition;
        fgColor = c.foregroundColor;
        bgColor = c.backgroundColor;
        isRealCharacter = c.isRealCharacter;
    }

    CharacterColor fgColor, bgColor;
    quint16 startPos;
    RenditionFlags rendition;
    bool isRealCharacter;
};

class CompactHistoryBlock
{
public:
    CompactHistoryBlock() :
        _blockLength(4096 * 64), // 256kb
        _head(static_cast<quint8 *>(mmap(nullptr, _blockLength, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0))),
        _tail(nullptr),
        _blockStart(nullptr),
        _allocCount(0)
    {
        Q_ASSERT(_head != MAP_FAILED);
        _tail = _blockStart = _head;
    }

    virtual ~CompactHistoryBlock()
    {
        //free(_blockStart);
        munmap(_blockStart, _blockLength);
    }

    virtual unsigned int remaining()
    {
        return _blockStart + _blockLength - _tail;
    }

    virtual unsigned  length()
    {
        return _blockLength;
    }

    virtual void *allocate(size_t size);
    virtual bool contains(void *addr)
    {
        return addr >= _blockStart && addr < (_blockStart + _blockLength);
    }

    virtual void deallocate();
    virtual bool isInUse()
    {
        return _allocCount != 0;
    }

private:
    size_t _blockLength;
    quint8 *_head;
    quint8 *_tail;
    quint8 *_blockStart;
    int _allocCount;
};

class CompactHistoryBlockList
{
public:
    CompactHistoryBlockList() :
        list(QList<CompactHistoryBlock *>())
    {
    }

    ~CompactHistoryBlockList();

    void *allocate(size_t size);
    void deallocate(void *);
    int length()
    {
        return list.size();
    }

private:
    QList<CompactHistoryBlock *> list;
};

class CompactHistoryLine
{
public:
    CompactHistoryLine(const TextLine &, CompactHistoryBlockList &blockList);
    virtual ~CompactHistoryLine();

    // custom new operator to allocate memory from custom pool instead of heap
    static void *operator new(size_t size, CompactHistoryBlockList &blockList);
    static void operator delete(void *)
    {
        /* do nothing, deallocation from pool is done in destructor*/
    }

    virtual void getCharacters(Character *array, int size, int startColumn);
    virtual void getCharacter(int index, Character &r);
    virtual bool isWrapped() const
    {
        return _wrapped;
    }

    virtual void setWrapped(bool value)
    {
        _wrapped = value;
    }

    virtual unsigned int getLength() const
    {
        return _length;
    }

protected:
    CompactHistoryBlockList &_blockListRef;
    CharacterFormat *_formatArray;
    quint16 _length;
    quint16 *_text;
    quint16 _formatLength;
    bool _wrapped;
};

class KONSOLEPRIVATE_EXPORT CompactHistoryScroll : public HistoryScroll
{
    typedef QList<CompactHistoryLine *> HistoryArray;

public:
    explicit CompactHistoryScroll(unsigned int maxNbLines = 1000);
    ~CompactHistoryScroll() Q_DECL_OVERRIDE;

    int  getLines() Q_DECL_OVERRIDE;
    int  getLineLen(int lineNumber) Q_DECL_OVERRIDE;
    void getCells(int lineNumber, int startColumn, int count, Character buffer[]) Q_DECL_OVERRIDE;
    bool isWrappedLine(int lineNumber) Q_DECL_OVERRIDE;

    void addCells(const Character a[], int count) Q_DECL_OVERRIDE;
    void addCellsVector(const TextLine &cells) Q_DECL_OVERRIDE;
    void addLine(bool previousWrapped = false) Q_DECL_OVERRIDE;

    void setMaxNbLines(unsigned int lineCount);

private:
    bool hasDifferentColors(const TextLine &line) const;
    HistoryArray _lines;
    CompactHistoryBlockList _blockList;

    unsigned int _maxLineCount;
};

//////////////////////////////////////////////////////////////////////
// History type
//////////////////////////////////////////////////////////////////////

class KONSOLEPRIVATE_EXPORT HistoryType
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
     * Converts from one type of HistoryScroll to another or if given the
     * same type, returns it.
     */
    virtual HistoryScroll *scroll(HistoryScroll *) const = 0;
    /**
     * Returns true if the history size is unlimited.
     */
    bool isUnlimited() const
    {
        return maximumLineCount() == -1;
    }
};

class KONSOLEPRIVATE_EXPORT HistoryTypeNone : public HistoryType
{
public:
    HistoryTypeNone();

    bool isEnabled() const Q_DECL_OVERRIDE;
    int maximumLineCount() const Q_DECL_OVERRIDE;

    HistoryScroll *scroll(HistoryScroll *) const Q_DECL_OVERRIDE;
};

class KONSOLEPRIVATE_EXPORT HistoryTypeFile : public HistoryType
{
public:
    explicit HistoryTypeFile(const QString &fileName = QString());

    bool isEnabled() const Q_DECL_OVERRIDE;
    int maximumLineCount() const Q_DECL_OVERRIDE;

    HistoryScroll *scroll(HistoryScroll *) const Q_DECL_OVERRIDE;

protected:
    QString _fileName;
};

class KONSOLEPRIVATE_EXPORT CompactHistoryType : public HistoryType
{
public:
    explicit CompactHistoryType(unsigned int nbLines);

    bool isEnabled() const Q_DECL_OVERRIDE;
    int maximumLineCount() const Q_DECL_OVERRIDE;

    HistoryScroll *scroll(HistoryScroll *) const Q_DECL_OVERRIDE;

protected:
    unsigned int _maxLines;
};
}

#endif // HISTORY_H

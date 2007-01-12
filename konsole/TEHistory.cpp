/*
    This file is part of Konsole, an X terminal.
    Copyright (C) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

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

#include <iostream>
#include "TEHistory.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <kdebug.h>

// Reasonable line size
#define LINE_SIZE	1024

/*
   An arbitrary long scroll.

   One can modify the scroll only by adding either cells
   or newlines, but access it randomly.

   The model is that of an arbitrary wide typewriter scroll
   in that the scroll is a serie of lines and each line is
   a serie of cells with no overwriting permitted.

   The implementation provides arbitrary length and numbers
   of cells and line/column indexed read access to the scroll
   at constant costs.

FIXME: some complain about the history buffer comsuming the
       memory of their machines. This problem is critical
       since the history does not behave gracefully in cases
       where the memory is used up completely.

       I put in a workaround that should handle it problem
       now gracefully. I'm not satisfied with the solution.

FIXME: Terminating the history is not properly indicated
       in the menu. We should throw a signal.

FIXME: There is noticeable decrease in speed, also. Perhaps,
       there whole feature needs to be revisited therefore.
       Disadvantage of a more elaborated, say block-oriented
       scheme with wrap around would be it's complexity.
*/

//FIXME: tempory replacement for tmpfile
//       this is here one for debugging purpose.

//#define tmpfile xTmpFile

// History File ///////////////////////////////////////////

/*
  A Row(X) data type which allows adding elements to the end.
*/

HistoryFile::HistoryFile()
  : ion(-1),
    length(0)
{
  if (tmpFile.status() == 0)
  { 
    tmpFile.unlink();
    ion = tmpFile.handle();
  }
}

HistoryFile::~HistoryFile()
{
}

void HistoryFile::add(const unsigned char* bytes, int len)
{
  int rc = 0;

  rc = lseek(ion,length,SEEK_SET); if (rc < 0) { perror("HistoryFile::add.seek"); return; }
  rc = write(ion,bytes,len);       if (rc < 0) { perror("HistoryFile::add.write"); return; }
  length += rc;
}

void HistoryFile::get(unsigned char* bytes, int len, int loc)
{
  int rc = 0;

  if (loc < 0 || len < 0 || loc + len > length)
    fprintf(stderr,"getHist(...,%d,%d): invalid args.\n",len,loc);
  rc = lseek(ion,loc,SEEK_SET); if (rc < 0) { perror("HistoryFile::get.seek"); return; }
  rc = read(ion,bytes,len);     if (rc < 0) { perror("HistoryFile::get.read"); return; }
}

int HistoryFile::len()
{
  return length;
}


// History Scroll abstract base class //////////////////////////////////////


HistoryScroll::HistoryScroll(HistoryType* t)
  : m_histType(t)
{
}

HistoryScroll::~HistoryScroll()
{
  delete m_histType;
}

bool HistoryScroll::hasScroll()
{
  return true;
}

// History Scroll File //////////////////////////////////////

/* 
   The history scroll makes a Row(Row(Cell)) from
   two history buffers. The index buffer contains
   start of line positions which refere to the cells
   buffer.

   Note that index[0] addresses the second line
   (line #1), while the first line (line #0) starts
   at 0 in cells.
*/

HistoryScrollFile::HistoryScrollFile(const QString &logFileName)
  : HistoryScroll(new HistoryTypeFile(logFileName)),
  m_logFileName(logFileName)
{
}

HistoryScrollFile::~HistoryScrollFile()
{
}
 
int HistoryScrollFile::getLines()
{
  return index.len() / sizeof(int);
}

int HistoryScrollFile::getLineLen(int lineno)
{
  return (startOfLine(lineno+1) - startOfLine(lineno)) / sizeof(ca);
}

bool HistoryScrollFile::isWrappedLine(int lineno)
{
  if (lineno>=0 && lineno <= getLines()) {
    unsigned char flag;
    lineflags.get((unsigned char*)&flag,sizeof(unsigned char),(lineno)*sizeof(unsigned char));
    return flag;
  }
  return false;
}

int HistoryScrollFile::startOfLine(int lineno)
{
  if (lineno <= 0) return 0;
  if (lineno <= getLines())
    { int res;
    index.get((unsigned char*)&res,sizeof(int),(lineno-1)*sizeof(int));
    return res;
    }
  return cells.len();
}

void HistoryScrollFile::getCells(int lineno, int colno, int count, ca res[])
{
  cells.get((unsigned char*)res,count*sizeof(ca),startOfLine(lineno)+colno*sizeof(ca));
}

void HistoryScrollFile::addCells(ca text[], int count)
{
  cells.add((unsigned char*)text,count*sizeof(ca));
}

void HistoryScrollFile::addLine(bool previousWrapped)
{
  int locn = cells.len();
  index.add((unsigned char*)&locn,sizeof(int));
  unsigned char flags = previousWrapped ? 0x01 : 0x00;
  lineflags.add((unsigned char*)&flags,sizeof(unsigned char));
}


// History Scroll Buffer //////////////////////////////////////
HistoryScrollBuffer::HistoryScrollBuffer(unsigned int maxNbLines)
  : HistoryScroll(new HistoryTypeBuffer(maxNbLines)),
    m_histBuffer(maxNbLines),
    m_wrappedLine(maxNbLines),
    m_maxNbLines(maxNbLines),
    m_nbLines(0),
    m_arrayIndex(maxNbLines - 1)
{
}

HistoryScrollBuffer::~HistoryScrollBuffer()
{
   for(size_t line = 0; line < m_nbLines; ++line) {
      delete m_histBuffer[adjustLineNb(line)];   
   }
}

void HistoryScrollBuffer::addCells(ca a[], int count)
{
  histline* newLine = new histline;

  newLine->duplicate(a, count);
  
  ++m_arrayIndex;
  if (m_arrayIndex >= m_maxNbLines) {
     m_arrayIndex = 0;
  }

  if (m_nbLines < m_maxNbLines) ++m_nbLines;

  delete m_histBuffer[m_arrayIndex];
  m_histBuffer.insert(m_arrayIndex, newLine);
  m_wrappedLine.clearBit(m_arrayIndex);
}

void HistoryScrollBuffer::addLine(bool previousWrapped)
{
  m_wrappedLine.setBit(m_arrayIndex,previousWrapped);
}

int HistoryScrollBuffer::getLines()
{
  return m_nbLines; // m_histBuffer.size();
}

int HistoryScrollBuffer::getLineLen(int lineno)
{
  if (lineno >= (int) m_maxNbLines) return 0;

  lineno = adjustLineNb(lineno);

  histline *l = m_histBuffer[lineno];

  return l ? l->size() : 0;
}

bool HistoryScrollBuffer::isWrappedLine(int lineno)
{
  if (lineno >= (int) m_maxNbLines)
    return 0;

  return m_wrappedLine[adjustLineNb(lineno)];
}

void HistoryScrollBuffer::getCells(int lineno, int colno, int count, ca res[])
{
  if (!count) return;

  assert (lineno < (int) m_maxNbLines);

  lineno = adjustLineNb(lineno);
  
  histline *l = m_histBuffer[lineno];

  if (!l) {
    memset(res, 0, count * sizeof(ca));
    return;
  }

  assert(colno <= (int) l->size() - count);
    
  memcpy(res, l->data() + colno, count * sizeof(ca));
}

void HistoryScrollBuffer::setMaxNbLines(unsigned int nbLines)
{
  QPtrVector<histline> newHistBuffer(nbLines);
  QBitArray newWrappedLine(nbLines);
  
  size_t preservedLines = (nbLines > m_nbLines ? m_nbLines : nbLines); //min

  // delete any lines that will be lost
  size_t lineOld;
  for(lineOld = 0; lineOld < m_nbLines - preservedLines; ++lineOld) {
     delete m_histBuffer[adjustLineNb(lineOld)];
  }

  // copy the lines to new arrays
  size_t indexNew = 0;
  while(indexNew < preservedLines) {
     newHistBuffer.insert(indexNew, m_histBuffer[adjustLineNb(lineOld)]);
     newWrappedLine.setBit(indexNew, m_wrappedLine[adjustLineNb(lineOld)]);
     ++lineOld; 
     ++indexNew;
  }
  m_arrayIndex = preservedLines - 1;
  
  m_histBuffer = newHistBuffer;
  m_wrappedLine = newWrappedLine;

  m_maxNbLines = nbLines;
  if (m_nbLines > m_maxNbLines)
     m_nbLines = m_maxNbLines;

  delete m_histType;
  m_histType = new HistoryTypeBuffer(nbLines);
}

int HistoryScrollBuffer::adjustLineNb(int lineno)
{
   // lineno = 0:               oldest line
   // lineno = getLines() - 1:  newest line

   return (m_arrayIndex + lineno - (m_nbLines - 1) + m_maxNbLines) % m_maxNbLines;
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

void HistoryScrollNone::getCells(int, int, int, ca [])
{
}

void HistoryScrollNone::addCells(ca [], int)
{
}

void HistoryScrollNone::addLine(bool)
{
}

// History Scroll BlockArray //////////////////////////////////////

HistoryScrollBlockArray::HistoryScrollBlockArray(size_t size)
  : HistoryScroll(new HistoryTypeBlockArray(size))
{
  m_lineLengths.setAutoDelete(true);
  m_blockArray.setHistorySize(size); // nb. of lines.
}

HistoryScrollBlockArray::~HistoryScrollBlockArray()
{
}

int  HistoryScrollBlockArray::getLines()
{
//   kdDebug(1211) << "HistoryScrollBlockArray::getLines() : "
//             << m_lineLengths.count() << endl;

  return m_lineLengths.count();
}

int  HistoryScrollBlockArray::getLineLen(int lineno)
{
  size_t *pLen = m_lineLengths[lineno];
  size_t res = pLen ? *pLen : 0;

  return res;
}

bool HistoryScrollBlockArray::isWrappedLine(int /*lineno*/)
{
  return false;
}

void HistoryScrollBlockArray::getCells(int lineno, int colno,
                                       int count, ca res[])
{
  if (!count) return;

  const Block *b = m_blockArray.at(lineno);

  if (!b) {
    memset(res, 0, count * sizeof(ca)); // still better than random data
    return;
  }

  assert(((colno + count) * sizeof(ca)) < ENTRIES);
  memcpy(res, b->data + (colno * sizeof(ca)), count * sizeof(ca));
}

void HistoryScrollBlockArray::addCells(ca a[], int count)
{
  Block *b = m_blockArray.lastBlock();
  
  if (!b) return;

  // put cells in block's data
  assert((count * sizeof(ca)) < ENTRIES);

  memset(b->data, 0, ENTRIES);

  memcpy(b->data, a, count * sizeof(ca));
  b->size = count * sizeof(ca);

  size_t res = m_blockArray.newBlock();
  assert (res > 0);
  Q_UNUSED( res );

  // store line length
  size_t *pLen = new size_t;
  *pLen = count;
  
  m_lineLengths.replace(m_blockArray.getCurrent(), pLen);
}

void HistoryScrollBlockArray::addLine(bool)
{
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

bool HistoryTypeNone::isOn() const
{
  return false;
}

HistoryScroll* HistoryTypeNone::getScroll(HistoryScroll *old) const
{
  delete old;
  return new HistoryScrollNone();
}

unsigned int HistoryTypeNone::getSize() const
{
  return 0;
}

//////////////////////////////

HistoryTypeBlockArray::HistoryTypeBlockArray(size_t size)
  : m_size(size)
{
}

bool HistoryTypeBlockArray::isOn() const
{
  return true;
}

unsigned int HistoryTypeBlockArray::getSize() const
{
  return m_size;
}

HistoryScroll* HistoryTypeBlockArray::getScroll(HistoryScroll *old) const
{
  delete old;
  return new HistoryScrollBlockArray(m_size);
}


//////////////////////////////

HistoryTypeBuffer::HistoryTypeBuffer(unsigned int nbLines)
  : m_nbLines(nbLines)
{
}

bool HistoryTypeBuffer::isOn() const
{
  return true;
}

unsigned int HistoryTypeBuffer::getSize() const
{
  return m_nbLines;
}

HistoryScroll* HistoryTypeBuffer::getScroll(HistoryScroll *old) const
{
  if (old)
  {
    HistoryScrollBuffer *oldBuffer = dynamic_cast<HistoryScrollBuffer*>(old);
    if (oldBuffer)
    {
       oldBuffer->setMaxNbLines(m_nbLines);
       return oldBuffer;
    }

    HistoryScroll *newScroll = new HistoryScrollBuffer(m_nbLines);
    int lines = old->getLines();
    int startLine = 0;
    if (lines > (int) m_nbLines)
       startLine = lines - m_nbLines;

    ca line[LINE_SIZE];
    for(int i = startLine; i < lines; i++)
    {
       int size = old->getLineLen(i);
       if (size > LINE_SIZE)
       {
          ca *tmp_line = new ca[size];
          old->getCells(i, 0, size, tmp_line);
          newScroll->addCells(tmp_line, size);
          newScroll->addLine(old->isWrappedLine(i));
          delete tmp_line;
       }
       else
       {
          old->getCells(i, 0, size, line);
          newScroll->addCells(line, size);
          newScroll->addLine(old->isWrappedLine(i));
       }
    }
    delete old;
    return newScroll;
  }
  return new HistoryScrollBuffer(m_nbLines);
}

//////////////////////////////

HistoryTypeFile::HistoryTypeFile(const QString& fileName)
  : m_fileName(fileName)
{
}

bool HistoryTypeFile::isOn() const
{
  return true;
}

const QString& HistoryTypeFile::getFileName() const
{
  return m_fileName;
}

HistoryScroll* HistoryTypeFile::getScroll(HistoryScroll *old) const
{
  if (dynamic_cast<HistoryFile *>(old)) 
     return old; // Unchanged.

  HistoryScroll *newScroll = new HistoryScrollFile(m_fileName);

  ca line[LINE_SIZE];
  int lines = old->getLines();
  for(int i = 0; i < lines; i++)
  {
     int size = old->getLineLen(i);
     if (size > LINE_SIZE)
     {
        ca *tmp_line = new ca[size];
        old->getCells(i, 0, size, tmp_line);
        newScroll->addCells(tmp_line, size);
        newScroll->addLine(old->isWrappedLine(i));
        delete tmp_line;
     }
     else
     {
        old->getCells(i, 0, size, line);
        newScroll->addCells(line, size);
        newScroll->addLine(old->isWrappedLine(i));
     }
  }

  delete old;
  return newScroll; 
}

unsigned int HistoryTypeFile::getSize() const
{
  return 0;
}

/* -------------------------------------------------------------------------- */
/*                                                                            */
/* [TEHistory.C]                   History Buffer                             */
/*                                                                            */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>            */
/*                                                                            */
/* This file is part of Konsole - an X terminal for KDE                       */
/*                                                                            */
/* -------------------------------------------------------------------------- */

#include "TEHistory.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <kdebug.h>


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

FIXME: There is noticable decrease in speed, also. Perhaps,
       there whole feature needs to be revisited therefore.
       Disadvantage of a more elaborated, say block-oriented
       scheme with wrap around would be it's complexity.
*/

//FIXME: tempory replacement for tmpfile
//       this is here one for debugging purpose.


// History Buffer ///////////////////////////////////////////

/*
   A Row(X) data type which allows adding elements to the end.
*/

HistoryBuffer::HistoryBuffer()
{
    length = 0;
}

HistoryBuffer::~HistoryBuffer()
{
    setScroll(false);
}

void HistoryBuffer::setScroll(bool on)
{
    kdDebug() << "setScroll " << on << endl;

    if (on == hasScroll()) return;

    if (on)
    {
        array.setSize(500000);
        assert(array.lastBlock());
    }
    else
    {
        array.setSize(0);
    }
}

bool HistoryBuffer::hasScroll()
{
    return (array.lastBlock() != 0);
}

void HistoryBuffer::add(const unsigned char* bytes, int len)
{
    assert(hasScroll());
    int rest = ENTRIES - array.lastBlock()->size;
    while (rest < len) {
        memcpy(array.lastBlock()->data + array.lastBlock()->size, bytes, rest);
        array.lastBlock()->size += rest;
        array.newBlock();
        if (!array.lastBlock()) { // something failed
            setScroll(false);
            return;
        }
        len -= rest;
        bytes += rest;
        rest = ENTRIES - array.lastBlock()->size;
    }
    memcpy(array.lastBlock()->data + array.lastBlock()->size , bytes, len);
    array.lastBlock()->size += len;
}

void HistoryBuffer::get(unsigned char* bytes, int len, int loc)
{
    assert(hasScroll());
    if (len < 0 || loc < 0 || size_t(loc + len) > this->len()) {
        fprintf(stderr,"getHist(...,%d,%d): invalid args.\n",len,loc);
        return;
    }
    size_t firstblock = loc / ENTRIES;
    size_t offset = loc - (firstblock * ENTRIES);
    int rest = ENTRIES - offset;
    if (rest > len)
        rest = len;
    if (!array.at(firstblock))
        return;

    memcpy(bytes, array.at(firstblock)->data + offset, rest);
    len -= rest;
    bytes += rest;
    firstblock++;
    while (len) {
         if (int(ENTRIES) > len)
             rest = len;
         else
             rest = ENTRIES;
         memcpy(bytes, array.at(firstblock)->data, rest);
         bytes += rest;
         len -= rest;
         firstblock++;
    }
}

bool HistoryBuffer::has(int loc) const
{
    return array.has(loc / ENTRIES);
}

size_t HistoryBuffer::len() const
{
    if (array.lastBlock())
        return array.len() * ENTRIES + array.lastBlock()->size;
    else
        return 0;
}

// History Scroll //////////////////////////////////////

/*
   The history scroll makes a Row(Row(Cell)) from
   two history buffers. The index buffer contains
   start of line positions which refere to the cells
   buffer.

   Note that index[0] addresses the second line
   (line #1), while the first line (line #0) starts
   at 0 in cells.
*/

HistoryScroll::HistoryScroll()
{
}

HistoryScroll::~HistoryScroll()
{
}

void HistoryScroll::setScroll(bool on)
{
  index.setScroll(on);
  cells.setScroll(on);
}

bool HistoryScroll::hasScroll()
{
    return index.hasScroll() && cells.hasScroll();
}

int HistoryScroll::getLines()
{
    if (!hasScroll()) return 0;
    int indexlen = index.len() / sizeof(int);
    if (indexlen <= 0)
        return 0;
    return indexlen;
    /*
    int orig = indexlen;
  int res = 0;
  index.get((unsigned char*)&res,sizeof(int),(indexlen-1)*sizeof(int));
  kdDebug() << "getLines1 " << res << " " << indexlen << endl;
  while (!cells.has(res)) {
      indexlen--;
      if (!indexlen)
          break;
      index.get((unsigned char*)&res,sizeof(int),(indexlen-1)*sizeof(int));
  }
  kdDebug() << "getLines " << orig << " " << indexlen << endl;
  return indexlen;
*/
}

int HistoryScroll::getLineLen(int lineno)
{
  if (!hasScroll()) return 0;
  return (startOfLine(lineno+1) - startOfLine(lineno)) / sizeof(ca);
}

int HistoryScroll::startOfLine(int lineno)
{
  if (lineno <= 0) return 0;
  if (!hasScroll()) return 0;
  if (lineno <= getLines())
  {
      int res = 0;
      index.get((unsigned char*)&res,sizeof(int),(lineno-1)*sizeof(int));
//      kdDebug() << "startOfLine " << cells.len() << " " << res << endl;
      return res;
  }

  return cells.len();
}

void HistoryScroll::getCells(int lineno, int colno, int count, ca res[])
{
  assert(hasScroll());
  cells.get((unsigned char*)res,count*sizeof(ca),startOfLine(lineno)+colno*sizeof(ca));
}

void HistoryScroll::addCells(ca text[], int count)
{
  if (!hasScroll()) return;
  cells.add((unsigned char*)text,count*sizeof(ca));
}

void HistoryScroll::addLine()
{
  if (!hasScroll()) return;
  int locn = cells.len();
  index.add((unsigned char*)&locn, sizeof(int));
}

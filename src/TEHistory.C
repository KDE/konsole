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

#define HERE printf("%s(%d): here\n",__FILE__,__LINE__)

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

//#define tmpfile xTmpFile

FILE* xTmpFile()
{
  static int fid = 0;
  char fname[80];
  sprintf(fname,"TmpFile.%d",fid++);
  return fopen(fname,"w");
}


// History Buffer ///////////////////////////////////////////

/*
   A Row(X) data type which allows adding elements to the end.
*/

HistoryBuffer::HistoryBuffer()
{
  ion    = -1;
  length = 0;
}

HistoryBuffer::~HistoryBuffer()
{
  setScroll(FALSE);
}

void HistoryBuffer::setScroll(bool on)
{
  if (on == hasScroll()) return;

  if (on)
  {
    assert( ion < 0 );
    assert( length == 0);
    FILE* tmp = tmpfile(); if (!tmp) { perror("konsole: cannot open temp file.\n"); return; }
    ion = dup(fileno(tmp)); if (ion<0) perror("konsole: cannot dup temp file.\n");
    fclose(tmp);
  }
  else
  {
    assert( ion >= 0 );
    close(ion);
    ion    = -1;
    length = 0;
  }
}

bool HistoryBuffer::hasScroll()
{
  return ion >= 0;
}

void HistoryBuffer::add(const unsigned char* bytes, int len)
{ int rc;
  assert(hasScroll());
  rc = lseek(ion,length,SEEK_SET); if (rc < 0) { perror("HistoryBuffer::add.seek"); setScroll(FALSE); return; }
  rc = write(ion,bytes,len);       if (rc < 0) { perror("HistoryBuffer::add.write"); setScroll(FALSE); return; }
  length += rc;
}

void HistoryBuffer::get(unsigned char* bytes, int len, int loc)
{ int rc;
  assert(hasScroll());
  if (loc < 0 || len < 0 || loc + len > length)
    fprintf(stderr,"getHist(...,%d,%d): invalid args.\n",len,loc);
  rc = lseek(ion,loc,SEEK_SET); if (rc < 0) { perror("HistoryBuffer::get.seek"); setScroll(FALSE); return; }
  rc = read(ion,bytes,len);     if (rc < 0) { perror("HistoryBuffer::get.read"); setScroll(FALSE); return; }
}

int HistoryBuffer::len()
{
  return length;
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
  return index.len() / sizeof(int);
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
  { int res;
    index.get((unsigned char*)&res,sizeof(int),(lineno-1)*sizeof(int));
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
  index.add((unsigned char*)&locn,sizeof(int));
}

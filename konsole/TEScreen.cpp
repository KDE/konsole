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

/*! \class TEScreen

    \brief The image manipulated by the emulation.

    This class implements the operations of the terminal emulation framework.
    It is a complete passive device, driven by the emulation decoder
    (TEmuVT102). By this it forms in fact an ADT, that defines operations
    on a rectangular image.

    It does neither know how to display its image nor about escape sequences.
    It is further independent of the underlying toolkit. By this, one can even
    use this module for an ordinary text surface.

    Since the operations are called by a specific emulation decoder, one may
    collect their different operations here.

    The state manipulated by the operations is mainly kept in `image', though
    it is a little more complex bejond this. See the header file of the class.

    \sa TEWidget \sa VT102Emulation
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <kdebug.h>

#include <assert.h>
#include <string.h>
#include <ctype.h>

#include "konsole_wcwidth.h"
#include "TEScreen.h"

//FIXME: this is emulation specific. Use false for xterm, true for ANSI.
//FIXME: see if we can get this from terminfo.
#define BS_CLEARS false

#ifndef loc
#define loc(X,Y) ((Y)*columns+(X))
#endif

//#define REVERSE_WRAPPED_LINES  // for wrapped line debug

/*! creates a `TEScreen' of `lines' lines and `columns' columns.
*/

TEScreen::TEScreen(int l, int c)
  : lines(l),
    columns(c),
    image(new ca[(lines+1)*columns]),
    histCursor(0),
    hist(new HistoryScrollNone()),
    cuX(0), cuY(0),
    cu_fg(cacol()), cu_bg(cacol()), cu_re(0),
    tmargin(0), bmargin(0),
    tabstops(0),
    sel_begin(0), sel_TL(0), sel_BR(0),
    sel_busy(false),
    columnmode(false),
    ef_fg(cacol()), ef_bg(cacol()), ef_re(0),
    sa_cuX(0), sa_cuY(0),
    sa_cu_re(0), sa_cu_fg(cacol()), sa_cu_bg(cacol()),
    lastPos(-1)
{
  /*
    this->lines   = lines;
    this->columns = columns;

    // we add +1 here as under some weired circumstances konsole crashes
    // reading out of bound. As a crash is worse, we afford the minimum
    // of added memory
    image      = (ca*) malloc((lines+1)*columns*sizeof(ca));
    tabstops   = NULL; initTabStops();
    cuX = cuY = sa_cu_re = cu_re = sa_cu_fg = cu_fg = sa_cu_bg = cu_bg = 0;

    histCursor = 0;
  */
  line_wrapped.resize(lines+1);
  initTabStops();
  clearSelection();
  reset();
}

/*! Destructor
*/

TEScreen::~TEScreen()
{
  delete[] image;
  delete[] tabstops;
  delete hist;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* Normalized                    Screen Operations                           */
/*                                                                           */
/* ------------------------------------------------------------------------- */

// Cursor Setting --------------------------------------------------------------

/*! \section Cursor

    The `cursor' is a location within the screen that is implicitely used in
    many operations. The operations within this section allow to manipulate
    the cursor explicitly and to obtain it's value.

    The position of the cursor is guarantied to be between (including) 0 and
    `columns-1' and `lines-1'.
*/

/*!
    Move the cursor up.

    The cursor will not be moved beyond the top margin.
*/

void TEScreen::cursorUp(int n)
//=CUU
{
  if (n == 0) n = 1; // Default
  int stop = cuY < tmargin ? 0 : tmargin;
  cuX = QMIN(columns-1,cuX); // nowrap!
  cuY = QMAX(stop,cuY-n);
}

/*!
    Move the cursor down.

    The cursor will not be moved beyond the bottom margin.
*/

void TEScreen::cursorDown(int n)
//=CUD
{
  if (n == 0) n = 1; // Default
  int stop = cuY > bmargin ? lines-1 : bmargin;
  cuX = QMIN(columns-1,cuX); // nowrap!
  cuY = QMIN(stop,cuY+n);
}

/*!
    Move the cursor left.

    The cursor will not move beyond the first column.
*/

void TEScreen::cursorLeft(int n)
//=CUB
{
  if (n == 0) n = 1; // Default
  cuX = QMIN(columns-1,cuX); // nowrap!
  cuX = QMAX(0,cuX-n);
}

/*!
    Move the cursor left.

    The cursor will not move beyond the rightmost column.
*/

void TEScreen::cursorRight(int n)
//=CUF
{
  if (n == 0) n = 1; // Default
  cuX = QMIN(columns-1,cuX+n);
}

/*!
    Set top and bottom margin.
*/

void TEScreen::setMargins(int top, int bot)
//=STBM
{
  if (top == 0) top = 1;      // Default
  if (bot == 0) bot = lines;  // Default
  top = top - 1;              // Adjust to internal lineno
  bot = bot - 1;              // Adjust to internal lineno
  if ( !( 0 <= top && top < bot && bot < lines ) )
  { kdDebug()<<" setRegion("<<top<<","<<bot<<") : bad range."<<endl;
    return;                   // Default error action: ignore
  }
  tmargin = top;
  bmargin = bot;
  cuX = 0;
  cuY = getMode(MODE_Origin) ? top : 0;
}

/*!
    Move the cursor down one line.

    If cursor is on bottom margin, the region between the
    actual top and bottom margin is scrolled up instead.
*/

void TEScreen::index()
//=IND
{
  if (cuY == bmargin)
  {
    scrollUp(1);
  }
  else if (cuY < lines-1)
    cuY += 1;
}

/*!
    Move the cursor up one line.

    If cursor is on the top margin, the region between the
    actual top and bottom margin is scrolled down instead.
*/

void TEScreen::reverseIndex()
//=RI
{
  if (cuY == tmargin)
     scrollDown(tmargin,1);
  else if (cuY > 0)
    cuY -= 1;
}

/*!
    Move the cursor to the begin of the next line.

    If cursor is on bottom margin, the region between the
    actual top and bottom margin is scrolled up.
*/

void TEScreen::NextLine()
//=NEL
{
  Return(); index();
}

// Line Editing ----------------------------------------------------------------

/*! \section inserting / deleting characters
*/

/*! erase `n' characters starting from (including) the cursor position.

    The line is filled in from the right with spaces.
*/

void TEScreen::eraseChars(int n)
{
  if (n == 0) n = 1; // Default
  int p = QMAX(0,QMIN(cuX+n-1,columns-1));
  clearImage(loc(cuX,cuY),loc(p,cuY),' ');
}

/*! delete `n' characters starting from (including) the cursor position.

    The line is filled in from the right with spaces.
*/

void TEScreen::deleteChars(int n)
{
  if (n == 0) n = 1; // Default
  if (n > columns) n = columns - 1;
  int p = QMAX(0,QMIN(cuX+n,columns-1));
  moveImage(loc(cuX,cuY),loc(p,cuY),loc(columns-1,cuY));
  clearImage(loc(columns-n,cuY),loc(columns-1,cuY),' ');
}

/*! insert `n' spaces at the cursor position.

    The cursor is not moved by the operation.
*/

void TEScreen::insertChars(int n)
{
  if (n == 0) n = 1; // Default
  int p = QMAX(0,QMIN(columns-1-n,columns-1));
  int q = QMAX(0,QMIN(cuX+n,columns-1));
  moveImage(loc(q,cuY),loc(cuX,cuY),loc(p,cuY));
  clearImage(loc(cuX,cuY),loc(q-1,cuY),' ');
}

/*! delete `n' lines starting from (including) the cursor position.

    The cursor is not moved by the operation.
*/

void TEScreen::deleteLines(int n)
{
  if (n == 0) n = 1; // Default
  scrollUp(cuY,n);
}

/*! insert `n' lines at the cursor position.

    The cursor is not moved by the operation.
*/

void TEScreen::insertLines(int n)
{
  if (n == 0) n = 1; // Default
  scrollDown(cuY,n);
}

// Mode Operations -----------------------------------------------------------

/*! Set a specific mode. */

void TEScreen::setMode(int m)
{
  currParm.mode[m] = true;
  switch(m)
  {
    case MODE_Origin : cuX = 0; cuY = tmargin; break; //FIXME: home
  }
}

/*! Reset a specific mode. */

void TEScreen::resetMode(int m)
{
  currParm.mode[m] = false;
  switch(m)
  {
    case MODE_Origin : cuX = 0; cuY = 0; break; //FIXME: home
  }
}

/*! Save a specific mode. */

void TEScreen::saveMode(int m)
{
  saveParm.mode[m] = currParm.mode[m];
}

/*! Restore a specific mode. */

void TEScreen::restoreMode(int m)
{
  currParm.mode[m] = saveParm.mode[m];
}

//NOTE: this is a helper function
/*! Return the setting  a specific mode. */
bool TEScreen::getMode(int m)
{
  return currParm.mode[m];
}

/*! Save the cursor position and the rendition attribute settings. */

void TEScreen::saveCursor()
{
  sa_cuX     = cuX;
  sa_cuY     = cuY;
  sa_cu_re   = cu_re;
  sa_cu_fg   = cu_fg;
  sa_cu_bg   = cu_bg;
}

/*! Restore the cursor position and the rendition attribute settings. */

void TEScreen::restoreCursor()
{
  cuX     = QMIN(sa_cuX,columns-1);
  cuY     = QMIN(sa_cuY,lines-1);
  cu_re   = sa_cu_re;
  cu_fg   = sa_cu_fg;
  cu_bg   = sa_cu_bg;
  effectiveRendition();
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                             Screen Operations                             */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/*! Assing a new size to the screen.

    The topmost left position is maintained, while lower lines
    or right hand side columns might be removed or filled with
    spaces to fit the new size.

    The region setting is reset to the whole screen and the
    tab positions reinitialized.
*/

void TEScreen::resizeImage(int new_lines, int new_columns)
{
  if ((new_lines==lines) && (new_columns==columns)) return;

  if (cuY > new_lines-1)
  { // attempt to preserve focus and lines
    bmargin = lines-1; //FIXME: margin lost
    for (int i = 0; i < cuY-(new_lines-1); i++)
    {
      addHistLine(); scrollUp(0,1);
    }
  }

  // make new image

  ca* newimg = new ca[(new_lines+1)*new_columns];
  QBitArray newwrapped(new_lines+1);
  clearSelection();

  // clear new image
  for (int y = 0; y < new_lines; y++) {
    for (int x = 0; x < new_columns; x++)
    {
      newimg[y*new_columns+x].c = ' ';
      newimg[y*new_columns+x].f = cacol(CO_DFT,DEFAULT_FORE_COLOR);
      newimg[y*new_columns+x].b = cacol(CO_DFT,DEFAULT_BACK_COLOR);
      newimg[y*new_columns+x].r = DEFAULT_RENDITION;
    }
    newwrapped[y]=false;
  }
  int cpy_lines   = QMIN(new_lines,  lines);
  int cpy_columns = QMIN(new_columns,columns);
  // copy to new image
  for (int y = 0; y < cpy_lines; y++) {
    for (int x = 0; x < cpy_columns; x++)
    {
      newimg[y*new_columns+x].c = image[loc(x,y)].c;
      newimg[y*new_columns+x].f = image[loc(x,y)].f;
      newimg[y*new_columns+x].b = image[loc(x,y)].b;
      newimg[y*new_columns+x].r = image[loc(x,y)].r;
    }
    newwrapped[y]=line_wrapped[y];
  }
  delete[] image;
  image = newimg;
  line_wrapped = newwrapped;
  lines = new_lines;
  columns = new_columns;
  cuX = QMIN(cuX,columns-1);
  cuY = QMIN(cuY,lines-1);

  // FIXME: try to keep values, evtl.
  tmargin=0;
  bmargin=lines-1;
  initTabStops();
  clearSelection();
}

/*
   Clarifying rendition here and in TEWidget.

   currently, TEWidget's color table is
     0       1       2 .. 9    10 .. 17
     dft_fg, dft_bg, dim 0..7, intensive 0..7

   cu_fg, cu_bg contain values 0..8;
   - 0    = default color
   - 1..8 = ansi specified color

   re_fg, re_bg contain values 0..17
   due to the TEWidget's color table

   rendition attributes are

      attr           widget screen
      -------------- ------ ------
      RE_UNDERLINE     XX     XX    affects foreground only
      RE_BLINK         XX     XX    affects foreground only
      RE_BOLD          XX     XX    affects foreground only
      RE_REVERSE       --     XX
      RE_TRANSPARENT   XX     --    affects background only
      RE_INTENSIVE     XX     --    affects foreground only

   Note that RE_BOLD is used in both widget
   and screen rendition. Since xterm/vt102
   is to poor to distinguish between bold
   (which is a font attribute) and intensive
   (which is a color attribute), we translate
   this and RE_BOLD in falls eventually appart
   into RE_BOLD and RE_INTENSIVE.
*/

void TEScreen::reverseRendition(ca* p)
{ cacol f = p->f; cacol b = p->b;
  p->f = b; p->b = f; //p->r &= ~RE_TRANSPARENT;
}

void TEScreen::effectiveRendition()
// calculate rendition
{
  ef_re = cu_re & (RE_UNDERLINE | RE_BLINK);
  if (cu_re & RE_REVERSE)
  {
    ef_fg = cu_bg;
    ef_bg = cu_fg;
  }
  else
  {
    ef_fg = cu_fg;
    ef_bg = cu_bg;
  }
  if (cu_re & RE_BOLD)
    ef_fg.toggleIntensive();
}

/*!
    returns the image.

    Get the size of the image by \sa getLines and \sa getColumns.

    NOTE that the image returned by this function must later be
    freed.

*/

ca* TEScreen::getCookedImage()
{
/*kdDebug() << "sel_begin=" << sel_begin << "(" << sel_begin/columns << "," << sel_begin%columns << ")"
  << "  sel_TL=" << sel_TL << "(" << sel_TL/columns << "," << sel_TL%columns << ")"
  << "  sel_BR=" << sel_BR << "(" << sel_BR/columns << "," << sel_BR%columns << ")"
  << "  histcursor=" << histCursor << endl;*/

  int x,y;
  ca* merged = (ca*)malloc((lines*columns+1)*sizeof(ca));
  ca dft(' ',cacol(CO_DFT,DEFAULT_FORE_COLOR),cacol(CO_DFT,DEFAULT_BACK_COLOR),DEFAULT_RENDITION);
  merged[lines*columns] = dft;

//  kdDebug(1211) << "InGetCookedImage" << endl;
  for (y = 0; (y < lines) && (y < (hist->getLines()-histCursor)); y++)
  {
    int len = QMIN(columns,hist->getLineLen(y+histCursor));
    int yp  = y*columns;

//    kdDebug(1211) << "InGetCookedImage - In first For.  Y =" << y << "histCursor = " << histCursor << endl;
    hist->getCells(y+histCursor,0,len,merged+yp);
    for (x = len; x < columns; x++) merged[yp+x] = dft;
    if (sel_begin !=-1)
    for (x = 0; x < columns; x++)
      {
#ifdef REVERSE_WRAPPED_LINES
        if (hist->isWrappedLine(y+histCursor))
          reverseRendition(&merged[p]);
#endif
        if (testIsSelected(x,y)) {
          int p=x + yp;
          reverseRendition(&merged[p]); // for selection
    }
  }
  }
  if (lines >= hist->getLines()-histCursor)
  {
    for (y = (hist->getLines()-histCursor); y < lines ; y++)
    {
       int yp  = y*columns;
       int yr =  (y-hist->getLines()+histCursor)*columns;
//       kdDebug(1211) << "InGetCookedImage - In second For.  Y =" << y << endl;
       for (x = 0; x < columns; x++)
       { int p = x + yp; int r = x + yr;
         merged[p] = image[r];
#ifdef REVERSE_WRAPPED_LINES
         if (line_wrapped[y- hist->getLines() +histCursor])
           reverseRendition(&merged[p]);
#endif
         if (sel_begin != -1 && testIsSelected(x,y))
           reverseRendition(&merged[p]); // for selection
       }

    }
  }
  // evtl. inverse display
  if (getMode(MODE_Screen))
  {
    for (int i = 0; i < lines*columns; i++)
      reverseRendition(&merged[i]); // for reverse display
  }
//  if (getMode(MODE_Cursor) && (cuY+(hist->getLines()-histCursor) < lines)) // cursor visible

  int loc_ = loc(cuX, cuY+hist->getLines()-histCursor);
  if(getMode(MODE_Cursor) && loc_ < columns*lines)
    merged[loc(cuX,cuY+(hist->getLines()-histCursor))].r|=RE_CURSOR;
  return merged;
}

QBitArray TEScreen::getCookedLineWrapped()
{
  QBitArray result(lines);

  for (int y = 0; (y < lines) && (y < (hist->getLines()-histCursor)); y++)
    result[y]=hist->isWrappedLine(y+histCursor);

  if (lines >= hist->getLines()-histCursor)
    for (int y = (hist->getLines()-histCursor); y < lines ; y++)
      result[y]=line_wrapped[y- hist->getLines() +histCursor];

  return result;
}

/*!
*/

void TEScreen::reset()
{
    setMode(MODE_Wrap  ); saveMode(MODE_Wrap  );  // wrap at end of margin
  resetMode(MODE_Origin); saveMode(MODE_Origin);  // position refere to [1,1]
  resetMode(MODE_Insert); saveMode(MODE_Insert);  // overstroke
    setMode(MODE_Cursor);                         // cursor visible
  resetMode(MODE_Screen);                         // screen not inverse
  resetMode(MODE_NewLine);

  tmargin=0;
  bmargin=lines-1;

  setDefaultRendition();
  saveCursor();

  clear();
}

/*! Clear the entire screen and home the cursor.
*/

void TEScreen::clear()
{
  clearEntireScreen();
  home();
}

/*! Moves the cursor left one column.
*/

void TEScreen::BackSpace()
{
  cuX = QMAX(0,cuX-1);
  if (BS_CLEARS) image[loc(cuX,cuY)].c = ' ';
}

/*!
*/

void TEScreen::Tabulate(int n)
{
  // note that TAB is a format effector (does not write ' ');
  if (n == 0) n = 1;
  while((n > 0) && (cuX < columns-1))
  {
    cursorRight(1); while((cuX < columns-1) && !tabstops[cuX]) cursorRight(1);
    n--;
  }
}

void TEScreen::backTabulate(int n)
{
  // note that TAB is a format effector (does not write ' ');
  if (n == 0) n = 1;
  while((n > 0) && (cuX > 0))
  {
     cursorLeft(1); while((cuX > 0) && !tabstops[cuX]) cursorLeft(1);
     n--;
  }
}

void TEScreen::clearTabStops()
{
  for (int i = 0; i < columns; i++) tabstops[i] = false;
}

void TEScreen::changeTabStop(bool set)
{
  if (cuX >= columns) return;
  tabstops[cuX] = set;
}

void TEScreen::initTabStops()
{
  delete[] tabstops;
  tabstops = new bool[columns];

  // Arrg! The 1st tabstop has to be one longer than the other.
  // i.e. the kids start counting from 0 instead of 1.
  // Other programs might behave correctly. Be aware.
  for (int i = 0; i < columns; i++) tabstops[i] = (i%8 == 0 && i != 0);
}

/*!
   This behaves either as IND (Screen::Index) or as NEL (Screen::NextLine)
   depending on the NewLine Mode (LNM). This mode also
   affects the key sequence returned for newline ([CR]LF).
*/

void TEScreen::NewLine()
{
  if (getMode(MODE_NewLine)) Return();
  index();
}

/*! put `c' literally onto the screen at the current cursor position.

    VT100 uses the convention to produce an automatic newline (am)
    with the *first* character that would fall onto the next line (xenl).
*/

void TEScreen::checkSelection(int from, int to)
{
  if (sel_begin == -1) return;
  int scr_TL = loc(0, hist->getLines());
  //Clear entire selection if it overlaps region [from, to]
  if ( (sel_BR > (from+scr_TL) )&&(sel_TL < (to+scr_TL)) )
  {
    clearSelection();
  }
}

void TEScreen::ShowCharacter(unsigned short c)
{
  // Note that VT100 does wrapping BEFORE putting the character.
  // This has impact on the assumption of valid cursor positions.
  // We indicate the fact that a newline has to be triggered by
  // putting the cursor one right to the last column of the screen.

  int w = konsole_wcwidth(c);

  if (w <= 0)
     return;

  if (cuX+w > columns) {
    if (getMode(MODE_Wrap)) {
      line_wrapped[cuY]=true;
      NextLine();
    }
    else
      cuX = columns-w;
  }

  if (getMode(MODE_Insert)) insertChars(w);

  int i = loc(cuX,cuY);

  checkSelection(i, i); // check if selection is still valid.

  image[i].c = c;
  image[i].f = ef_fg;
  image[i].b = ef_bg;
  image[i].r = ef_re;
  
  lastPos = i;

  cuX += w--;

  while(w)
  {
     i++;
     image[i].c = 0;
     image[i].f = ef_fg;
     image[i].b = ef_bg;
     image[i].r = ef_re;
     w--;
  }
}

void TEScreen::compose(QString compose)
{
  if (lastPos == -1)
     return;
     
  QChar c(image[lastPos].c);
  compose.prepend(c);
  compose.compose();
  image[lastPos].c = compose[0].unicode();
}

// Region commands -------------------------------------------------------------

void TEScreen::scrollUp(int n)
{
   if (n == 0) n = 1; // Default
   if (tmargin == 0) addHistLine(); // hist.history
   scrollUp(tmargin, n);
}

/*! scroll up `n' lines within current region.
    The `n' new lines are cleared.
    \sa setRegion \sa scrollDown
*/

void TEScreen::scrollUp(int from, int n)
{
  if (n <= 0 || from + n > bmargin) return;
  //FIXME: make sure `tmargin', `bmargin', `from', `n' is in bounds.
  moveImage(loc(0,from),loc(0,from+n),loc(columns-1,bmargin));
  clearImage(loc(0,bmargin-n+1),loc(columns-1,bmargin),' ');
}

void TEScreen::scrollDown(int n)
{
   if (n == 0) n = 1; // Default
   scrollDown(tmargin, n);
}

/*! scroll down `n' lines within current region.
    The `n' new lines are cleared.
    \sa setRegion \sa scrollUp
*/

void TEScreen::scrollDown(int from, int n)
{
//FIXME: make sure `tmargin', `bmargin', `from', `n' is in bounds.
  if (n <= 0) return;
  if (from > bmargin) return;
  if (from + n > bmargin) n = bmargin - from;
  moveImage(loc(0,from+n),loc(0,from),loc(columns-1,bmargin-n));
  clearImage(loc(0,from),loc(columns-1,from+n-1),' ');
}

/*! position the cursor to a specific line and column. */
void TEScreen::setCursorYX(int y, int x)
{
  setCursorY(y); setCursorX(x);
}

/*! Set the cursor to x-th line. */

void TEScreen::setCursorX(int x)
{
  if (x == 0) x = 1; // Default
  x -= 1; // Adjust
  cuX = QMAX(0,QMIN(columns-1, x));
}

/*! Set the cursor to y-th line. */

void TEScreen::setCursorY(int y)
{
  if (y == 0) y = 1; // Default
  y -= 1; // Adjust
  cuY = QMAX(0,QMIN(lines  -1, y + (getMode(MODE_Origin) ? tmargin : 0) ));
}

/*! set cursor to the `left upper' corner of the screen (1,1).
*/

void TEScreen::home()
{
  cuX = 0;
  cuY = 0;
}

/*! set cursor to the begin of the current line.
*/

void TEScreen::Return()
{
  cuX = 0;
}

/*! returns the current cursor columns.
*/

int TEScreen::getCursorX()
{
  return cuX;
}

/*! returns the current cursor line.
*/

int TEScreen::getCursorY()
{
  return cuY;
}

// Erasing ---------------------------------------------------------------------

/*! \section Erasing

    This group of operations erase parts of the screen contents by filling
    it with spaces colored due to the current rendition settings.

    Althought the cursor position is involved in most of these operations,
    it is never modified by them.
*/

/*! fill screen between (including) `loca' and `loce' with spaces.

    This is an internal helper functions. The parameter types are internal
    addresses of within the screen image and make use of the way how the
    screen matrix is mapped to the image vector.
*/

void TEScreen::clearImage(int loca, int loce, char c)
{ int i;
  int scr_TL=loc(0,hist->getLines());
  //FIXME: check positions

  //Clear entire selection if it overlaps region to be moved...
  if ( (sel_BR > (loca+scr_TL) )&&(sel_TL < (loce+scr_TL)) )
  {
    clearSelection();
  }
  
  for (i = loca; i <= loce; i++)
  {
    // Use the current colors but the default rendition
    // Check with: echo -e '\033[41;33;07m\033[2Khello world\033[00m'
    image[i].c = c;
    image[i].f = cu_fg;
    image[i].b = cu_bg;
    image[i].r = DEFAULT_RENDITION;
  }

  for (i = loca/columns; i<=loce/columns; i++)
    line_wrapped[i]=false;
}

/*! move image between (including) `loca' and `loce' to 'dst'.

    This is an internal helper functions. The parameter types are internal
    addresses of within the screen image and make use of the way how the
    screen matrix is mapped to the image vector.
*/

void TEScreen::moveImage(int dst, int loca, int loce)
{
//FIXME: check positions
  if (loce < loca) {
    kdDebug(1211) << "WARNING!!! call to TEScreen:moveImage with loce < loca!" << endl;
    return;
  }
  //kdDebug(1211) << "Using memmove to scroll up" << endl;
  memmove(&image[dst],&image[loca],(loce-loca+1)*sizeof(ca));
  for (int i=0;i<=(loce-loca+1)/columns;i++)
    line_wrapped[(dst/columns)+i]=line_wrapped[(loca/columns)+i];
  if (lastPos != -1)
  {
     int diff = dst - loca; // Scroll by this amount
     lastPos += diff;
     if ((lastPos < 0) || (lastPos >= (lines*columns)))
        lastPos = -1;
  }
  if (sel_begin != -1)
  {
     // Adjust selection to follow scroll.
     bool beginIsTL = (sel_begin == sel_TL);
     int diff = dst - loca; // Scroll by this amount
     int scr_TL=loc(0,hist->getLines());
     int srca = loca+scr_TL; // Translate index from screen to global
     int srce = loce+scr_TL; // Translate index from screen to global
     int desta = srca+diff;
     int deste = srce+diff;

     if ((sel_TL >= srca) && (sel_TL <= srce))
        sel_TL += diff;
     else if ((sel_TL >= desta) && (sel_TL <= deste))
        sel_BR = -1; // Clear selection (see below)

     if ((sel_BR >= srca) && (sel_BR <= srce))
        sel_BR += diff;
     else if ((sel_BR >= desta) && (sel_BR <= deste))
        sel_BR = -1; // Clear selection (see below)

     if (sel_BR < 0)
     {
        clearSelection();
     }
     else
     {
        if (sel_TL < 0)
           sel_TL = 0;
     }

     if (beginIsTL)
        sel_begin = sel_TL;
     else
        sel_begin = sel_BR;
  }
}

/*! clear from (including) current cursor position to end of screen.
*/

void TEScreen::clearToEndOfScreen()
{
  clearImage(loc(cuX,cuY),loc(columns-1,lines-1),' ');
}

/*! clear from begin of screen to (including) current cursor position.
*/

void TEScreen::clearToBeginOfScreen()
{
  clearImage(loc(0,0),loc(cuX,cuY),' ');
}

/*! clear the entire screen.
*/

void TEScreen::clearEntireScreen()
{
  // Add entire screen to history
  for (int i = 0; i < (lines-1); i++)
  {
    addHistLine(); scrollUp(0,1);
  }

  clearImage(loc(0,0),loc(columns-1,lines-1),' ');
}

/*! fill screen with 'E'
    This is to aid screen alignment
*/

void TEScreen::helpAlign()
{
  clearImage(loc(0,0),loc(columns-1,lines-1),'E');
}

/*! clear from (including) current cursor position to end of current cursor line.
*/

void TEScreen::clearToEndOfLine()
{
  clearImage(loc(cuX,cuY),loc(columns-1,cuY),' ');
}

/*! clear from begin of current cursor line to (including) current cursor position.
*/

void TEScreen::clearToBeginOfLine()
{
  clearImage(loc(0,cuY),loc(cuX,cuY),' ');
}

/*! clears entire current cursor line
*/

void TEScreen::clearEntireLine()
{
  clearImage(loc(0,cuY),loc(columns-1,cuY),' ');
}

// Rendition ------------------------------------------------------------------

/*!
    set rendition mode
*/

void TEScreen::setRendition(int re)
{
  cu_re |= re;
  effectiveRendition();
}

/*!
    reset rendition mode
*/

void TEScreen::resetRendition(int re)
{
  cu_re &= ~re;
  effectiveRendition();
}

/*!
*/

void TEScreen::setDefaultRendition()
{
  setForeColor(CO_DFT,DEFAULT_FORE_COLOR);
  setBackColor(CO_DFT,DEFAULT_BACK_COLOR);
  cu_re   = DEFAULT_RENDITION;
  effectiveRendition();
}

/*!
*/
void TEScreen::setForeColor(int space, int color)
{
  cu_fg = cacol(space, color);
  effectiveRendition();
}

/*!
*/
void TEScreen::setBackColor(int space, int color)
{
  cu_bg = cacol(space, color);
  effectiveRendition();
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                            Marking & Selection                            */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TEScreen::clearSelection()
{
  sel_BR = -1;
  sel_TL = -1;
  sel_begin = -1;
}

void TEScreen::setSelBeginXY(const int x, const int y, const bool mode)
{
//  kdDebug(1211) << "setSelBeginXY(" << x << "," << y << ")" << endl;
  sel_begin = loc(x,y+histCursor) ;

  /* FIXME, HACK to correct for x too far to the right... */
  if (x == columns) sel_begin--;

  sel_BR = sel_begin;
  sel_TL = sel_begin;
  columnmode = mode;
}

void TEScreen::setSelExtentXY(const int x, const int y)
{
//  kdDebug(1211) << "setSelExtentXY(" << x << "," << y << ")" << endl;
  if (sel_begin == -1) return;
  int l =  loc(x,y + histCursor);

  if (l < sel_begin)
  {
    sel_TL = l;
    sel_BR = sel_begin;
  }
  else
  {
    /* FIXME, HACK to correct for x too far to the right... */
    if (x == columns) l--;

    sel_TL = sel_begin;
    sel_BR = l;
  }
}

bool TEScreen::testIsSelected(const int x,const int y)
{
  if (columnmode) {
    int sel_Left,sel_Right;
    if ( sel_TL % columns < sel_BR % columns ) {
      sel_Left = sel_TL; sel_Right = sel_BR;
    } else {
      sel_Left = sel_BR; sel_Right = sel_TL;
    }
    return ( x >= sel_Left % columns ) && ( x <= sel_Right % columns ) &&
           ( y+histCursor >= sel_TL / columns ) && ( y+histCursor <= sel_BR / columns );
  }
  else {
  int pos = loc(x,y+histCursor);
  return ( pos >= sel_TL && pos <= sel_BR );
  }
}

static bool isSpace(UINT16 c)
{
  if ((c > 32) && (c < 127))
     return false;
  if ((c == 32) || (c == 0))
     return true;
  QChar qc(c);
  return qc.isSpace();
}

QString TEScreen::getSelText(bool preserve_line_breaks)
{
  QString result;
  QTextOStream stream(&result);
  getSelText(preserve_line_breaks, &stream);
  return result;
}


static QString makeString(int *m, int d, bool stripTrailingSpaces)
{
  QChar* qc = new QChar[d];

  int last_space = -1;
  int j = 0;

  for (int i = 0; i < d; i++, j++)
    {
      if (m[i] == ' ')
        {
          if (last_space == -1)
            last_space = j;
        }
      else
        {
          last_space = -1;
        }
      qc[j] = m[i];
    }

  if ((last_space != -1) && stripTrailingSpaces)
    {
      // Strip trailing spaces
      j = last_space;
    }

  QString res(qc, j);
  delete [] qc;
  return res;
}

void TEScreen::getSelText(bool preserve_line_breaks, QTextStream *stream)
{
  if (sel_begin == -1)
     return; // Selection got clear while selecting.

  int *m;			// buffer to fill.
  int s, d;			// source index, dest. index.
  int hist_BR = loc(0, hist->getLines());
  int hY = sel_TL / columns;
  int hX = sel_TL % columns;
  int eol;			// end of line

  s = sel_TL;			// tracks copy in source.

				// allocate buffer for maximum
				// possible size...
  d = (sel_BR - sel_TL) / columns + 1;
  m = new int[columns + 3];
  d = 0;

#define LINE_END	do { \
                          assert(d <= columns); \
                          *stream << makeString(m, d, true) << (preserve_line_breaks ? "\n" : " "); \
                          d = 0; \
                        } while(false)
#define LINE_WRAP	do { \
                          assert(d <= columns); \
                          *stream << makeString(m, d, false); \
                          d = 0; \
                        } while(false)
#define LINE_FLUSH	do { \
                          assert(d <= columns); \
                          *stream << makeString(m, d, false); \
                          d = 0; \
                        } while(false)

  if (columnmode) {
    bool newlineneeded=false;
    preserve_line_breaks = true; // Just in case

    int sel_Left, sel_Right;
    if ( sel_TL % columns < sel_BR % columns ) {
      sel_Left = sel_TL; sel_Right = sel_BR;
    } else {
      sel_Left = sel_BR; sel_Right = sel_TL;
    }

    while (s <= sel_BR) {
      if (s < hist_BR) {		// get lines from hist->history buffer.
          hX = sel_Left % columns;
	  eol = hist->getLineLen(hY);
          if (eol > columns)
              eol = columns;
	  if ((hY == (sel_BR / columns)) &&
              (eol > (sel_BR % columns)))
          {
              eol = sel_BR % columns + 1;
          }

	  while (hX < eol && hX <= sel_Right % columns)
          {
            Q_UINT16 c = hist->getCell(hY, hX++).c;
            if (c)
              m[d++] = c;
            s++;
          }
          LINE_END;

          hY++;
          s = hY * columns;
      }
      else {				// or from screen image.
        if (testIsSelected((s - hist_BR) % columns, (s - hist_BR) / columns)) {
          Q_UINT16 c = image[s++ - hist_BR].c;
          if (c) {
            m[d++] = c;
            newlineneeded = true;
	  }
	  if (((s - hist_BR) % columns == 0) && newlineneeded)
	  {
            LINE_END;
	    newlineneeded = false;
	  }
	}
	else {
	  s++;
	  if (newlineneeded) {
            LINE_END;
	    newlineneeded = false;
	  }
	}
      }
    }
    if (newlineneeded)
      LINE_END;
  }
  else
  {
  while (s <= sel_BR)
  {
      if (s < hist_BR)
      {				// get lines from hist->history buffer.
          eol = hist->getLineLen(hY);
          if (eol > columns)
              eol = columns;
          
          if ((hY == (sel_BR / columns)) &&
              (eol > (sel_BR % columns)))
          {
              eol = sel_BR % columns + 1;
          }

          while (hX < eol)
          {
              Q_UINT16 c = hist->getCell(hY, hX++).c;
              if (c)
                 m[d++] = c;
              s++;
          }

          if (s <= sel_BR)
          {			// The line break handling
              bool wrap = false;
              if (eol % columns == 0)
              { 	        // That's either a full or empty line
                  if ((eol != 0) && hist->isWrappedLine(hY))
                     wrap = true;
              }
              else if ((eol + 1) % columns == 0)
              {
                  if (hist->isWrappedLine(hY))
                     wrap = true;
              }

              if (wrap)
              {
                  LINE_WRAP;
              }
              else
              {
                  LINE_END;
              }
                 
          }
          else
          {
              // Flush trailing stuff
              LINE_FLUSH;
          }

          hY++;
          hX = 0;
          s = hY * columns;
      }
      else
      {				// or from screen image.
        eol = (s / columns + 1) * columns - 1;

        bool addNewLine = false;

        if (eol < sel_BR)
        {
            while ((eol > s) &&
                   (!image[eol - hist_BR].c || isSpace(image[eol - hist_BR].c)) &&
                   !line_wrapped[(eol-hist_BR)/columns])
            {
                eol--;
            }
        }
        else if (eol == sel_BR)
        {
            if (!line_wrapped[(eol - hist_BR)/columns])
	        addNewLine = true;
        }
        else
        {
            eol = sel_BR;
        }

        while (s <= eol)
        {
            Q_UINT16 c = image[s++ - hist_BR].c;
            if (c)
                 m[d++] = c;
        }

        if (eol < sel_BR)
        {			// eol processing
            bool wrap = false;
            if ((eol + 1) % columns == 0)
            {			// the whole line is filled
                if (line_wrapped[(eol - hist_BR)/columns])
                    wrap = true;
            }
            if (wrap)
            {
                LINE_WRAP;
            }
            else
            {
                LINE_END;
            }
        }
        else
        {
            // Flush trailing stuff
            if (addNewLine && preserve_line_breaks)
            {
               LINE_END;
            }
            else
            {
               LINE_FLUSH;
            }
        }

        s = (eol / columns + 1) * columns;
      }
    }
  }

  assert(d == 0);

  delete [] m;
}

void TEScreen::streamHistory(QTextStream* stream) {
  sel_begin = 0;
  sel_BR = sel_begin;
  sel_TL = sel_begin;
  setSelExtentXY(columns-1,lines-1+hist->getLines()-histCursor);
  getSelText(true, stream);
  clearSelection();
}

QString TEScreen::getHistoryLine(int no)
{
  sel_begin = loc(0,no);
  sel_TL = sel_begin;
  sel_BR = loc(columns-1,no);
  return getSelText(false);
}

void TEScreen::addHistLine()
{
  assert(hasScroll() || histCursor == 0);

  // add to hist buffer
  // we have to take care about scrolling, too...

  if (hasScroll())
  { ca dft;

    int end = columns-1;
    while (end >= 0 && image[end] == dft && !line_wrapped[0])
      end -= 1;

    int oldHistLines = hist->getLines();

    hist->addCells(image,end+1);
    hist->addLine(line_wrapped[0]);

    int newHistLines = hist->getLines();

    bool beginIsTL = (sel_begin == sel_TL);

    // adjust history cursor
    if (newHistLines > oldHistLines)
    {
       histCursor++;
       // Adjust selection for the new point of reference
       if (sel_begin != -1)
       {
          sel_TL += columns;
          sel_BR += columns;
       }
    }

    // Scroll up if user is looking at the history and we can scroll up
    if ((histCursor > 0) &&  // We can scroll up and...
        ((histCursor != newHistLines) || // User is looking at history...
          sel_busy)) // or user is selecting text.
    {
       histCursor--;
    }

    if (sel_begin != -1)
    {
       // Scroll selection in history up
       int top_BR = loc(0, 1+newHistLines);

       if (sel_TL < top_BR)
          sel_TL -= columns;

       if (sel_BR < top_BR)
          sel_BR -= columns;

       if (sel_BR < 0)
       {
          clearSelection();
       }
       else
       {
          if (sel_TL < 0)
             sel_TL = 0;
       }

       if (beginIsTL)
          sel_begin = sel_TL;
       else
          sel_begin = sel_BR;
    }
  }

  if (!hasScroll()) histCursor = 0; //FIXME: a poor workaround
}

void TEScreen::setHistCursor(int cursor)
{
  histCursor = cursor; //FIXME:rangecheck
}

int TEScreen::getHistCursor()
{
  return histCursor;
}

int TEScreen::getHistLines()
{
  return hist->getLines();
}

void TEScreen::setScroll(const HistoryType& t)
{
  clearSelection();
  hist = t.getScroll(hist);
  histCursor = hist->getLines();
}

bool TEScreen::hasScroll()
{
  return hist->hasScroll();
}

const HistoryType& TEScreen::getScroll()
{
  return hist->getType();
}

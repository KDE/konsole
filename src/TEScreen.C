/* -------------------------------------------------------------------------- */
/*                                                                            */
/* [TEScreen.C]               Screen Data Type                                */
/*                                                                            */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>            */
/*                                                                            */
/* This file is part of Konsole - an X terminal for KDE                       */
/*                                                                            */
/* -------------------------------------------------------------------------- */

/*! \class TEScreen

    This class implements the operations of the terminal emulation framework.
    It is a complete passive device, driven by the emulation decoder 
    (AnsiEmulation). By this it forms infact an ADT, that mainly defines
    operations on a rectangular image.

    It does neither know how to display its image nor about escape sequences.
    It is further independent of the underlying toolkit. By this, one can even
    use this module for an ordinary text surface.

    Since the operations are called by a specific emulation decoder, one may
    collect their different operations here.

    The state manipulated by the operations is mainly kept in `image'.

    \sa TEWidget \sa AnsiEmulation
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <assert.h>
#include <string.h>
#include <ctype.h>

#include "TEScreen.h"

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

#define HERE printf("%s(%d): here\n",__FILE__,__LINE__)

//FIXME: this is emulation specific. Use FALSE for xterm, TRUE for ANSI.
//FIXME: see if we can get this from terminfo.
#define BS_CLEARS FALSE

#define loc(X,Y) ((Y)*columns+(X))

/*! creates a `TEScreen' of `lines' lines and `columns' columns.
*/

TEScreen::TEScreen(int lines, int columns)
{
  this->lines   = lines;
  this->columns = columns;

  image      = (ca*) malloc(lines*columns*sizeof(ca));
  tabstops   = NULL; initTabStops();
  histBuffer = NULL;
  histLines  = 0;
  histCursor = 0;
  histMaxLines = 0;
  clearSelection();
  reset();
}

/*! Destructor
*/

TEScreen::~TEScreen()
{
  free(image);
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

void TEScreen::cursorUp(int n)
//=CUU
{
  if (n == 0) n = 1; // Default
  int stop = cuY < tmargin ? 0 : tmargin;
  cuX = MIN(columns-1,cuX); // nowrap!
  cuY = MAX(stop,cuY-n);
}

void TEScreen::cursorDown(int n)
//=CUD
{
  if (n == 0) n = 1; // Default
  int stop = cuY > bmargin ? lines-1 : bmargin;
  cuX = MIN(columns-1,cuX); // nowrap!
  cuY = MIN(stop,cuY+n);
}

void TEScreen::cursorLeft(int n)
//=CUB
{
  if (n == 0) n = 1; // Default
  cuX = MIN(columns-1,cuX); // nowrap!
  cuX = MAX(0,cuX-n);
}

void TEScreen::cursorRight(int n)
//=CUF
{
  if (n == 0) n = 1; // Default
  cuX = MIN(columns-1,cuX+n);
}

/*!
*/

void TEScreen::setMargins(int top, int bot)
//=STBM
{
  if (top == 0) top = 1;      // Default
  if (bot == 0) bot = lines;  // Default
  top = top - 1;              // Adjust to internal lineno
  bot = bot - 1;              // Adjust to internal lineno
  if ( !( 0 <= top && top < bot && bot < lines ) )
  { fprintf(stderr,"%s(%d) : setRegion(%d,%d) : bad range.\n",
                   __FILE__,__LINE__,top,bot);
    return;                   // Default error action: ignore
  }
  tmargin = top;
  bmargin = bot;
  cuX = 0;
  cuY = getMode(MODE_Origin) ? top : 0;
}

void TEScreen::clearSelection() 
{
  sel_BR = -1;
  sel_TL = -1;
  sel_begin = -1;
}

void TEScreen::setSelBeginXY(const int x, const int y) 
{
  sel_begin = loc( (x==columns)?(x-1):x,y+histCursor) ;
/*  printf( "SetSelBeginXY( %d, %d, --> %d ) histCursor=%d \n", 
        x, y, sel_begin, histCursor );
*/
  sel_BR = sel_begin;
  sel_TL = sel_begin;
}

void TEScreen::setSelExtentXY(const int x, const int y) 
{
  int l =  loc( (x==columns)?(x-1):x,y + histCursor);

  if (l < sel_begin)
  { 
    sel_TL = l; 
    sel_BR = sel_begin;
  } 
  else
  { 
    sel_TL = sel_begin;
    sel_BR = l; 
  }
/*
  printf( "SetSelExtentXY( %d, %d, TL=%d, BR=%d ) histCursor=%d \n", 
        x, y, sel_TL, sel_BR, histCursor );
 */
}

char *TEScreen::getSelText(const BOOL preserve_line_breaks) 
{
  char *m;
  int s,d; /// source index, dest. index.
  int hist_BR=loc(0,histLines);
  int hY = sel_TL / columns ;
  int hX = sel_TL % columns;
  int eol;
  s = sel_TL;
  d = 0;
  
  // allocate buffer for maximum possible size...
  d = (sel_BR - sel_TL)/columns + 1 ;
  m = (char*) malloc(  sizeof(char) * d * (columns+1) + 2 );
  d = 0;

  while ( s <= sel_BR )
  {
    if ( s < hist_BR )
    { 	// get lines from history buffer.
      eol=histBuffer[hY]->len-1;
      if  ((hY == (sel_BR/columns)) && (eol > (sel_BR%columns)) ) eol=sel_BR%columns;
      while ( hX <= eol )
      { 
         m[d++] = histBuffer[hY]->line[hX++].c; 
         s++;
      }

      // see below for end of line processing...
      if ( s <= sel_BR ) {
	if ( (eol+1)%columns == 0) {
	    if (histBuffer[hY]->line[columns-1].c == ' ') { m[d++]=' '; }
	} 
        else {
		m[d++]=((preserve_line_breaks||(eol%columns==0))?'\n':' ');   
                s = ((s+1)/columns + 1)*columns;
        }
      }
      hY++; 
      hX=0; 
    }
    else // or from screen image.
    {
      eol = (s/columns + 1)*columns - 1 ;
      if ( eol < sel_BR )
      {
        while ((eol > s) && isspace(image[eol-hist_BR].c)) eol--  ;
      }
      else
      {
        eol = sel_BR ;
      }
      while (s <= eol)  m[d++] = image[s++-hist_BR].c;

/* end of line processing for selection -- psilva
cases:

1)    (eol+1)%columns == 0 --> the whole line is filled.
   If the last char is a space, insert (preserve) space. otherwise
   leave the text alone, so that words that are broken by linewrap 
   are preserved.

FIXME:
	* this suppresses \n for command output that is
	  sized to the exact column width of the screen.

2)    eol%columns == 0     --> blank line.
   insert a \n unconditionally.
   Do it either you would because you are in preserve_line_break mode,
   or because it's an ASCII paragraph delimiter, so even when
   not preserving line_breaks, you want to preserve paragraph breaks.

3)    else		 --> partially filled line
   insert a \n in preserve line break mode, else a space 
   The space prevents concatenation of the last word of one
   line with the first of the next.

*/
      if (eol < sel_BR) {
	if ( (eol+1)%columns == 0) {
	    if (image[eol-hist_BR].c == ' ') m[d++]=' ';
	} 
        else m[d++]=(preserve_line_breaks||((eol%columns)==0)?'\n':' ');   
      }
      s = ( eol/columns + 1)*columns;
    }
  }

  // trim buffer size to actual size needed.
  m=(char*)realloc( m ,  sizeof(char)*(d+1) );
  m[d]= '\0';
  //printf( "TEScreen::getSelText returning +%s+\n", m );
  return(m);
}
 
void TEScreen::setHistMaxLines(int maxlines)
// set history buffer size
{
  if (maxlines < histMaxLines) return; //FIXME: handle decrease
  histMaxLines = maxlines;
}

void TEScreen::addHistLine()
{ int x; histLine* line; int end = columns-1;
  ca dft(' ',DEFAULT_FORE_COLOR,DEFAULT_BACK_COLOR,DEFAULT_RENDITION);

  if (histMaxLines == 0) return; // no buffer

  // extract 1st line
  while (end >= 0 && image[end] == dft) end -= 1;
  line = (histLine*) malloc(sizeof(histLine)+sizeof(ca)*(end+1-1));
  for (x = 0; x <= end; x++) line->line[x] = image[loc(x,0)];
  line->len = end+1;

  // add to hist buffer
  if (histLines >= histMaxLines) 
  { int i;
    free(histBuffer[0]);
    for (i = 1; i < histLines; i++) histBuffer[i-1] = histBuffer[i];
    if (histCursor == 0) histCursor = histLines; // revert to non-hist.
    histCursor -= (histLines != histCursor);
  }
  else
  {
    histCursor += (histLines == histCursor);
    histLines  += 1;
    histBuffer  = (histLine**)realloc(histBuffer,sizeof(histLine*)*histLines);

    // correct selection 
    if (sel_TL > 0 ) 
    {
      sel_TL += columns;
      sel_BR += columns;
      sel_begin += columns;
    }
  }
  histBuffer[histLines-1] = line;
}

void TEScreen::index()
//=IND
{
  //FIXME: below bmargin?
  if (cuY >= bmargin)
  {
    if (tmargin == 0 && bmargin == lines-1) addHistLine(); // history
    scrollUp(tmargin,1);
  } 
  else
    cuY += 1;
}

void TEScreen::reverseIndex()

//=RI
{
  //FIXME: above tmargin?
  if (cuY <= tmargin) scrollDown(tmargin,1); else cuY -= 1;
}

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
  int p = MAX(0,MIN(cuX+n-1,columns-1));
  clearImage(loc(cuX,cuY),loc(p,cuY),' ');
}

/*! delete `n' characters starting from (including) the cursor position.
   
    The line is filled in from the right with spaces.
*/

void TEScreen::deleteChars(int n)
{ 
  if (n == 0) n = 1; // Default
  int p = MAX(0,MIN(cuX+n,columns-1));
  moveImage(loc(cuX,cuY),loc(p,cuY),loc(columns-1,cuY));
  clearImage(loc(columns-n,cuY),loc(columns-1,cuY),' ');
}

/*! insert `n' spaces at the cursor position.
   
    The cursor is not moved by the operation.
*/

void TEScreen::insertChars(int n)
{ 
  if (n == 0) n = 1; // Default
  int p = MAX(0,MIN(columns-1-n,columns-1));
  int q = MAX(0,MIN(cuX+n,columns-1));
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

void TEScreen::setMode(int m)
{
  currParm.mode[m] = TRUE;
  switch(m)
  {
    case MODE_Origin : cuX = 0; cuY = tmargin; break; //FIXME: home
  }
}

void TEScreen::resetMode(int m)
{
  currParm.mode[m] = FALSE;
  switch(m)
  {
    case MODE_Origin : cuX = 0; cuY = 0; break; //FIXME: home
  }
}

void TEScreen::saveMode(int m)
{
  saveParm.mode[m] = currParm.mode[m];
}

void TEScreen::restoreMode(int m)
{
  currParm.mode[m] = saveParm.mode[m];
}

//NOTE: this is a helper function
BOOL TEScreen::getMode(int m)
{
  return currParm.mode[m];
}

void TEScreen::saveCursor()
{
  sa_cuX     = cuX;
  sa_cuY     = cuY;
  sa_graphic = graphic;
  sa_pound   = pound;
  sa_cu_re   = cu_re;
  sa_cu_fg   = cu_fg;
  sa_cu_bg   = cu_bg;
  // FIXME: Character set info: sa_charset = charsets[cScreen->charset];
  //                            sa_charset_num = cScreen->charset;
}

void TEScreen::restoreCursor()
{
  cuX     = MIN(sa_cuX,columns-1);
  cuY     = MIN(sa_cuY,lines-1);
  graphic = sa_graphic;
  pound   = sa_pound;
  cu_re   = sa_cu_re;
  cu_fg   = sa_cu_fg;
  cu_bg   = sa_cu_bg;
  // FIXME: Character set info: sa_charset = charsets[cScreen->charset];
  //                            sa_charset_num = cScreen->charset;
  effectiveRendition();
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                             Screen Operations                             */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/*!
*/

void TEScreen::resizeImage(int new_lines, int new_columns)
{
//printf( "resize image(new_lines=%d, new_columns=%d)\n", new_lines, new_columns );
//FIXME: evtl. transfer from/to history buffer

  // make new image
  ca* newimg = (ca*)malloc(new_lines*new_columns*sizeof(ca));

  clearSelection();

  // clear new image
  for (int y = 0; y < new_lines; y++)
  for (int x = 0; x < new_columns; x++)
  {
    newimg[y*new_columns+x].c = ' ';
    newimg[y*new_columns+x].f = DEFAULT_FORE_COLOR;
    newimg[y*new_columns+x].b = DEFAULT_BACK_COLOR;
    newimg[y*new_columns+x].r = DEFAULT_RENDITION;
  }
  int cpy_lines   = MIN(new_lines,  lines);
  int cpy_columns = MIN(new_columns,columns);
  // copy to new image
  for (int y = 0; y < cpy_lines; y++)
  for (int x = 0; x < cpy_columns; x++)
  {
    newimg[y*new_columns+x].c = image[loc(x,y)].c;
    newimg[y*new_columns+x].f = image[loc(x,y)].f;
    newimg[y*new_columns+x].b = image[loc(x,y)].b;
    newimg[y*new_columns+x].r = image[loc(x,y)].r;
  }
  free(image);
  image = newimg;
  lines = new_lines;
  columns = new_columns;
  cuX = MIN(cuX,columns-1);
  cuY = MIN(cuY,lines-1);

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
{ UINT8 f = p->f; UINT8 b = p->b;
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
  {
    ef_fg += BASE_COLORS;
  }
}

/*!
    returns the image.

    Get the size of the image by /sa getLines and /sa getColumns.

    NOTE that the image returned by this function must later be
    freed.

*/

ca* TEScreen::getCookedImage()
{ int x,y;
  ca* merged = (ca*)malloc(lines*columns*sizeof(ca));
  ca dft(' ',DEFAULT_FORE_COLOR,DEFAULT_BACK_COLOR,DEFAULT_RENDITION);

  for (y = 0; (y < lines) && (y < (histLines-histCursor)); y++)
  {
    int len = MIN(columns,histBuffer[y+histCursor]->len);
    ca* img = histBuffer[y+histCursor]->line;
    
    for (x = 0; x < len; x++) 
    {   int p=x + y*columns;
        int q=x + (y+histCursor)*columns;
        merged[p] = img[x];
        if ( ( q >= sel_TL ) && ( q <= sel_BR ) )
          reverseRendition(&merged[p]); // for selection
    }
    for (; x < columns; x++) 
    {   int p=x + y*columns; 
        int q=x + (y+histCursor)*columns;
        merged[p] = dft;
        if ( ( q >= sel_TL ) && ( q <= sel_BR ) )
          reverseRendition(&merged[p]); // for selection
    }
  }
  if (lines >= histLines-histCursor)
  {
    for (y = (histLines-histCursor); y < lines ; y++)
    {
       for (x = 0; x < columns; x++) 
       { int p = x + y * columns;
         int q = x + (y+histCursor)*columns;
         int r = x + (y-histLines+histCursor)*columns;
         merged[p] = image[r];
         if ( q >= sel_TL && q <= sel_BR )
           reverseRendition(&merged[p]); // for selection
       }
        
    }
  }
  // evtl. inverse display
  if (getMode(MODE_Screen))
  { int i,n = lines*columns;
    for (i = 0; i < n; i++)
      reverseRendition(&merged[i]); // for reverse display
  }
  if (getMode(MODE_Cursor) && (cuY+(histLines-histCursor) < lines)) // cursor visible
    reverseRendition(&merged[loc(cuX,cuY+(histLines-histCursor))]);
  return merged;
}


/*!
*/

void TEScreen::reset()
{
    setMode(MODE_Wrap  ); saveMode(MODE_Wrap  );  // wrap at end of margin
  resetMode(MODE_Origin); saveMode(MODE_Origin);  // position refere to [1,1]
  resetMode(MODE_Insert); saveMode(MODE_Origin);  // overstroke
    setMode(MODE_Cursor);                         // cursor visible
  resetMode(MODE_Screen);                         // screen not inverse
  resetMode(MODE_NewLine);

  tmargin=0;
  bmargin=lines-1;

  cu_cs   = 0;
  strncpy(charset,"BBBB",4);
  graphic = FALSE;
  pound   = FALSE;

  setDefaultRendition();
  saveCursor();

  clear();
}

/*!
*/

void TEScreen::clear()
{ 
  clearEntireScreen();
  home();
}

/*!
*/

void TEScreen::BackSpace()
{
  cuX = MAX(0,cuX-1);
  if (BS_CLEARS) image[loc(cuX,cuY)].c = ' ';
}

/*!
*/

void TEScreen::Tabulate()
{
  // note that TAB is a format effector (does not write ' ');
  cursorRight(1); while(cuX < columns-1 && !tabstops[cuX]) cursorRight(1);
}

void TEScreen::clearTabStops()
{
  for (int i = 0; i < columns; i++) tabstops[i-1] = FALSE;
}

void TEScreen::changeTabStop(bool set)
{
  if (cuX >= columns) return;
  tabstops[cuX] = set;
}

void TEScreen::initTabStops()
{
  if (tabstops) free(tabstops);
  tabstops = (bool*)malloc(columns*sizeof(bool));
//FIXME: Arrg! The 1st tabstop has to be one longer than the other!
//       i.e. the kids start counting from 0 instead of 1.
//       Other programs might behave correctly. Be aware.
  for (int i = 0; i < columns; i++) tabstops[i] = (i%8 == 0 && i != 0);
}

/*!
*/

void TEScreen::NewLine()
{
// This behaves either as IND (index) or as NEL (NextLine)
// depending on the NewLine Mode (LNM). This mode also
// affects the key sequence returned for newline ([CR]LF).

  if (getMode(MODE_NewLine)) Return();
  index();
}

/*! put `c' literally onto the screen at the current cursor position.

    VT100 uses the convention to produce an automatic newline (am)
    with the *first* character that would fall onto the next line (xenl).
*/

void TEScreen::ShowCharacter(unsigned char c)
{

  // if (xenl)
  if (cuX >= columns)
  {
//    fprintf(stderr, "at eol, wrap=%d c=+%c+(%d)\n", getMode(MODE_Wrap), c, c );
    if (getMode(MODE_Wrap)) NextLine(); else cuX = columns-1;
    
  }

  if (graphic) // handle graphical character set here
  {
    if (c >= 0x5f && c <= 0x7e) c = (c == 0x5f) ? 0x7f : c - 0x5f;
    //FIXME: the linux console knows some non-VT100 chars, too.
  }
  if (pound && c == '#') c = 0xa3; // pound sign

  if (getMode(MODE_Insert)) insertChars(1);

  int i = loc(cuX,cuY) ;

  if ( ((i+histCursor*columns)<=sel_BR) &&
       ((i+histCursor*columns)>=sel_TL) && 
       c != image[i].c                     ) clearSelection();

  image[i].c = c;
  image[i].f = ef_fg;
  image[i].b = ef_bg;
  image[i].r = ef_re;

  cuX += 1;

  //FIXME: VT100 wrapping is more complicated then i first thought.
  //       It does not happen after, but before the character is put.
  //       This has impact on the assumption of valid cursor positions.
  //       We indicate the fact that a newline has to be triggered by
  //       putting the cursor one right to the last column of the screen.
  // Further, other emulations (ansi.sys) might be more straight, so
  // the actual processing should be made dependend on a flag (xenl).

  // if (!xenl)
  // if (cuX >= columns)
  // {
  //   if (getMode(MODE_Wrap)) NextLine(); else cuX = columns-1;
  // }

}

/*!
*/

void TEScreen::setCharset(int n, int cs)
{
  charset[n&3] = cs;
  useCharset(cu_cs);
}

/*!
*/

void TEScreen::setAndUseCharset(int n, int cs)
{
  charset[n&3] = cs;
  useCharset(n&3);
}

/*!
*/

void TEScreen::useCharset(int n)
{
  cu_cs   = n&3;
  graphic = (charset[n&3] == '0');
  pound   = (charset[n&3] == 'A');
}

// Region commands -------------------------------------------------------------


/*! scroll up `n' lines within current region.
    The `n' new lines are cleared.
    \sa setRegion \sa scrollDown
*/

void TEScreen::scrollUp(int from, int n)
{
//  printf( " TEScreen::scrollUp(%d, %d)\n" , from, n );
  if (n <= 0 || from + n > bmargin) return;
  //FIXME: make sure `tmargin', `bmargin', `from', `n' is in bounds.
  moveImage(loc(0,from),loc(0,from+n),loc(columns-1,bmargin));
  clearImage(loc(0,bmargin-n+1),loc(columns-1,bmargin),' ');
  
  if ( from == 0 && sel_begin >= 0) //FIXME: && lines scroll into the history buffer
                                    //FIXME: condition is not ok.
  {
      sel_TL -= (n-from)*columns;
      sel_BR -= (n-from)*columns;
  } else 
      clearSelection();
        
}

/*! scroll down `n' lines within current region.
    The `n' new lines are cleared.
    \sa setRegion \sa scrollUp
*/

void TEScreen::scrollDown(int from, int n)
{
//FIXME: make sure `tmargin', `bmargin', `from', `n' is in bounds.
//  printf( " TEScreen::scrollDown(%d)\n" , n );
  if (n <= 0) return;
  if (from > bmargin) return;
  if (from + n > bmargin) n = bmargin - from;
  moveImage(loc(0,from+n),loc(0,from),loc(columns-1,bmargin-n));
  clearImage(loc(0,from),loc(columns-1,from+n-1),' ');
  if (from == 0 && sel_begin >= 0) //FIXME: condition is not ok
  {
     sel_TL += (n-from)*columns;
     sel_BR += (n-from)*columns;
  } 
  else
     clearSelection();
}

void TEScreen::setCursorYX(int y, int x)
{
  setCursorY(y); setCursorX(x);
}

void TEScreen::setCursorX(int x)
{
  if (x == 0) x = 1; // Default
  x -= 1; // Adjust
  cuX = MIN(columns-1, x);
}

void TEScreen::setCursorY(int y)
{
  if (y == 0) y = 1; // Default
  y -= 1; // Adjust
  cuY = MIN(lines  -1, y + (getMode(MODE_Origin) ? tmargin : 0) );
}

/*! set cursor to the `left upper' corner of the screen.
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
  int scr_TL=loc(0,histCursor);
  //FIXME: check positions

  //Clear entire selection if it overlaps region to be moved...
  if ( (sel_BR > (loca+scr_TL) )&&(sel_TL < (loce+scr_TL)) )
  {
    clearSelection();
  }
  for (i = loca; i <= loce; i++)
  {
    image[i].c = c;
    image[i].f = ef_fg; //DEFAULT_FORE_COLOR; //FIXME: xterm and linux/ansi
    image[i].b = ef_bg; //DEFAULT_BACK_COLOR; //       many have different
    image[i].r = ef_re; //DEFAULT_RENDITION;  //       ideas here.
  }
}

/*! move image between (including) `loca' and `loce' to 'dst'.

    This is an internal helper functions. The parameter types are internal
    addresses of within the screen image and make use of the way how the
    screen matrix is mapped to the image vector.
*/

void TEScreen::moveImage(int dst, int loca, int loce)
{
//FIXME: check positions
/*printf( " TEScreen::moveImage(dst=%d, loca=%d, loce=%d)\n",
        dst, loca, loce );
*/
  memmove(&image[dst],&image[loca],(loce-loca+1)*sizeof(ca));
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
  setForeColorToDefault();
  setBackColorToDefault();
  cu_re   = DEFAULT_RENDITION;
  effectiveRendition();
}

/*!
*/

void TEScreen::setForeColor(int fgcolor)
{
  cu_fg = (fgcolor&7)+((fgcolor&8) ? 4+8 : 2);
  effectiveRendition();
}

/*!
*/

void TEScreen::setBackColor(int bgcolor)
{
  cu_bg = (bgcolor&7)+((bgcolor&8) ? 4+8 : 2);
  effectiveRendition();
}

/*!
*/

void TEScreen::setBackColorToDefault()
{
  cu_bg = DEFAULT_BACK_COLOR;
  effectiveRendition();
}

/*!
*/

void TEScreen::setForeColorToDefault()
{
  cu_fg = DEFAULT_FORE_COLOR;
  effectiveRendition();
}

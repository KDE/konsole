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

#ifndef TESCREEN_H
#define TESCREEN_H

#include "TECommon.h"
#include "TEHistory.h"

#define MODE_Origin    0
#define MODE_Wrap      1
#define MODE_Insert    2
#define MODE_Screen    3
#define MODE_Cursor    4
#define MODE_NewLine   5
#define MODES_SCREEN   6

/*!
*/
struct ScreenParm
{
  int mode[MODES_SCREEN];
};


class TEScreen
{
public:
    TEScreen(int lines, int columns);
    ~TEScreen();

public: // these are all `Screen' operations
    //
    // VT100/2 Operations ------------------
    //
    // Cursor Movement
    //
    void cursorUp    (int n);
    void cursorDown  (int n);
    void cursorLeft  (int n);
    void cursorRight (int n);
    void setCursorY  (int y);
    void setCursorX  (int x);
    void setCursorYX (int y, int x);
    void setMargins  (int t, int b);
    //
    // Cursor Movement with Scrolling
    //
    void NewLine     ();
    void NextLine    ();
    void index       ();
    void reverseIndex();
    //
    // Scrolling
    //
    void scrollUp(int n);
    void scrollDown(int n);
    //
    void Return      ();
    void BackSpace   ();
    void Tabulate    (int n = 1);
    void backTabulate(int n);
    //
    // Editing
    //
    void eraseChars  (int n);
    void deleteChars (int n);
    void insertChars (int n);
    void deleteLines (int n);
    void insertLines (int n);
    //
    // -------------------------------------
    //
    void clearTabStops();
    void changeTabStop(bool set);
    //
    void resetMode   (int n);
    void setMode     (int n);
    void saveMode    (int n);
    void restoreMode (int n);
    //
    void saveCursor  ();
    void restoreCursor();
    //
    // -------------------------------------
    //
    void clearEntireScreen();
    void clearToEndOfScreen();
    void clearToBeginOfScreen();
    //
    void clearEntireLine();
    void clearToEndOfLine();
    void clearToBeginOfLine();
    //
    void helpAlign   ();
    //
    // -------------------------------------
    //
    void setRendition  (int rendition);
    void resetRendition(int rendition);
    //
    void setForeColor  (int space, int color);
    void setBackColor  (int space, int color);
    //
    void setDefaultRendition();
    //
    // -------------------------------------
    //
    bool getMode     (int n);
    //
    // only for report cursor position
    //
    int  getCursorX();
    int  getCursorY();
    //
    // -------------------------------------
    //
    void clear();
    void home();
    void reset();
    // Show character
    void ShowCharacter(unsigned short c);
    
    // Do composition with last shown character
    void compose(QString compose);
    
    //
    void resizeImage(int new_lines, int new_columns);
    //
    ca*  getCookedImage();
    QBitArray getCookedLineWrapped();

    /*! return the number of lines. */
    int  getLines()   { return lines; }
    /*! return the number of columns. */
    int  getColumns() { return columns; }

    /*! set the position of the history cursor. */
    void setHistCursor(int cursor);
    /*! return the position of the history cursor. */

    int  getHistCursor();
    int  getHistLines ();
    void setScroll(const HistoryType&);
    const HistoryType& getScroll();
    bool hasScroll();

    //
    // Selection
    //
    void setSelBeginXY(const int x, const int y, const bool columnmode);
    void setSelExtentXY(const int x, const int y);
    void clearSelection();
    void setBusySelecting(bool busy) { sel_busy = busy; }
    bool testIsSelected(const int x,const int y);

    QString getSelText(bool preserve_line_breaks);
    void getSelText(bool preserve_line_breaks, QTextStream* stream);
    void streamHistory(QTextStream* stream);
    QString getHistoryLine(int no);

    void checkSelection(int from, int to);

private: // helper

    void clearImage(int loca, int loce, char c);
    void moveImage(int dst, int loca, int loce);
    
    void scrollUp(int from, int i);
    void scrollDown(int from, int i);

    void addHistLine();

    void initTabStops();

    void effectiveRendition();
    void reverseRendition(ca* p);

    /*
       The state of the screen is more complex as one would
       expect first. The screem does really do part of the
       emulation providing state informations in form of modes,
       margins, tabulators, cursor etc.

       Even more unexpected are variables to save and restore
       parts of the state.
    */

    // screen image ----------------

    int lines;
    int columns;
    ca *image; // [lines][columns]
    QBitArray line_wrapped; // [lines]

    // history buffer ---------------

    int histCursor;   // display position relative to start of the history buffer
    HistoryScroll *hist;
    
    // cursor location

    int cuX;
    int cuY;

    // cursor color and rendition info

    cacol cu_fg;      // foreground
    cacol cu_bg;      // background
    UINT8 cu_re;      // rendition

    // margins ----------------

    int tmargin;      // top margin
    int bmargin;      // bottom margin

    // states ----------------

    ScreenParm currParm;

    // ----------------------------

    bool* tabstops;

    // selection -------------------

    int sel_begin; // The first location selected.
    int sel_TL;    // TopLeft Location.
    int sel_BR;    // Bottom Right Location.
    bool sel_busy; // Busy making a selection.
    bool columnmode;  // Column selection mode

    // effective colors and rendition ------------

    cacol ef_fg;      // These are derived from
    cacol ef_bg;      // the cu_* variables above
    UINT8 ef_re;      // to speed up operation

    //
    // save cursor, rendition & states ------------
    // 

    // cursor location

    int sa_cuX;
    int sa_cuY;

    // rendition info

    UINT8 sa_cu_re;
    cacol sa_cu_fg;
    cacol sa_cu_bg;
    
    // last position where we added a character
    int lastPos;

    // modes

    ScreenParm saveParm;
};

#endif // TESCREEN_H

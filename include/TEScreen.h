/* -------------------------------------------------------------------------- */
/*                                                                            */
/* [te_screen.h]                 Screen Data Type                             */
/*                                                                            */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>            */
/*                                                                            */
/* This file is part of Konsole - an X terminal for KDE                       */
/*                                                                            */
/* -------------------------------------------------------------------------- */

#ifndef TESCREEN_H
#define TESCREEN_H

/*! \file
*/

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
    void Return      ();
    void BackSpace   ();
    void Tabulate    ();
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
    void setForeColor  (int fgcolor);
    void setBackColor  (int bgcolor);
    //
    void setDefaultRendition();
    void setForeColorToDefault();
    void setBackColorToDefault();
    //
    // -------------------------------------
    //
    BOOL getMode     (int n);
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
    //
    void ShowCharacter(unsigned short c);
    //
    void resizeImage(int new_lines, int new_columns);
    //
    ca*  getCookedImage();
    
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
    void setSelBeginXY(const int x, const int y);
    void setSelExtentXY(const int x, const int y);
    void clearSelection();
    QString getSelText(const BOOL preserve_line_breaks);

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

private:

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

    // history buffer ---------------

    int histCursor;   // display position relative to start of the history buffer
    HistoryScroll *hist;
    
    // cursor location

    int cuX;
    int cuY;

    // cursor color and rendition info

    UINT8 cu_fg;      // foreground
    UINT8 cu_bg;      // background
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

    // effective colors and rendition ------------

    UINT8 ef_fg;      // These are derived from
    UINT8 ef_bg;      // the cu_* variables above
    UINT8 ef_re;      // to speed up operation

    //
    // save cursor, rendition & states ------------
    // 

    // cursor location

    int sa_cuX;
    int sa_cuY;

    // rendition info

    UINT8 sa_cu_re;
    UINT8 sa_cu_fg;
    UINT8 sa_cu_bg;

    // modes

    ScreenParm saveParm;
};

#endif // TESCREEN_H

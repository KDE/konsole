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

/*!
*/
#define MODE_Origin    0
/*!
*/
#define MODE_Wrap      1
/*!
*/
#define MODE_Insert    2
/*!
*/
#define MODE_Screen    3
/*!
*/
#define MODE_Cursor    4
/*!
*/
#define MODE_NewLine   5
/*!
*/
#define MODES_SCREEN   6

/*!
*/
struct ScreenParm
{
  int mode[MODES_SCREEN];
};

/*!
*/
struct histLine
// basically, another string representation
{
  int len;
  ca  line[1]; // [len], really.
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
    void ShowCharacter(unsigned char c);
    //
    void setCharset(int n, int cs);       //FIXME: evtl. move to emulation
    void setAndUseCharset(int n, int cs); //FIXME: evtl. move to emulation
    void useCharset(int n);               //FIXME: evtl. move to emulation
    //
    void resizeImage(int new_lines, int new_columns);
    //
    ca*  getCookedImage();
    
    /*! return the number of lines. */
    int  getLines()   { return lines; }
    /*! return the number of columns. */
    int  getColumns() { return columns; }

    /*! set the position of the history cursor. */
    void setHistCursor(int cursor) { histCursor = cursor; } //FIXME:rangecheck
    /*! return the position of the history cursor. */
    int  getHistCursor() { return histCursor; }
    /*! return the number of lines in the history buffer. */
    int  getHistLines (){ return histLines;  }
    void setHistMaxLines(int maxLines);

    //
    // Selection
    //
    void setSelBeginXY(const int x, const int y);
    void setSelExtentXY(const int x, const int y);
    void clearSelection();
    char *getSelText(const BOOL preserve_line_breaks);

private: // helper

    void clearImage(int loca, int loce, char c);
    void moveImage(int dst, int loca, int loce);
    
    void scrollUp(int from, int i);
    void scrollDown(int from, int i);

    void addHistLine();

    void swapCursor();
    void initTabStops();

    void effectiveRendition();
    void reverseRendition(ca* p);


private:

    bool* tabstops;
    char charset[4]; //FIXME: evtl. move to emulation

    // rendition info
    int   cu_cs;      // actual charset.
    bool  graphic;    //FIXME: evtl. move to emulation
    bool  pound  ;    //FIXME: evtl. move to emulation
    UINT8 cu_fg;
    UINT8 cu_bg;
    UINT8 cu_re;      // rendition

    UINT8 ef_fg;
    UINT8 ef_bg;
    UINT8 ef_re;      // effective rendition
    
    // cursor location
    int cuX;
    int cuY;

    // save cursor & rendition  --------------------

    // rendition info
    bool  sa_graphic;    //FIXME: evtl. move to emulation
    bool  sa_pound;      //FIXME: evtl. move to emulation
    UINT8 sa_cu_re;
    UINT8 sa_cu_fg;
    UINT8 sa_cu_bg;

    // cursor location
    int sa_cuX;
    int sa_cuY;

    // margins ----------------

    int bmargin;
    int tmargin;

    // states ----------------
    // FIXME: this is a first try
    ScreenParm currParm;
    ScreenParm saveParm;

    // screen image ----------------
    int lines;
    int columns;
    ca *image; // [lines][columns]

    // history buffer ---------------

    int histCursor;   // display position relative to start of histBuffer
    int histLines;    // current lines in histbuffer
    int histMaxLines; // maximum lines in histbuffer
    histLine** histBuffer;

    // selection -------------------

    int sel_begin; // The first location selected.
    int sel_TL;    // TopLeft Location.
    int sel_BR;    // Bottom Right Location.

};

#endif // TESCREEN_H

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
//Added by qt3to4:
#include <QTextStream>
#include <QVarLengthArray>

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

class TerminalCharacterDecoder;

/**
    \brief An image of characters with associated attributes.

    The terminal emulation ( TEmulation ) receives a serial stream of
    characters from the program currently running in the terminal.
    From this stream it creates an image of characters which is ultimately
    rendered by the display widget ( TEWidget ).  Some types of emulation
    may have more than one screen image. 

    getCookedImage() is used to retrieve the currently visible image
    which is then used by the display widget to draw the output from the
    terminal. 

    The number of lines of output history which are kept in addition to the current
    screen image depends on the history scroll being used to store the output.  
    The scroll is specified using setScroll()
    The output history can be retrieved using writeToStream()

    The screen image has a selection associated with it, specified using 
    setSelectionStart() and setSelectionEnd().  The selected text can be retrieved
    using selectedText().  When getCookedImage() is used to retrieve the the visible image,
    characters which are part of the selection have their colours inverted.   
*/
class TEScreen
{
public:
    /** Construct a new screen image of size @p lines by @p columns. */
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
    //sets the scrolling margins back to their default positions
    void setDefaultMargins();
    
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
    
    // Do composition with last shown character FIXME: Not implemented yet for KDE 4
    void compose(QString compose);
    
    /** 
     * Resizes the image to a new fixed size of @p new_lines by @p new_columns.  
     * In the case that @p new_columns is smaller than the current number of columns,
     * existing lines are not truncated.  This prevents characters from being lost
     * if the terminal display resized smaller and then larger again.
     *
     * (note that in versions of Konsole prior to KDE 4, existing lines were
     *  truncated when making the screen image smaller)
     */
    void resizeImage(int new_lines, int new_columns);
    
    // Return current on screen image.  Result array is [getLines()][getColumns()]
    ca*  	  getCookedImage();

    /** 
     * Returns the additional attributes associated with lines in the image.
     * The most important attribute is LINE_WRAPPED which specifies that the line is wrapped,
     * other attributes control the size of characters in the line.
     */
    QVector<LineProperty> getCookedLineProperties();
	

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
    
    /** 
     * Sets the start of the selection.
     *
     * @param column The column index of the first character in the selection.
     * @param line The line index of the first character in the selection.
     * @param columnmode TODO: Document me!
     */
    void setSelectionStart(const int column, const int line, const bool columnmode);
    /**
     * Sets the end of the current selection.  If this is before the selection start
     * specified with setSelectionStart() then the selection will be normalized (ie.
     * the start of the selection will always occur before the end of the selection). 
     *
     * @param column The column index of the last character in the selection.
     * @param line The line index of the last character in the selection. 
     */ 
    void setSelectionEnd(const int column, const int line);
    void clearSelection();
    void setBusySelecting(bool busy) { sel_busy = busy; }

    /**
     * Returns true if the character at (@p column, @p line) is part of the current selection.
     */ 
    bool isSelected(const int column,const int line);

    /** 
     * Convenience method.  Returns the currently selected text. 
     * @param preserve_line_breaks TODO: Not yet handled in KDE 4.  See comments near definition. 
     */
    QString selectedText(bool preserve_line_breaks);
	    
	/** 
	 * Copies the entire output history, including the characters currently on screen
	 * into a text stream.
	 *
	 * @param stream An output stream which receives the history text
	 * @param decoder A decoder which converts terminal characters into text.  PlainTextDecoder
	 * 				  is the most commonly used decoder which coverts characters into plain
	 * 				  text with no formatting.
	 */
	void writeToStream(QTextStream* stream , TerminalCharacterDecoder* decoder);

	/**
	 * Copies part of the output to a stream.
	 *
	 * @param stream An output stream which receives the text
	 * @param decoder A decoder which coverts terminal characters into text
	 * @param from The first line in the history to retrieve
	 * @param to The last line in the history to retrieve
	 */
	void writeToStream(QTextStream* stream , TerminalCharacterDecoder* decoder, int from, int to);
	
    QString getHistoryLine(int no);

	/**
	 * Copies the selected characters, set using @see setSelBeginXY and @see setSelExtentXY
	 * into a stream using the specified character decoder.
	 *
	 * @param stream An output stream which receives the converted text
	 * @param decoder A decoder which converts terminal characters into text.  PlainTextDecoder
	 * 				  is the most commonly used decoder which coverts characters into plain
	 * 				  text with no formatting.
	 */
	void writeSelectionToStream(QTextStream* stream , TerminalCharacterDecoder* decoder);

    void checkSelection(int from, int to);

	/** 
	 * Sets or clears an attribute of the current line.
	 * 
	 * @param property The attribute to set or clear
	 * Possible properties are:
	 * LINE_WRAPPED:	 Specifies that the line is wrapped.
	 * LINE_DOUBLEWIDTH: Specifies that the characters in the current line should be double the normal width.
	 * LINE_DOUBLEHEIGHT:Specifies that the characters in the current line should be double the normal height.
	 *
	 * @param enable true to apply the attribute to the current line or false to remove it
	 */
	void setLineProperty(LineProperty property , bool enable);

private: // helper

	//copies a line of text from the screen or history into a stream using a specified character decoder
	//line - the line number to copy, from 0 (the earliest line in the history) up to 
	//		 hist->getLines() + lines - 1
	//start - the first column on the line to copy
	//count - the number of characters on the line to copy
	//stream - the output stream to write the text into
	//decoder - a decoder which coverts terminal characters (an ca array) into text
	void copyLineToStream(int line, int start, int count, QTextStream* stream,
						  TerminalCharacterDecoder* decoder);
	
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
       emulation providing state information in form of modes,
       margins, tabulators, cursor etc.

       Even more unexpected are variables to save and restore
       parts of the state.
    */

    // screen image ----------------

    int lines;
    int columns;


    typedef QVector<ca> ImageLine;      // [0..columns]
    ImageLine*          screenLines;    // [lines]

    QVarLengthArray<LineProperty,64> lineProperties;    
	
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

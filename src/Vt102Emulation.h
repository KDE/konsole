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

#ifndef VT102EMU_H
#define VT102EMU_H

// Standard Library
#include <stdio.h>

// Qt 
#include <QKeyEvent>
#include <QHash>
#include <QTimer>

// Konsole
#include "Emulation.h"
#include "Screen.h"

#define MODE_AppScreen (MODES_SCREEN+0)
#define MODE_AppCuKeys (MODES_SCREEN+1)
#define MODE_AppKeyPad (MODES_SCREEN+2)
#define MODE_Mouse1000 (MODES_SCREEN+3)
#define MODE_Mouse1001 (MODES_SCREEN+4)
#define MODE_Mouse1002 (MODES_SCREEN+5)
#define MODE_Mouse1003 (MODES_SCREEN+6)
#define MODE_Ansi      (MODES_SCREEN+7)
#define MODE_total     (MODES_SCREEN+8)

namespace Konsole
{

struct DECpar
{
  bool mode[MODE_total];
};

struct CharCodes
{
  // coding info
  char charset[4]; //
  int  cu_cs;      // actual charset.
  bool graphic;    // Some VT100 tricks
  bool pound  ;    // Some VT100 tricks
  bool sa_graphic; // saved graphic
  bool sa_pound;   // saved pound
};

/**
 * Provides an xterm compatible terminal emulation based on the DEC VT102 terminal.
 * A full description of this terminal can be found at http://vt100.net/docs/vt102-ug/
 * 
 * In addition, various additional xterm escape sequences are supported to provide 
 * features such as mouse input handling.
 * See http://rtfm.etla.org/xterm/ctlseq.html for a description of xterm's escape
 * sequences. 
 *
 */
class Vt102Emulation : public Emulation
{ Q_OBJECT

public:

  /** Constructs a new emulation */
  Vt102Emulation();
  ~Vt102Emulation();
  
public Q_SLOTS: 

  /** Sends a raw character string to the terminal */
  virtual void sendString(const char*);

  virtual void sendText(const QString& text);
  virtual void onKeyPress(QKeyEvent*);
  virtual void onMouse( int buttons, int column, int line , int eventType );
  
Q_SIGNALS:

  /**
   * Emitted when the program running in the terminal wishes to update the 
   * session's title.  This also allows terminal programs to customize other
   * aspects of the terminal emulation display. 
   *
   * This signal is emitted when the escape sequence "\033]ARG;VALUE\007"
   * is received in the input string, where ARG is a number specifying what
   * should change and VALUE is a string specifying the new value.
   *
   * TODO:  The name of this method is not very accurate since this method
   * is used to perform a whole range of tasks besides just setting
   * the user-title of the session.    
   *
   * @param title Specifies what to change.
   *    0 - Set window icon text and session title to @p newTitle
   *    1 - Set window icon text to @p newTitle
   *    2 - Set session title to @p newTitle
   *    11 - Set the session's default background color to @p newTitle,
   *         where @p newTitle can be an HTML-style string (#RRGGBB) or a named
   *         color (eg 'red', 'blue').  
   *         See http://doc.trolltech.com/4.2/qcolor.html#setNamedColor for more
   *         details.
   *    31 - Supposedly treats @p newTitle as a URL and opens it (NOT IMPLEMENTED)
   *    32 - Sets the icon associated with the session.  @p newTitle is the name 
   *    of the icon to use, which can be the name of any icon in the current KDE icon
   *    theme (eg: 'konsole', 'kate', 'folder_home')
   * @param newTitle Specifies the new title 
   */
  void changeTitle(int title,const QString& newTitle);

public:

  void clearEntireScreen();
  void reset();

  void onReceiveChar(int cc);

public Q_SLOTS:
    
public:

  bool getMode    (int m);

  void setMode    (int m);
  void resetMode  (int m);
  void saveMode   (int m);
  void restoreMode(int m);
  void resetModes();

  void setConnect(bool r);
  
  char getErase();

private:

  //Scrolls all the views on this emulation.
  //lines may be positive (to scroll down) or negative (to scroll up) 
  void scrollView( int lines );
  //Scrolls all the views on this emulation by a given number of pages - where a page
  //is half the number of visible lines.  Page Up and Page Down scroll by -1 and +1 pages
  //respectively.
  void scrollViewPages( int pages );

  //REMOVED Enables or disables mouse marking in all the views on this emulation
  //REMOVED void setViewMouseMarks( bool marks );


  //REMOVED 
  //
  //Enables or disables Vt102 specific handling of input from the view 
  //(including xterm-style mouse input for example)
  //
  //See also - Emulation::connectView()
  //void setReceiveViewInput( TerminalDisplay* view , bool enable );

  void resetToken();
#define MAXPBUF 80
  void pushToToken(int cc);
  int pbuf[MAXPBUF]; //FIXME: overflow?
  int ppos;
#define MAXARGS 15
  void addDigit(int dig);
  void addArgument();
  int argv[MAXARGS];
  int argc;
  void initTokenizer();
  int tbl[256];

  void scan_buffer_report(); //FIXME: rename
  void ReportErrorToken();   //FIXME: rename

  void tau(int code, int p, int q);
  void XtermHack();

  //

  void reportTerminalType();
  void reportSecondaryAttributes();
  void reportStatus();
  void reportAnswerBack();
  void reportCursorPosition();
  void reportTerminalParms(int p);

  void onScrollLock();
  void scrollLock(const bool lock);
 
protected slots:
		
  //causes changeTitle() to be emitted for each (int,QString) pair in pendingTitleUpdates
  //used to buffer multiple title updates
  void updateTitle();
 
protected:

  unsigned short applyCharset(unsigned short c);
  void setCharset(int n, int cs);
  void useCharset(int n);
  void setAndUseCharset(int n, int cs);
  void saveCursor();
  void restoreCursor();
  void resetCharset(int scrno);

  void setMargins(int t, int b);
  //set margins for all screens back to their defaults
  void setDefaultMargins();

  CharCodes _charset[2];

  DECpar _currParm;
  DECpar _saveParm;
  bool _holdScreen;

  //hash table and timer for buffering calls to the session instance to update the name of the session
  //or window title.
  //these calls occur when certain escape sequences are seen in the output from the terminal
  QHash<int,QString> _pendingTitleUpdates;
  QTimer _titleUpdateTimer;
  
};

}

#endif // ifndef ANSIEMU_H

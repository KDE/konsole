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

#include "TEWidget.h"
#include "TEScreen.h"
#include "TEmulation.h"
#include <stdio.h>

//

#define MODE_AppScreen (MODES_SCREEN+0)
#define MODE_AppCuKeys (MODES_SCREEN+1)
#define MODE_AppKeyPad (MODES_SCREEN+2)
#define MODE_Mouse1000 (MODES_SCREEN+3)
#define MODE_Ansi      (MODES_SCREEN+4)
#define MODE_total     (MODES_SCREEN+5)

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

class TEmuVt102 : public TEmulation
{ Q_OBJECT

public:

  TEmuVt102(TEWidget* gui);
  void changeGUI(TEWidget* newgui);
  ~TEmuVt102();

  virtual void onKeyPress(QKeyEvent*);
public slots: // signals incoming from TEWidget

  void onMouse(int cb, int cx, int cy);

signals:

  void changeTitle(int,const QString&);

public:

  void clearEntireScreen();
  void reset();

  void onRcvChar(int cc);
public slots:
  void sendString(const char *);

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

protected:

  unsigned short applyCharset(unsigned short c);
  void setCharset(int n, int cs);
  void useCharset(int n);
  void setAndUseCharset(int n, int cs);
  void saveCursor();
  void restoreCursor();
  void resetCharset(int scrno);
  void setMargins(int t, int b);

  CharCodes charset[2];

  DECpar currParm;
  DECpar saveParm;
  bool holdScreen;
};

#endif // ifndef ANSIEMU_H

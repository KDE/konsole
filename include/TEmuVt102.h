/* -------------------------------------------------------------------------- */
/*                                                                            */
/* [xtermemu.h]             X Terminal Emulation                              */
/*                                                                            */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>            */
/*                                                                            */
/* This file is part of Konsole - an X terminal for KDE                       */
/*                                                                            */
/* -------------------------------------------------------------------------- */

#ifndef VT102EMU_H
#define VT102EMU_H

#include "TEWidget.h"
#include "TEScreen.h"
#include "TEmulation.h"
#include <qtimer.h>
#include <stdio.h>

#define MODE_AppScreen (MODES_SCREEN+0)
#define MODE_AppCuKeys (MODES_SCREEN+1)
#define MODE_AppKeyPad (MODES_SCREEN+2)
#define MODE_Mouse1000 (MODES_SCREEN+3)
#define MODE_Ansi      (MODES_SCREEN+4)
#define MODE_BsHack    (MODES_SCREEN+5)
#define MODE_total     (MODES_SCREEN+6)

struct DECpar
{
  BOOL mode[MODE_total];
};

class VT102Emulation : public Emulation // QObject
{ Q_OBJECT

public:

  VT102Emulation(TEWidget* gui, const char* term);
  ~VT102Emulation();

public slots: // signals incoming from TEWidget
 
  void onImageSizeChange(int lines, int columns);
  void onKeyPress(QKeyEvent*);
  void onMouse(int cb, int cx, int cy);

signals:
  void changeTitle(int,const char*);

public:

  void tableInit();
  void onRcvByte(int);
  void reset();
  void resetTerminal();
  void sendString(const char *);

public:
    
  BOOL getMode    (int m);

  void setMode    (int m);
  void resetMode  (int m);
  void saveMode   (int m);
  void restoreMode(int m);

  void setCharset(int n, int cs); //FIXME: experimental

  void setConnect(bool r);

  void setColumns(int columns);   //FIXME: experimental

  void NewLine    ();             //FIXME: evtl. move to TEmulation

private:

#define MAXPBUF 80
  unsigned char pbuf[MAXPBUF]; //FIXME: overflow!
  int ppos;
#define MAXARGS 15
  int argv[MAXARGS];
  int argc;
  int tbl[256];

  void scan_buffer_report(); //FIXME: rename
  void ReportErrorToken();   //FIXME: rename
  void NotImplemented(char* what);

  void tau(int code, int p, int q);
  void XtermHack();

  // 

  void reportTerminalType();
  void reportStatus();
  void reportAnswerBack();
  void reportCursorPosition();
  void reportMouseEvent(int ev, int x, int y);
  void reportTerminalParms(int p);

  QString emulation;

protected:

  TEScreen* screen[2];   // 0 = primary, 1 = alternate
  void setScreen(int n); // set `scr' to `screen[n]'

  DECpar currParm;
  DECpar saveParm;
};

#endif // ifndef ANSIEMU_H

/* -------------------------------------------------------------------------- */
/*                                                                            */
/* [emulation.h]          Fundamental Terminal Emulation                      */
/*                                                                            */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>            */
/*                                                                            */
/* This file is part of Konsole - an X terminal for KDE                       */
/*                                                                            */
/* -------------------------------------------------------------------------- */

#ifndef EMULATION_H
#define EMULATION_H

#include "TEWidget.h"
#include "TEScreen.h"
#include <qtimer.h>
#include <stdio.h>

class Emulation : public QObject
{ Q_OBJECT

public:

  Emulation(TEWidget* gui);
  ~Emulation();

public slots: // signals incoming from TEWidget

  virtual void onImageSizeChange(int lines, int columns);
  virtual void onHistoryCursorChange(int cursor);
  virtual void onKeyPress(QKeyEvent*);
//void onMouse(int cb, int cx, int cy);
 
  virtual void clearSelection();
  virtual void onSelectionBegin(const int x, const int y);
  virtual void onSelectionExtend(const int x, const int y);
  virtual void setSelection(const BOOL preserve_line_breaks);

public slots: // signals incoming from data source

  void onRcvBlock(const char* txt,int len);

signals:

  void sndBlock(const char* txt,int len);
  void ImageSizeChanged(int lines, int columns);
  void changeColumns(int columns);
  void changeTitle(int arg, const char* str);

public:

  virtual void onRcvByte(int);
  virtual void setMode  (int) = 0;
  virtual void resetMode(int) = 0;

  virtual void sendString(const char*) = 0;

public:

  virtual void setConnect(bool r);

protected slots: // triggered by timer

  void showBulk();

protected:

  TEWidget* gui;
  TEScreen* scr;       // referes to one `screen'

private:

  QTimer bulk_timer;
  int    bulk_nlcnt;   // bulk newline counter
  char*  SelectedText;
  int    bulk_incnt;   // bulk counter

protected:
  bool   connected;    // communicate with widget

protected:

  void bulkNewline();
  void bulkStart();
  void bulkEnd();

};

#endif // ifndef EMULATION_H

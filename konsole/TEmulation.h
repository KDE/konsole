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
#include <qtextcodec.h>
#include <keytrans.h>

class TEmulation : public QObject
{ Q_OBJECT

public:

  TEmulation(TEWidget* gui);
  ~TEmulation();

public:
  virtual void setHistory(const HistoryType&);
  virtual const HistoryType& history();

public slots: // signals incoming from TEWidget

  virtual void onImageSizeChange(int lines, int columns);
  virtual void onHistoryCursorChange(int cursor);
  virtual void onKeyPress(QKeyEvent*);
 
  virtual void clearSelection();
  virtual void onSelectionBegin(const int x, const int y);
  virtual void onSelectionExtend(const int x, const int y);
  virtual void setSelection(const bool preserve_line_breaks);

public slots: // signals incoming from data source

  void onRcvBlock(const char* txt,int len);

signals:

  void sndBlock(const char* txt,int len);
  void ImageSizeChanged(int lines, int columns);
  void changeColumns(int columns);
  void changeTitle(int arg, const char* str);

public:

  virtual void onRcvChar(int);

  virtual void setMode  (int) = 0;
  virtual void resetMode(int) = 0;

  virtual void sendString(const char*) = 0;

  virtual void setConnect(bool r);
  void setColumns(int columns);

  void setKeymap(int no);
  void setKeymap(const QString &id);
  int keymapNo();
  QString keymap();

protected:

  TEWidget* gui;
  TEScreen* scr;         // referes to one `screen'
  TEScreen* screen[2];   // 0 = primary, 1 = alternate
  void setScreen(int n); // set `scr' to `screen[n]'

  bool   connected;    // communicate with widget

  void setCodec(int c); // codec number, 0 = locale, 1=utf8

  QTextCodec* codec;
  QTextDecoder* decoder;

  KeyTrans* keytrans;

// refreshing related material.
// this is localized in the class.
private slots: // triggered by timer

  void showBulk();

private:

  void bulkNewline();
  void bulkStart();
  void bulkEnd();

private:

  QTimer bulk_timer;
  int    bulk_nlcnt;   // bulk newline counter
  int    bulk_incnt;   // bulk counter


};

#endif // ifndef EMULATION_H

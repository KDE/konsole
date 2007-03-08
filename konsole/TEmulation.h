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

#ifndef EMULATION_H
#define EMULATION_H

// System
#include <stdio.h>

// Qt 
#include <QKeyEvent>
//#include <QPointer>
#include <QTextCodec>
#include <QTextStream>
#include <QTimer>

// Konsole
#include "KeyTrans.h"
#include "TEScreen.h"
#include "TEWidget.h"

class ScreenWindow;

enum { NOTIFYNORMAL=0, NOTIFYBELL=1, NOTIFYACTIVITY=2, NOTIFYSILENCE=3 };

/**
 * Base class for terminal emulation back-ends.
 *
 * The back-end is responsible for decoding the incoming character stream from the terminal
 * program and producing an output image of characters which it then sends to connected
 * views by calling their TEWidget::setImage() method.
 *
 * The emulation also is also responsible for converting input from the connected views such
 * as keypresses and mouse activity into a character string which can be sent
 * to the terminal program.
 *
 * As new lines of text are added to the output, older lines are transferred to a history
 * store, which can be set using the setHistory() method.
 */
class TEmulation : public QObject
{ Q_OBJECT

public:
  
  //Construct a new emulation and adds connects it to the view 'gui'
 
   TEmulation();
  // TEmulation(TEWidget* gui);
  ~TEmulation();

  /** 
   * Adds a new view for this emulation.
   *
   * When the emulation output changes, the view will be updated to display the new output.
   */
  virtual void addView(TEWidget* widget);
  /**
   * Removes a view from this emulation.
   *
   * @p widget will no longer be updated when the emulation output changes.
   */
  virtual void removeView(TEWidget* widget);

  //TODO Document me
  ScreenWindow* createWindow();

public:
  /** Returns the size of the screen image which the emulation produces */
  QSize imageSize();

  /** 
   * Sets the history store used by this emulation.  When new lines
   * are added to the output, older lines at the top of the screen are transferred to a history
   * store.   
   *
   * The number of lines which are kept and the storage location depend on the 
   * type of store.
   */
  virtual void setHistory(const HistoryType&);
  /** Returns the history store used by this emulation.  See setHistory() */
  virtual const HistoryType& history();
  
  /** 
   * Copies the entire output history to a text stream.
   *
   * @param stream The text stream into which the output history will be written.
   * @param decoder A decoder which converts lines of terminal characters with 
   * appearence attributes into output text.  PlainTextDecoder is the most commonly
   * used decoder.
   */
  virtual void writeToStream(QTextStream* stream,TerminalCharacterDecoder* decoder,int startLine,int endLine);
  
  /**
   * Returns the total number of lines
   */ 
  int lines();

  /** Returns the codec used to decode incoming characters.  See setCodec() */
  const QTextCodec *codec() { return m_codec; }
  /** Sets the codec used to decode incoming characters.  */
  void setCodec(const QTextCodec *);

  /** 
   * Initiates a text-search of the output history.  This sets the current search position
   * to the start of the output 
   */
  virtual void findTextBegin();
  /** 
   * Searches the output history for the next occurrence of a particular string or pattern and 
   * scrolls the the screen to the first occurrence found.
   *
   * @param str The string or pattern to search for within the output history
   * @param forward @c true to search forward or @c false to search backwards in the output history.
   * @param caseSensitive @c true for a case-sensitive search or @c false for a case-insensitive search.
   * @param regExp @c true to treat @p str as a regular expression or @c false to treat
   * str as plain text.
   *
   * @returns true if a match is found or false otherwise.
   */ 
  virtual bool findTextNext( const QString &str, bool forward, bool caseSensitive, bool regExp );

public Q_SLOTS: // signals incoming from TEWidget

  /** Change the size of the emulation's image */
  virtual void onImageSizeChange(int lines, int columns);
  
  virtual void onHistoryCursorChange(int cursor);
  
  /** 
   * Interprets a key press event.  
   * This should be reimplemented in sub-classes to emit an appropriate character
   * sequence via sndBlock() 
   */
  virtual void onKeyPress(QKeyEvent*);
 
  /** Clear the current selection */
  //virtual void clearSelection();

  /** Copy the current selection to the clipboard */
  virtual void copySelection();

  /** Begin a new selection at column @p x, row @p y. TODO:  What does columnmode do? */
  //virtual void onSelectionBegin(const int x, const int y, const bool columnmode);
  /** Extend the current selection to column @p x, row @p y. */
  //virtual void onSelectionExtend(const int x, const int y);
  /** Calls the TEWidget::setSelection() method of each associated view with the currently selected text */
  //virtual void setSelection(const bool preserve_line_breaks);
   
  virtual void isBusySelecting(bool busy);
  //virtual void testIsSelected(const int x, const int y, bool &selected);

public Q_SLOTS: // signals incoming from data source

  /** Processes an incoming block of text, triggering an update of connected views if necessary. */
  void onRcvBlock(const char* txt,int len);

Q_SIGNALS:

  void lockPty(bool);
  void useUtf8(bool);
  void sndBlock(const char* txt,int len);
  void ImageSizeChanged(int lines, int columns);
  void changeColumns(int columns);
  void changeColLin(int columns, int lines);
  void changeTitle(int arg, const char* str);
  void notifySessionState(int state);
  void zmodemDetected();
  void changeTabTextColor(int color);

  void updateViews();

public:
  /** 
   * Processes an incoming character.  See onRcvBlock()
   * @p ch A unicode character code. 
   */
  virtual void onRcvChar(int ch);

  virtual void setMode  (int) = 0;
  virtual void resetMode(int) = 0;

  virtual void sendString(const char*) = 0;

  virtual void setConnect(bool r);
  bool isConnected() { return connected; }
  
  bool utf8() { return m_codec->mibEnum() == 106; }

  virtual char getErase();

  virtual void setListenToKeyPress(bool l);
  void setColumns(int columns);

  void setKeymap(int no);
  void setKeymap(const QString &id);
  int keymapNo();
  QString keymap();

  virtual void clearEntireScreen() =0;
  virtual void reset() =0;

protected:

  QList<TEWidget*> _views; //QPointer<TEWidget> > _views;
  QList<ScreenWindow*> _windows;

  //QPointer<TEWidget> gui;
  
  TEScreen* currentScreen;         // referes to one `screen'
  TEScreen* screen[2];   // 0 = primary, 1 = alternate
  void setScreen(int n); // set `scr' to `screen[n]'

  bool   connected;    // communicate with widget
  bool   listenToKeyPress;  // listen to input

  void setCodec(int c); // codec number, 0 = locale, 1=utf8

  //decodes an incoming C-style character stream into a unicode QString using 
  //the current text codec.  (this allows for rendering of non-ASCII characters in text files etc.)
  const QTextCodec* m_codec;
  QTextDecoder* decoder;

  KeyTrans* keytrans; // the keyboard layout

// refreshing related material.
// this is localized in the class.
private Q_SLOTS: 

  // triggered by timer, causes the emulation to send an updated screen image to each
  // view
  void showBulk(); 

private:

  void connectView(TEWidget* widget);

  void bulkStart();

private:

  QTimer bulk_timer1;
  QTimer bulk_timer2;
  
  int    m_findPos;


};

#endif // ifndef EMULATION_H

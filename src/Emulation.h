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
#include <QtGui/QKeyEvent>
//#include <QPointer>
#include <QtCore/QTextCodec>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>

class KeyTrans;

namespace Konsole
{

class HistoryType;
class Screen;
class ScreenWindow;
class TerminalCharacterDecoder;

enum { NOTIFYNORMAL=0, NOTIFYBELL=1, NOTIFYACTIVITY=2, NOTIFYSILENCE=3 };

/**
 * Base class for terminal emulation back-ends.
 *
 * The back-end is responsible for decoding the incoming character stream from the terminal
 * program and producing an output image of characters which it then sends to connected
 * views by calling their TerminalDisplay::setImage() method.
 *
 * The emulation also is also responsible for converting input from the connected views such
 * as keypresses and mouse activity into a character string which can be sent
 * to the terminal program.
 *
 * As new lines of text are added to the output, older lines are transferred to a history
 * store, which can be set using the setHistory() method.
 */
class Emulation : public QObject
{ Q_OBJECT

public:
 
   /** Constructs a new terminal emulation */ 
   Emulation();
  ~Emulation();

  /**
   * Creates a new window onto the output from this emulation.  The contents
   * of the window are then rendered by views which are set to use this window using the
   * TerminalDisplay::setScreenWindow() method.
   */
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
   * appearance attributes into output text.  PlainTextDecoder is the most commonly
   * used decoder.
   */
  virtual void writeToStream(QTextStream* stream,TerminalCharacterDecoder* decoder,int startLine,int endLine);
  
  /**
   * Returns the total number of lines
   */ 
  int lines();

  /** Returns the codec used to decode incoming characters.  See setCodec() */
  const QTextCodec *codec() { return _codec; }
  /** Sets the codec used to decode incoming characters.  */
  void setCodec(const QTextCodec *);

public Q_SLOTS: // signals incoming from TerminalDisplay

  /** Change the size of the emulation's image */
  virtual void onImageSizeChange(int lines, int columns);
  
  /** 
   * Interprets a sequence of characters and sends the result to the terminal.
   * This is equivalent to calling onKeyPress for each character in @p text in succession.
   */
  virtual void sendText(const QString& text) = 0;

  /** 
   * Interprets a key press event.  
   * This should be reimplemented in sub-classes to emit an appropriate character
   * sequence via sndBlock() 
   */
  virtual void onKeyPress(QKeyEvent*);
 
  /** 
   * Converts information about a mouse event into an xterm-compatible escape
   * sequence and emits the character sequence via sendString()
   */
  virtual void onMouse(int buttons, int column, int line, int eventType);
  
  /**
   * Sends a string of characters to the foreground terminal process
   */
  virtual void sendString(const char *) = 0;

  /** Clear the current selection */
  //virtual void clearSelection();

  /** Begin a new selection at column @p x, row @p y. TODO:  What does columnmode do? */
  //virtual void onSelectionBegin(const int x, const int y, const bool columnmode);
  /** Extend the current selection to column @p x, row @p y. */
  //virtual void onSelectionExtend(const int x, const int y);
  /** Calls the TerminalDisplay::setSelection() method of each associated view with the currently selected text */
  //virtual void setSelection(const bool preserve_line_breaks);
   
  virtual void isBusySelecting(bool busy);
  //virtual void testIsSelected(const int x, const int y, bool &selected);


public Q_SLOTS: // signals incoming from data source

  /** Processes an incoming block of text, triggering an update of connected views if necessary. */
  void onReceiveBlock(const char* txt,int len);

Q_SIGNALS:

  void lockPty(bool);
  void useUtf8(bool);
  void sendBlock(const char* txt,int len);
  void setColumnCount(int columns);
  void setScreenSize(int columns, int lines);
  void changeTitle(int arg, const char* str);
  void notifySessionState(int state);
  void zmodemDetected();
  void changeTabTextColor(int color);

  /** 
   * This is emitted when the program running in the shell indicates whether or
   * not it is interested in mouse events.
   *
   * @param usesMouse This will be true if the program wants to be informed about
   * mouse events or false otherwise.
   */
  void programUsesMouse(bool usesMouse);

  /** Emitted to trigger an update of attached views */
  void updateViews();


public:
  /** 
   * Processes an incoming character.  See onReceiveBlock()
   * @p ch A unicode character code. 
   */
  virtual void onReceiveChar(int ch);

  virtual void setMode  (int) = 0;
  virtual void resetMode(int) = 0;

  //REMOVED
  //virtual void setConnect(bool r);
  //bool isConnected() { return connected; }
  
  bool utf8() { Q_ASSERT(_codec); return _codec->mibEnum() == 106; }

  virtual char getErase();

  void setColumns(int columns);

  void setKeymap(const QString &id);
  QString keymap();

  virtual void clearEntireScreen() =0;
  virtual void reset() =0;

protected:
 /** 
   * Sets the active screen
   *
   * @param index 0 to switch to the primary screen, or 1 to switch to the alternate screen
   */
  void setScreen(int index); 

  enum EmulationCodec
  {
      LocaleCodec = 0,
      Utf8Codec   = 1
  };
  void setCodec(EmulationCodec codec); // codec number, 0 = locale, 1=utf8


  QList<ScreenWindow*> _windows;
  
  Screen* _currentScreen;  // pointer to the screen which is currently active, 
                            // this is one of the elements in the screen[] array

  Screen* _screen[2];      // 0 = primary screen ( used by most programs, including the shell
                            //                      scrollbars are enabled in this mode )
                            // 1 = alternate      ( used by vi , emacs etc.
                            //                      scrollbars are not enabled in this mode )
                            
  
  //decodes an incoming C-style character stream into a unicode QString using 
  //the current text codec.  (this allows for rendering of non-ASCII characters in text files etc.)
  const QTextCodec* _codec;
  QTextDecoder* _decoder;

  KeyTrans* _keyTranslator; // the keyboard layout

protected Q_SLOTS:
  /** 
   * Schedules an update of attached views.
   * Repeated calls to bufferedUpdate() in close succession will result in only a single update,
   * much like the Qt buffered update of widgets. 
   */
  void bufferedUpdate();

// refreshing related material.
// this is localized in the class.
private Q_SLOTS: 

  // triggered by timer, causes the emulation to send an updated screen image to each
  // view
  void showBulk(); 

private:

  QTimer _bulkTimer1;
  QTimer _bulkTimer2;
  
};

}

#endif // ifndef EMULATION_H

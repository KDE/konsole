/*
    This file is part of Konsole, an X terminal.
    Copyright (C) 1996 by Matthias Ettrich <ettrich@kde.org>
    Copyright (C) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>
    Copyright (C) 2007 Robert Knight <robertknight@gmail.com> 

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

/*! \class Emulation

    \brief Mediator between TerminalDisplay and Screen.

   This class is responsible to scan the escapes sequences of the terminal
   emulation and to map it to their corresponding semantic complements.
   Thus this module knows mainly about decoding escapes sequences and
   is a stateless device w.rendition.t. the semantics.

   It is also responsible to refresh the TerminalDisplay by certain rules.

   \sa TerminalDisplay \sa Screen

   \par A note on refreshing

   Although the modifications to the current screen image could immediately
   be propagated via `TerminalDisplay' to the graphical surface, we have chosen
   another way here.

   The reason for doing so is twofold.

   First, experiments show that directly displaying the operation results
   in slowing down the overall performance of emulations. Displaying
   individual characters using X11 creates a lot of overhead.

   Second, by using the following refreshing method, the screen operations
   can be completely separated from the displaying. This greatly simplifies
   the programmer's task of coding and maintaining the screen operations,
   since one need not worry about differential modifications on the
   display affecting the operation of concern.

   We use a refreshing algorithm here that has been adoped from rxvt/kvt.

   By this, refreshing is driven by a timer, which is (re)started whenever
   a new bunch of data to be interpreted by the emulation arives at `onReceiveBlock'.
   As soon as no more data arrive for `BULK_TIMEOUT' milliseconds, we trigger
   refresh. This rule suits both bulk display operation as done by curses as
   well as individual characters typed.

   We start also a second time which is never restarted. If repeatedly
   restarting of the first timer could delay continuous output indefinitly,
   the second timer guarantees that the output is refreshed with at least
   a fixed rate.
*/

// System
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Qt
#include <QApplication>
#include <QClipboard>
#include <QClipboard>
#include <QKeyEvent>
#include <QRegExp>
#include <QTextStream>
#include <QThread>

// KDE
#include <kdebug.h>

// Konsole
#include "Screen.h"
#include "TerminalCharacterDecoder.h"
#include "ScreenWindow.h"
#include "Emulation.h"

using namespace Konsole;

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                               Emulation                                  */
/*                                                                           */
/* ------------------------------------------------------------------------- */

#define CNTL(c) ((c)-'@')

/*!
*/

Emulation::Emulation() :
  currentScreen(0),
  listenToKeyPress(false),
  m_codec(0),
  decoder(0),
  keytrans(0),
  m_findPos(-1)
{

  //initialize screens with a default size
  screen[0] = new Screen(40,80);
  screen[1] = new Screen(40,80);
  currentScreen = screen[0];

  QObject::connect(&bulk_timer1, SIGNAL(timeout()), this, SLOT(showBulk()) );
  QObject::connect(&bulk_timer2, SIGNAL(timeout()), this, SLOT(showBulk()) );
   
  setKeymap(0); // Default keymap
}

ScreenWindow* Emulation::createWindow()
{
    ScreenWindow* window = new ScreenWindow();
    window->setScreen(currentScreen);
    _windows << window;

    //FIXME - Used delayed updates when the selection changes
    connect(window , SIGNAL(selectionChanged()),
            this , SLOT(bufferedUpdate()));

    connect(this , SIGNAL(updateViews()),
            window , SLOT(notifyOutputChanged()) );
    return window;
}

/*!
*/

Emulation::~Emulation()
{
  QListIterator<ScreenWindow*> windowIter(_windows);

  while (windowIter.hasNext())
  {
    delete windowIter.next();
  }

  delete screen[0];
  delete screen[1];
  delete decoder;
}

/*! change between primary and alternate screen
*/

void Emulation::setScreen(int n)
{
  Screen *old = currentScreen;
  currentScreen = screen[n&1];
  if (currentScreen != old) 
  {
     old->setBusySelecting(false);

     // tell all windows onto this emulation to switch to the newly active screen
     QListIterator<ScreenWindow*> windowIter(_windows);
     while ( windowIter.hasNext() )
     {
         windowIter.next()->setScreen(currentScreen);
     }
  }
}

void Emulation::setHistory(const HistoryType& t)
{
  screen[0]->setScroll(t);

  showBulk();
}

const HistoryType& Emulation::history()
{
  return screen[0]->getScroll();
}

void Emulation::setCodec(const QTextCodec * qtc)
{
  m_codec = qtc;
  delete decoder;
  decoder = m_codec->makeDecoder();

  emit useUtf8(utf8());
}

void Emulation::setCodec(int c)
{
  setCodec(c ? QTextCodec::codecForName("utf8")
           : QTextCodec::codecForLocale());
}

void Emulation::setKeymap(int no)
{
  keytrans = KeyTrans::find(no);
}

void Emulation::setKeymap(const QString &id)
{
  keytrans = KeyTrans::find(id);
}

QString Emulation::keymap()
{
  return keytrans->id();
}

int Emulation::keymapNo()
{
  return keytrans->numb();
}

// Interpreting Codes ---------------------------------------------------------

/*
   This section deals with decoding the incoming character stream.
   Decoding means here, that the stream is first separated into `tokens'
   which are then mapped to a `meaning' provided as operations by the
   `Screen' class.
*/

/*!
*/

void Emulation::onReceiveChar(int c)
// process application unicode input to terminal
// this is a trivial scanner
{
  c &= 0xff;
  switch (c)
  {
    case '\b'      : currentScreen->BackSpace();                 break;
    case '\t'      : currentScreen->Tabulate();                  break;
    case '\n'      : currentScreen->NewLine();                   break;
    case '\r'      : currentScreen->Return();                    break;
    case 0x07      : emit notifySessionState(NOTIFYBELL);
                     break;
    default        : currentScreen->ShowCharacter(c);            break;
  };
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                             Keyboard Handling                             */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/*!
*/

void Emulation::onKeyPress( QKeyEvent* ev )
{
  if (!listenToKeyPress) return; // someone else gets the keys
  emit notifySessionState(NOTIFYNORMAL);
  
  if (!ev->text().isEmpty())
  { // A block of text
    // Note that the text is proper unicode.
    // We should do a conversion here, but since this
    // routine will never be used, we simply emit plain ascii.
    //emit sendBlock(ev->text().toAscii(),ev->text().length());
    emit sendBlock(ev->text().toUtf8(),ev->text().length());
  }
}

void Emulation::sendString(const char*)
{
    // default implementation does nothing
}

void Emulation::onMouse(int /*buttons*/, int /*column*/, int /*row*/, int /*eventType*/)
{
    // default implementation does nothing
}

// Unblocking, Byte to Unicode translation --------------------------------- --

/*
   We are doing code conversion from locale to unicode first.
TODO: Character composition from the old code.  See #96536
*/

void Emulation::onReceiveBlock(const char* text, int length)
{
	emit notifySessionState(NOTIFYACTIVITY);

	bufferedUpdate();
    	
    QString unicodeText = decoder->toUnicode(text,length);

	//send characters to terminal emulator
	for (int i=0;i<unicodeText.length();i++)
	{
		onReceiveChar(unicodeText[i].unicode());
	}

	//look for z-modem indicator
	//-- someone who understands more about z-modems that I do may be able to move
	//this check into the above for loop?
	for (int i=0;i<length;i++)
	{
		if (text[i] == '\030')
    		{
      			if ((length-i-1 > 3) && (strncmp(text+i+1, "B00", 3) == 0))
      				emit zmodemDetected();
    		}
	}
}

//OLDER VERSION
//This version of onRcvBlock was commented out because
//	a)  It decoded incoming characters one-by-one, which is slow in the current version of Qt (4.2 tech preview)
//	b)  It messed up decoding of non-ASCII characters, with the result that (for example) chinese characters
//	    were not printed properly.
//
//There is something about stopping the decoder if "we get a control code halfway a multi-byte sequence" (see below)
//which hasn't been ported into the newer function (above).  Hopefully someone who understands this better
//can find an alternative way of handling the check.  


/*void Emulation::onRcvBlock(const char *s, int len)
{
  emit notifySessionState(NOTIFYACTIVITY);
  
  bufferedUpdate();
  for (int i = 0; i < len; i++)
  {

    QString result = decoder->toUnicode(&s[i],1);
    int reslen = result.length();

    // If we get a control code halfway a multi-byte sequence
    // we flush the decoder and continue with the control code.
    if ((s[i] < 32) && (s[i] > 0))
    {
       // Flush decoder
       while(!result.length())
          result = decoder->toUnicode(&s[i],1);
       reslen = 1;
       result.resize(reslen);
       result[0] = QChar(s[i]);
    }

    for (int j = 0; j < reslen; j++)
    {
      if (result[j].characterategory() == QChar::Mark_NonSpacing)
         currentScreen->compose(result.mid(j,1));
      else
         onRcvChar(result[j].unicode());
    }
    if (s[i] == '\030')
    {
      if ((len-i-1 > 3) && (strncmp(s+i+1, "B00", 3) == 0))
      	emit zmodemDetected();
    }
  }
}*/

// Selection --------------------------------------------------------------- --

#if 0
void Emulation::onSelectionBegin(const int x, const int y, const bool columnmode) {
  if (!connected) return;
  currentScreen->setSelectionStart( x,y,columnmode);
  showBulk();
}

void Emulation::onSelectionExtend(const int x, const int y) {
  if (!connected) return;
  currentScreen->setSelectionEnd(x,y);
  showBulk();
}

void Emulation::setSelection(const bool preserve_line_breaks) {
  if (!connected) return;
  QString t = currentScreen->selectedText(preserve_line_breaks);
  if (!t.isNull()) 
  {
    QListIterator< TerminalDisplay* > viewIter(_views);

    while (viewIter.hasNext())    
        viewIter.next()->setSelection(t);
  }
}

void Emulation::testIsSelected(const int x, const int y, bool &selected)
{
  if (!connected) return;
  selected=currentScreen->isSelected(x,y);
}

void Emulation::clearSelection() {
  if (!connected) return;
  currentScreen->clearSelection();
  showBulk();
}

#endif 

void Emulation::isBusySelecting(bool busy)
{
  currentScreen->setBusySelecting(busy);
}

void Emulation::writeToStream(QTextStream* stream , 
                               TerminalCharacterDecoder* decoder , 
                               int startLine ,
                               int endLine) 
{
  currentScreen->writeToStream(stream,decoder,startLine,endLine);
}

int Emulation::lines()
{
    // sum number of lines currently on screen plus number of lines in history
    return currentScreen->getLines() + currentScreen->getHistLines();
}

void Emulation::findTextBegin()
{
  m_findPos = -1;
}

bool Emulation::findTextNext( const QString &str, bool forward, bool isCaseSensitive, bool isRegExp )
{
  int pos = -1;
  QString string;

  //text stream to read history into string for pattern or regular expression searching
  QTextStream searchStream(&string);

  PlainTextDecoder decoder;
 
  //setup first and last lines depending on search direction
  int line = 0;
  if (forward)
  	line = (m_findPos == -1 ? 0 : m_findPos+1);
  else
	line = (m_findPos == -1? (currentScreen->getHistLines()+currentScreen->getLines() ):m_findPos-1);

  int lastLine = 0;
  if (forward)
		  lastLine = currentScreen->getHistLines() + currentScreen->getLines();
  else
		  lastLine = 0;
		
  //read through and search history in blocks of 10K lines.
  //this balances the need to retrieve lots of data from the history each time (for efficient searching)
  //without using silly amounts of memory if the history is very large.    
  int delta = forward ? 10000 : -10000;

  //setup case sensitivity and regular expression if enabled
  Qt::CaseSensitivity caseSensitivity = isCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
  QRegExp regExp;

  if (isRegExp)
		  regExp = QRegExp(str,caseSensitivity);
  
  //loop through history in blocks of <delta> lines.
  while ( line != lastLine )
  {
    QApplication::processEvents();

	int endLine = 0;
	if (forward)
		endLine = qMin(line+delta,lastLine);
	else
		endLine = qMax(line+delta,lastLine);
		  
	currentScreen->writeToStream(&searchStream,&decoder, qMin(endLine,line) , qMax(endLine,line) );

	pos = -1;
		
	if (forward)
	{
		if (isRegExp)
				pos = string.indexOf(regExp);
		else
				pos = string.indexOf(str,0,caseSensitivity);
	}
	else
	{
		if (isRegExp)
				pos = string.lastIndexOf(regExp);
		else
				pos = string.lastIndexOf(str, -1, caseSensitivity);
	}

	//if a match is found, position the cursor on that line and update the screen
	if ( pos != -1 )
	{
		//work out how many lines into the current block of text the search result was found
		//- looks a little painful, but it only has to be done once per search.
		m_findPos = line + string.left(pos + 1).count(QChar('\n'));

//TODO - Reimplement moving active view to focus on current match now that
//       that multiple views can show different parts of the screen at the same
//       time.
//
//		if ( m_findPos > currentScreen->getHistLines() )
//				currentScreen->setHistCursor(currentScreen->getHistLines());
//		else
//				currentScreen->setHistCursor(m_findPos);

		//cause target line to be selected
		//currentScreen->getHistoryLine(m_findPos);
	
		//update display to show area of history containing selection	
        showBulk();

		return true;
	}

	//clear the current block of text and move to the next one
	string.clear();	
  	line = endLine;
  }

  return false;
}

// Refreshing -------------------------------------------------------------- --

#define BULK_TIMEOUT1 10
#define BULK_TIMEOUT2 40

/*!
*/
void Emulation::showBulk()
{
    bulk_timer1.stop();
    bulk_timer2.stop();

    emit updateViews();

    currentScreen->resetScrolledLines();
}

void Emulation::bufferedUpdate()
{
   bulk_timer1.setSingleShot(true);
   bulk_timer1.start(BULK_TIMEOUT1);
   if (!bulk_timer2.isActive())
   {
      bulk_timer2.setSingleShot(true);
      bulk_timer2.start(BULK_TIMEOUT2);
   }
}

char Emulation::getErase()
{
  return '\b';
}

void Emulation::setListenToKeyPress(bool l)
{
  listenToKeyPress=l;
}

// ---------------------------------------------------------------------------

/*!  triggered by image size change of the TerminalDisplay `gui'.

    This event is simply propagated to the attached screens
    and to the related serial line.
*/

void Emulation::onImageSizeChange(int lines, int columns)
{
  Q_ASSERT( lines > 0 );
  Q_ASSERT( columns > 0 );

  screen[0]->resizeImage(lines,columns);
  screen[1]->resizeImage(lines,columns);

  bufferedUpdate();
}

QSize Emulation::imageSize()
{
  return QSize(currentScreen->getColumns(), currentScreen->getLines());
}

void Emulation::setColumns(int columns)
{
  //FIXME: this goes strange ways.
  //       Can we put this straight or explain it at least?
  emit setColumnCount(columns);
}

#include "Emulation.moc"

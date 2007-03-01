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

/*! \class TEmulation

    \brief Mediator between TEWidget and TEScreen.

   This class is responsible to scan the escapes sequences of the terminal
   emulation and to map it to their corresponding semantic complements.
   Thus this module knows mainly about decoding escapes sequences and
   is a stateless device w.r.t. the semantics.

   It is also responsible to refresh the TEWidget by certain rules.

   \sa TEWidget \sa TEScreen

   \par A note on refreshing

   Although the modifications to the current screen image could immediately
   be propagated via `TEWidget' to the graphical surface, we have chosen
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
   a new bunch of data to be interpreted by the emulation arives at `onRcvBlock'.
   As soon as no more data arrive for `BULK_TIMEOUT' milliseconds, we trigger
   refresh. This rule suits both bulk display operation as done by curses as
   well as individual characters typed.

   We start also a second time which is never restarted. If repeatedly
   restarting of the first timer could delay continuous output indefinitly,
   the second timer guarantees that the output is refreshed with at least
   a fixed rate.
*/

/* FIXME
   - evtl. the bulk operations could be made more transparent.
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
#include "TEScreen.h"
#include "TEWidget.h"
#include "TerminalCharacterDecoder.h"

#include "TEmulation.h"

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                               TEmulation                                  */
/*                                                                           */
/* ------------------------------------------------------------------------- */

#define CNTL(c) ((c)-'@')

/*!
*/

TEmulation::TEmulation() :
  scr(0),
  connected(false),
  listenToKeyPress(false),
  m_codec(0),
  decoder(0),
  keytrans(0),
  m_findPos(-1)
{

  //initialize screens with a default size
  screen[0] = new TEScreen(40,80);
  screen[1] = new TEScreen(40,80);
  scr = screen[0];

  QObject::connect(&bulk_timer1, SIGNAL(timeout()), this, SLOT(showBulk()) );
  QObject::connect(&bulk_timer2, SIGNAL(timeout()), this, SLOT(showBulk()) );
   
  setKeymap(0); // Default keymap
}

/*!
*/

void TEmulation::connectView(TEWidget* view)
{

  QObject::connect(view,SIGNAL(changedHistoryCursor(int)),
                   this,SLOT(onHistoryCursorChange(int)));
  QObject::connect(view,SIGNAL(keyPressedSignal(QKeyEvent*)),
                   this,SLOT(onKeyPress(QKeyEvent*)));
  QObject::connect(view,SIGNAL(beginSelectionSignal(const int,const int,const bool)),
		   this,SLOT(onSelectionBegin(const int,const int,const bool)) );
  QObject::connect(view,SIGNAL(extendSelectionSignal(const int,const int)),
		   this,SLOT(onSelectionExtend(const int,const int)) );
  QObject::connect(view,SIGNAL(endSelectionSignal(const bool)),
		   this,SLOT(setSelection(const bool)) );
  QObject::connect(view,SIGNAL(copySelectionSignal()),
		   this,SLOT(copySelection()) );
  QObject::connect(view,SIGNAL(clearSelectionSignal()),
		   this,SLOT(clearSelection()) );
  QObject::connect(view,SIGNAL(isBusySelecting(bool)),
		   this,SLOT(isBusySelecting(bool)) );
  QObject::connect(view,SIGNAL(testIsSelected(const int, const int, bool &)),
		   this,SLOT(testIsSelected(const int, const int, bool &)) );
}

/*!
*/

TEmulation::~TEmulation()
{
  delete screen[0];
  delete screen[1];
  delete decoder;
}

/*! change between primary and alternate screen
*/

void TEmulation::setScreen(int n)
{
  TEScreen *old = scr;
  scr = screen[n&1];
  if (scr != old)
     old->setBusySelecting(false);
}

void TEmulation::setHistory(const HistoryType& t)
{
  screen[0]->setScroll(t);

  if (!connected) return;
  showBulk();
}

const HistoryType& TEmulation::history()
{
  return screen[0]->getScroll();
}

void TEmulation::setCodec(const QTextCodec * qtc)
{
  m_codec = qtc;
  delete decoder;
  decoder = m_codec->makeDecoder();
  emit useUtf8(utf8());
}

void TEmulation::setCodec(int c)
{
  setCodec(c ? QTextCodec::codecForName("utf8")
           : QTextCodec::codecForLocale());
}

void TEmulation::setKeymap(int no)
{
  keytrans = KeyTrans::find(no);
}

void TEmulation::setKeymap(const QString &id)
{
  keytrans = KeyTrans::find(id);
}

QString TEmulation::keymap()
{
  return keytrans->id();
}

int TEmulation::keymapNo()
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

void TEmulation::onRcvChar(int c)
// process application unicode input to terminal
// this is a trivial scanner
{
  c &= 0xff;
  switch (c)
  {
    case '\b'      : scr->BackSpace();                 break;
    case '\t'      : scr->Tabulate();                  break;
    case '\n'      : scr->NewLine();                   break;
    case '\r'      : scr->Return();                    break;
    case 0x07      : emit notifySessionState(NOTIFYBELL);
                     break;
    default        : scr->ShowCharacter(c);            break;
  };
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                             Keyboard Handling                             */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/*!
*/

void TEmulation::onKeyPress( QKeyEvent* ev )
{
  if (!listenToKeyPress) return; // someone else gets the keys
  emit notifySessionState(NOTIFYNORMAL);
  if (scr->getHistCursor() != scr->getHistLines() && !ev->text().isEmpty())
    scr->setHistCursor(scr->getHistLines());
  if (!ev->text().isEmpty())
  { // A block of text
    // Note that the text is proper unicode.
    // We should do a conversion here, but since this
    // routine will never be used, we simply emit plain ascii.
    //emit sndBlock(ev->text().toAscii(),ev->text().length());
    emit sndBlock(ev->text().toUtf8(),ev->text().length());
  }
}

// Unblocking, Byte to Unicode translation --------------------------------- --

/*
   We are doing code conversion from locale to unicode first.
TODO: Character composition from the old code.  See #96536
*/

void TEmulation::onRcvBlock(const char* text, int length)
{
	emit notifySessionState(NOTIFYACTIVITY);

	bulkStart();

    int pos = 0;

#warning "knight - Debugging code.  Remember to fix this."

    for ( pos = 0 ; pos < length ; pos += 512 )
    {
    //qDebug() << "input-length: " << length;

	QString unicodeText = decoder->toUnicode(text+pos,qMin(length-pos,512));
	
    //qDebug() << "output-length: " << unicodeText.length();

	//send characters to terminal emulator
	for (int i=0;i<unicodeText.length();i++)
	{
		onRcvChar(unicodeText[i].unicode());
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


/*void TEmulation::onRcvBlock(const char *s, int len)
{
  emit notifySessionState(NOTIFYACTIVITY);
  
  bulkStart();
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
      if (result[j].category() == QChar::Mark_NonSpacing)
         scr->compose(result.mid(j,1));
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

void TEmulation::onSelectionBegin(const int x, const int y, const bool columnmode) {
  if (!connected) return;
  scr->setSelectionStart( x,y,columnmode);
  showBulk();
}

void TEmulation::onSelectionExtend(const int x, const int y) {
  if (!connected) return;
  scr->setSelectionEnd(x,y);
  showBulk();
}

void TEmulation::setSelection(const bool preserve_line_breaks) {
  if (!connected) return;
  QString t = scr->selectedText(preserve_line_breaks);
  if (!t.isNull()) 
  {
    QListIterator< TEWidget* > viewIter(_views);

    while (viewIter.hasNext())    
        viewIter.next()->setSelection(t);
  }
}

void TEmulation::isBusySelecting(bool busy)
{
  if (!connected) return;
  scr->setBusySelecting(busy);
}

void TEmulation::testIsSelected(const int x, const int y, bool &selected)
{
  if (!connected) return;
  selected=scr->isSelected(x,y);
}

void TEmulation::clearSelection() {
  if (!connected) return;
  scr->clearSelection();
  showBulk();
}

void TEmulation::copySelection() {
  if (!connected) return;
  QString t = scr->selectedText(true);
  QApplication::clipboard()->setText(t);
}

void TEmulation::writeToStream(QTextStream* stream , 
                               TerminalCharacterDecoder* decoder , 
                               int startLine ,
                               int endLine) 
{
  scr->writeToStream(stream,decoder,startLine,endLine);
}

int TEmulation::lines()
{
    // sum number of lines currently on screen plus number of lines in history
    return scr->getLines() + scr->getHistLines();
}

void TEmulation::findTextBegin()
{
  m_findPos = -1;
}

bool TEmulation::findTextNext( const QString &str, bool forward, bool isCaseSensitive, bool isRegExp )
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
	line = (m_findPos == -1? (scr->getHistLines()+scr->getLines() ):m_findPos-1);

  int lastLine = 0;
  if (forward)
		  lastLine = scr->getHistLines() + scr->getLines();
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
		  
	scr->writeToStream(&searchStream,&decoder, qMin(endLine,line) , qMax(endLine,line) );

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
		
		if ( m_findPos > scr->getHistLines() )
				scr->setHistCursor(scr->getHistLines());
		else
				scr->setHistCursor(m_findPos);

		//cause target line to be selected
		//scr->getHistoryLine(m_findPos);
	
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

void TEmulation::addView(TEWidget* widget)
{
    Q_ASSERT( !_views.contains(widget) );
    _views << widget;

    connectView(widget);
}

void TEmulation::removeView(TEWidget* widget)
{
    Q_ASSERT( _views.contains(widget) );

    _views.removeAll(widget);

    disconnect(widget);
}

void TEmulation::showBulk()
{
  bulk_timer1.stop();
  bulk_timer2.stop();

  if (connected)
  {
    ca* image = scr->getCookedImage(); 
    QVector<LineProperty> lineProperties = scr->getCookedLineProperties(); 
    QListIterator<TEWidget*> viewIter(_views);

    while (viewIter.hasNext())
    {
        TEWidget* view = viewIter.next();

        QRect scrollRegion;
        scrollRegion.setTop( scr->topMargin() );
        scrollRegion.setBottom( scr->bottomMargin() );
        scrollRegion.setLeft( 0 );
        scrollRegion.setRight( scr->getColumns() );

        // this is an optimisation to avoid the view having to redraw the entire display
        // when the output is simply scrolled by a few lines.
        // scr->scrolledLines() is a guess as to how much the output has scrolled by since
        // the last call to scr->resetScrolledLines().  It does not matter if this count is
        // wrong since the final output from the view will always be the image set with
        // setImage() below.
        view->scrollImage( - scr->scrolledLines() , scrollRegion );
   
        // update the display 
        view->setLineProperties( lineProperties );
	    view->setImage(image,
                  scr->getLines(),
                  scr->getColumns());     
        view->setCursorPos(scr->getCursorX(), scr->getCursorY());	// set XIM position
	    
        //TODO - Update cursor
        view->setScroll(scr->getHistCursor(),scr->getHistLines()); 
    }
  
    scr->resetScrolledLines();  
    free(image);
  }
}

void TEmulation::bulkStart()
{
   bulk_timer1.setSingleShot(true);
   bulk_timer1.start(BULK_TIMEOUT1);
   if (!bulk_timer2.isActive())
   {
      bulk_timer2.setSingleShot(true);
      bulk_timer2.start(BULK_TIMEOUT2);
   }
}

void TEmulation::setConnect(bool c)
{
  connected = c;
  if ( connected)
  {
    showBulk();
  }
}

char TEmulation::getErase()
{
  return '\b';
}

void TEmulation::setListenToKeyPress(bool l)
{
  listenToKeyPress=l;
}

// ---------------------------------------------------------------------------

/*!  triggered by image size change of the TEWidget `gui'.

    This event is simply propagated to the attached screens
    and to the related serial line.
*/

void TEmulation::onImageSizeChange(int lines, int columns)
{
  Q_ASSERT( lines > 0 );
  Q_ASSERT( columns > 0 );

   //kDebug(1211)<<"TEmulation::onImageSizeChange()"<<endl;
  screen[0]->resizeImage(lines,columns);
  screen[1]->resizeImage(lines,columns);
    
  if (!connected) return;
  
  emit ImageSizeChanged(columns, lines);   // propagate event

  // temporary - schedule an update
  //bulkStart(); 
  showBulk();
}

QSize TEmulation::imageSize()
{
  return QSize(scr->getColumns(), scr->getLines());
}

void TEmulation::onHistoryCursorChange(int cursor)
{
  if (!connected) return;
   
  scr->setHistCursor(cursor);

  bulkStart();
}

void TEmulation::setColumns(int columns)
{
  //FIXME: this goes strange ways.
  //       Can we put this straight or explain it at least?
  emit changeColumns(columns);
}

#include "TEmulation.moc"

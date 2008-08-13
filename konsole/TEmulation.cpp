/*
    This file is part of Konsole, an X terminal.
    Copyright (C) 1996 by Matthias Ettrich <ettrich@kde.org>
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

#include "TEmulation.h"
#include "TEWidget.h"
#include "TEScreen.h"
#include <kdebug.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <qregexp.h>
#include <qclipboard.h>

#include <assert.h>

#include "TEmulation.moc"

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                               TEmulation                                  */
/*                                                                           */
/* ------------------------------------------------------------------------- */

#define CNTL(c) ((c)-'@')

/*!
*/

TEmulation::TEmulation(TEWidget* w)
: gui(w),
  scr(0),
  connected(false),
  listenToKeyPress(false),
  m_codec(0),
  decoder(0),
  keytrans(0),
  m_findPos(-1)
{

  screen[0] = new TEScreen(gui->Lines(),gui->Columns());
  screen[1] = new TEScreen(gui->Lines(),gui->Columns());
  scr = screen[0];

  QObject::connect(&bulk_timer1, SIGNAL(timeout()), this, SLOT(showBulk()) );
  QObject::connect(&bulk_timer2, SIGNAL(timeout()), this, SLOT(showBulk()) );
  connectGUI();
  setKeymap(0); // Default keymap
}

/*!
*/

void TEmulation::connectGUI()
{
  QObject::connect(gui,SIGNAL(changedHistoryCursor(int)),
                   this,SLOT(onHistoryCursorChange(int)));
  QObject::connect(gui,SIGNAL(keyPressedSignal(QKeyEvent*)),
                   this,SLOT(onKeyPress(QKeyEvent*)));
  QObject::connect(gui,SIGNAL(beginSelectionSignal(const int,const int,const bool)),
		   this,SLOT(onSelectionBegin(const int,const int,const bool)) );
  QObject::connect(gui,SIGNAL(extendSelectionSignal(const int,const int)),
		   this,SLOT(onSelectionExtend(const int,const int)) );
  QObject::connect(gui,SIGNAL(endSelectionSignal(const bool)),
		   this,SLOT(setSelection(const bool)) );
  QObject::connect(gui,SIGNAL(copySelectionSignal()),
		   this,SLOT(copySelection()) );
  QObject::connect(gui,SIGNAL(clearSelectionSignal()),
		   this,SLOT(clearSelection()) );
  QObject::connect(gui,SIGNAL(isBusySelecting(bool)),
		   this,SLOT(isBusySelecting(bool)) );
  QObject::connect(gui,SIGNAL(testIsSelected(const int, const int, bool &)),
		   this,SLOT(testIsSelected(const int, const int, bool &)) );
}

/*!
*/

void TEmulation::changeGUI(TEWidget* newgui)
{
  if (static_cast<TEWidget *>( gui )==newgui) return;

  if ( gui ) {
    QObject::disconnect(gui,SIGNAL(changedHistoryCursor(int)),
                     this,SLOT(onHistoryCursorChange(int)));
    QObject::disconnect(gui,SIGNAL(keyPressedSignal(QKeyEvent*)),
                     this,SLOT(onKeyPress(QKeyEvent*)));
    QObject::disconnect(gui,SIGNAL(beginSelectionSignal(const int,const int,const bool)),
                     this,SLOT(onSelectionBegin(const int,const int,const bool)) );
    QObject::disconnect(gui,SIGNAL(extendSelectionSignal(const int,const int)),
                     this,SLOT(onSelectionExtend(const int,const int)) );
    QObject::disconnect(gui,SIGNAL(endSelectionSignal(const bool)),
                     this,SLOT(setSelection(const bool)) );
    QObject::disconnect(gui,SIGNAL(copySelectionSignal()),
                     this,SLOT(copySelection()) );
    QObject::disconnect(gui,SIGNAL(clearSelectionSignal()),
                     this,SLOT(clearSelection()) );
    QObject::disconnect(gui,SIGNAL(isBusySelecting(bool)),
                     this,SLOT(isBusySelecting(bool)) );
    QObject::disconnect(gui,SIGNAL(testIsSelected(const int, const int, bool &)),
                     this,SLOT(testIsSelected(const int, const int, bool &)) );
  }
  gui=newgui;
  connectGUI();
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
   Decoding means here, that the stream is first seperated into `tokens'
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
    emit sndBlock(ev->text().ascii(),ev->text().length());
  }
  else if (ev->ascii()>0)
  { unsigned char c[1];
    c[0] = ev->ascii();
    emit sndBlock((char*)c,1);
  }
}

// Unblocking, Byte to Unicode translation --------------------------------- --

/*
   We are doing code conversion from locale to unicode first.
*/

void TEmulation::onRcvBlock(const char *s, int len)
{
  emit notifySessionState(NOTIFYACTIVITY);

  bulkStart();

  QString r;
  int i, l;

  for (i = 0; i < len; i++)
  {
    // If we get a control code halfway a multi-byte sequence
    // we flush the decoder and continue with the control code.
    if ((unsigned char) s[i] < 32)
    {
       if (!r.length()) {
         QString tmp;
         // Flush decoder
         while(!tmp.length())
             tmp = decoder->toUnicode(" ",1);
       }

       onRcvChar((unsigned char) s[i]);

       if (s[i] == '\030' && (len-i-1 > 3) && (strncmp(s+i+1, "B00", 3) == 0))
         emit zmodemDetected();

       continue;
    }

    // Otherwise, bulk decode until the next control code
    for(l = i; l < len; l++)
      if ((unsigned char) s[l+1] < 32)
         break;

    r = decoder->toUnicode(&s[i],l-i+1);
    int reslen = r.length();

    for (int j = 0; j < reslen; j++)
    {
      if (r[j].category() == QChar::Mark_NonSpacing)
         scr->compose(r.mid(j,1));
      else
         onRcvChar(r[j].unicode());
    }
    i = l;
  }
}

// Selection --------------------------------------------------------------- --

void TEmulation::onSelectionBegin(const int x, const int y, const bool columnmode) {
  if (!connected) return;
  scr->setSelBeginXY(x,y,columnmode);
  showBulk();
}

void TEmulation::onSelectionExtend(const int x, const int y) {
  if (!connected) return;
  scr->setSelExtentXY(x,y);
  showBulk();
}

void TEmulation::setSelection(const bool preserve_line_breaks) {
  if (!connected) return;
  QString t = scr->getSelText(preserve_line_breaks);
  if (!t.isNull()) gui->setSelection(t);
}

void TEmulation::isBusySelecting(bool busy)
{
  if (!connected) return;
  scr->setBusySelecting(busy);
}

void TEmulation::testIsSelected(const int x, const int y, bool &selected)
{
  if (!connected) return;
  selected=scr->testIsSelected(x,y);
}

void TEmulation::clearSelection() {
  if (!connected) return;
  scr->clearSelection();
  showBulk();
}

void TEmulation::copySelection() {
  if (!connected) return;
  QString t = scr->getSelText(true);
  QApplication::clipboard()->setText(t);
}

void TEmulation::streamHistory(QTextStream* stream) {
  scr->streamHistory(stream);
}

void TEmulation::findTextBegin()
{
  m_findPos = -1;
}

bool TEmulation::findTextNext( const QString &str, bool forward, bool caseSensitive, bool regExp )
{
  int pos = -1;
  QString string;

  if (forward) {
    for (int i = (m_findPos==-1?0:m_findPos+1); i<(scr->getHistLines()+scr->getLines()); i++) {
      string = scr->getHistoryLine(i);
      if (regExp)
        pos = string.find( QRegExp(str,caseSensitive) );
      else
        pos = string.find(str, 0, caseSensitive);
      if(pos!=-1) {
        m_findPos=i;
        if(i>scr->getHistLines())
          scr->setHistCursor(scr->getHistLines());
        else
          scr->setHistCursor(i);
        showBulk();
	return true;
      }
    }
  }
  else { // searching backwards
    for(int i = (m_findPos==-1?(scr->getHistLines()+scr->getLines()):m_findPos-1); i>=0; i--) {
      string = scr->getHistoryLine(i);
      if (regExp)
        pos = string.find( QRegExp(str,caseSensitive) );
      else
        pos = string.find(str, 0, caseSensitive);
      if(pos!=-1) {
        m_findPos=i;
        if(i>scr->getHistLines())
          scr->setHistCursor(scr->getHistLines());
        else
          scr->setHistCursor(i);
        showBulk();
	return true;
      }
    }
  }

  return false;
}

// Refreshing -------------------------------------------------------------- --

#define BULK_TIMEOUT1 10
#define BULK_TIMEOUT2 40

/*!
*/

void TEmulation::showBulk()
{
  bulk_timer1.stop();
  bulk_timer2.stop();

  if (connected)
  {
    ca* image = scr->getCookedImage();    // get the image
    gui->setImage(image,
                  scr->getLines(),
                  scr->getColumns());     // actual refresh
    gui->setCursorPos(scr->getCursorX(), scr->getCursorY());	// set XIM position
    free(image);
    //FIXME: check that we do not trigger other draw event here.
    gui->setLineWrapped( scr->getCookedLineWrapped() );
    //kdDebug(1211)<<"TEmulation::showBulk(): setScroll()"<<endl;
    gui->setScroll(scr->getHistCursor(),scr->getHistLines());
    //kdDebug(1211)<<"TEmulation::showBulk(): setScroll() done"<<endl;
  }
}

void TEmulation::bulkStart()
{
   bulk_timer1.start(BULK_TIMEOUT1,true);
   if (!bulk_timer2.isActive())
      bulk_timer2.start(BULK_TIMEOUT2, true);
}

void TEmulation::setConnect(bool c)
{
   //kdDebug(1211)<<"TEmulation::setConnect()"<<endl;
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
  assert( lines > 0 && columns > 0 );

   //kdDebug(1211)<<"TEmulation::onImageSizeChange()"<<endl;
  screen[0]->resizeImage(lines,columns);
  screen[1]->resizeImage(lines,columns);
    
  if (!connected) return;
   //kdDebug(1211)<<"TEmulation::onImageSizeChange() showBulk()"<<endl;
  showBulk();
   //kdDebug(1211)<<"TEmulation::onImageSizeChange() showBulk() done"<<endl;
  emit ImageSizeChanged(columns, lines);   // propagate event
   //kdDebug(1211)<<"TEmulation::onImageSizeChange() done"<<endl;
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


/* -------------------------------------------------------------------------- */
/*                                                                            */
/* [TEmulation.cpp]        Terminal Emulation Decoder                         */
/*                                                                            */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>            */
/*                                                                            */
/* This file is part of Konsole - an X terminal for KDE                       */
/*                                                                            */
/* -------------------------------------------------------------------------- */

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
   (BULK_TIMEOUT < 1000 / max characters received from keyboard per second).

   Additionally, we trigger refreshing by newlines comming in to make visual
   snapshots of lists as produced by `cat', `ls' and likely programs, thereby
   producing the illusion of a permanent and immediate display operation.

   As a sort of catch-all needed for cases where none of the above
   conditions catch, the screen refresh is also triggered by a count
   of incoming bulks (`bulk_incnt').
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
#include <qkeycode.h>

#include <assert.h>

#include "TEmulation.moc"

#ifndef HERE
#define HERE printf("%s(%d): here\n",__FILE__,__LINE__)
#endif

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
  codec(0),
  decoder(0),
  keytrans(0),
  bulk_nlcnt(0),
  bulk_incnt(0)
{

  screen[0] = new TEScreen(gui->Lines(),gui->Columns());
  screen[1] = new TEScreen(gui->Lines(),gui->Columns());
  scr = screen[0];

  QObject::connect(&bulk_timer, SIGNAL(timeout()), this, SLOT(showBulk()) );
  QObject::connect(gui,SIGNAL(changedImageSizeSignal(int,int)),
                   this,SLOT(onImageSizeChange(int,int)));
  QObject::connect(gui,SIGNAL(changedHistoryCursor(int)),
                   this,SLOT(onHistoryCursorChange(int)));
  QObject::connect(gui,SIGNAL(keyPressedSignal(QKeyEvent*)),
                   this,SLOT(onKeyPress(QKeyEvent*)));
  QObject::connect(gui,SIGNAL(beginSelectionSignal(const int,const int)),
		   this,SLOT(onSelectionBegin(const int,const int)) );
  QObject::connect(gui,SIGNAL(extendSelectionSignal(const int,const int)),
		   this,SLOT(onSelectionExtend(const int,const int)) );
  QObject::connect(gui,SIGNAL(endSelectionSignal(const bool)),
		   this,SLOT(setSelection(const bool)) );
  QObject::connect(gui,SIGNAL(clearSelectionSignal()),
		   this,SLOT(clearSelection()) );
  setKeymap(0); // Default keymap
}

/*!
*/

TEmulation::~TEmulation()
{
  delete screen[0];
  delete screen[1];
  delete decoder;
  bulk_timer.stop();
}

/*! change between primary and alternate screen
*/

void TEmulation::setScreen(int n)
{
  scr = screen[n&1];
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

void TEmulation::setCodec(int c)
{
  //FIXME: check whether we have to free codec
  codec = c ? QTextCodec::codecForName("utf8")
            : QTextCodec::codecForLocale();
  delete decoder;
  decoder = codec->makeDecoder();
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
    case 0x07      : gui->Bell();                      break;
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
  if (!connected) return; // someone else gets the keys
  if (scr->getHistCursor() != scr->getHistLines());
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
  bulkStart();
  bulk_incnt += 1;
  for (int i = 0; i < len; i++)
  {
    QString result = decoder->toUnicode(&s[i],1);
    int reslen = result.length();
    for (int j = 0; j < reslen; j++)
      onRcvChar(result[j].unicode());
    if (s[i] == '\n') bulkNewline();
  }
  bulkEnd();
}

// Selection --------------------------------------------------------------- --

void TEmulation::onSelectionBegin(const int x, const int y) {
  if (!connected) return;
  scr->setSelBeginXY(x,y);
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

void TEmulation::clearSelection() {
  if (!connected) return;
  scr->clearSelection();
  showBulk();
}

// Refreshing -------------------------------------------------------------- --

#define BULK_TIMEOUT 20

/*!
   called when \n comes in. Evtl. triggers showBulk at endBulk
*/

void TEmulation::bulkNewline()
{
  bulk_nlcnt += 1;
  bulk_incnt = 0;  // reset bulk counter since `nl' rule applies
}

/*!
*/

void TEmulation::showBulk()
{
  bulk_nlcnt = 0;                       // reset bulk newline counter
  bulk_incnt = 0;                       // reset bulk counter
  if (connected)
  {
    ca* image = scr->getCookedImage();    // get the image
    gui->setImage(image,
                  scr->getLines(),
                  scr->getColumns());     // actual refresh
    free(image);
    //FIXME: check that we do not trigger other draw event here.
    //kdDebug(1211)<<"TEmulation::showBulk(): setScroll()"<<endl;
    gui->setScroll(scr->getHistCursor(),scr->getHistLines());
    //kdDebug(1211)<<"TEmulation::showBulk(): setScroll() done"<<endl;
  }
}

void TEmulation::bulkStart()
{
  if (bulk_timer.isActive()) bulk_timer.stop();
}

void TEmulation::bulkEnd()
{
  if ( bulk_nlcnt > gui->Lines() || bulk_incnt > 20 )
    showBulk();                         // resets bulk_??cnt to 0, too.
  else
    bulk_timer.start(BULK_TIMEOUT,TRUE);
}

void TEmulation::setConnect(bool c)
{
   //kdDebug(1211)<<"TEmulation::setConnect()"<<endl;
  connected = c;
  if ( connected)
  {
    //kdDebug(1211)<<"TEmulation::setConnect() onImageSize..."<<endl;
    onImageSizeChange(gui->Lines(), gui->Columns());
    //kdDebug(1211)<<"TEmulation::setConnect() showBulk()"<<endl;
    showBulk();
    //kdDebug(1211)<<"TEmulation::setConnect() done"<<endl;
  }
  else
  {
    scr->clearSelection();
  }
}

// ---------------------------------------------------------------------------

/*!  triggered by image size change of the TEWidget `gui'.

    This event is simply propagated to the attached screens
    and to the related serial line.
*/

void TEmulation::onImageSizeChange(int lines, int columns)
{
   //kdDebug(1211)<<"TEmulation::onImageSizeChange()"<<endl;
  if (!connected) return;
  screen[0]->resizeImage(lines,columns);
  screen[1]->resizeImage(lines,columns);
   //kdDebug(1211)<<"TEmulation::onImageSizeChange() showBulk()"<<endl;
  showBulk();
   //kdDebug(1211)<<"TEmulation::onImageSizeChange() showBulk() done"<<endl;
  emit ImageSizeChanged(lines,columns);   // propagate event to serial line
   //kdDebug(1211)<<"TEmulation::onImageSizeChange() done"<<endl;
}

void TEmulation::onHistoryCursorChange(int cursor)
{
  if (!connected) return;
  scr->setHistCursor(cursor);
  showBulk();
}

void TEmulation::setColumns(int columns)
{
  //FIXME: this goes strange ways.
  //       Can we put this straight or explain it at least?
  emit changeColumns(columns);
}

/* ----------------------------------------------------------------------- *
 * [konsole_child.cpp]           Konsole                                   *
 * ----------------------------------------------------------------------- *
 * This file is part of Konsole, an X terminal.                            *
 *                                                                         *
 * Copyright (c) 2002 by Stephan Binner <binner@kde.org>                   *
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <klineeditdlg.h>
#include <qwmatrix.h> 

#include "konsole_child.h"
#include <netwm.h>

KonsoleChild::KonsoleChild(TESession* _se, int columns, int lines, int scrollbar_location, int frame_style,
                           ColorSchema* schema,QFont font,int bellmode,QString wordcharacters,
                           bool blinkingCursor, bool ctrlDrag, bool terminalSizeHint, int lineSpacing,
                           bool cutToBeginningOfLine, bool _allowResize):KMainWindow()
,session_terminated(false)
,wallpaperSource(0)
,se(_se)
,allowResize(_allowResize)
{
  te = new TEWidget(this);
  te->setVTFont(font);

  setCentralWidget(te);
  rootxpm = new KRootPixmap(te);

  te->setFocus();

  te->setWordCharacters(wordcharacters);
  te->setBlinkingCursor(blinkingCursor);
  te->setCtrlDrag(ctrlDrag);
  te->setTerminalSizeHint(terminalSizeHint);
  te->setTerminalSizeStartup(false);
  te->setLineSpacing(lineSpacing);
  te->setBellMode(bellmode);
  te->setMinimumSize(150,70);
  te->setCutToBeginningOfLine(cutToBeginningOfLine);
  te->setScrollbarLocation(scrollbar_location);
  te->setFrameStyle(frame_style);

  toolBar()->hide();

  setColLin(columns, lines);

  session_transparent=false;
  if (schema) {
    te->setColorTable(schema->table()); //FIXME: set twice here to work around a bug
    if (schema->useTransparency()) {
      rootxpm->setFadeEffect(schema->tr_x(), QColor(schema->tr_r(), schema->tr_g(), schema->tr_b()));
      rootxpm->start();
      rootxpm->repaint(true);
      session_transparent=true;
    } else {
      rootxpm->stop();
      pixmap_menu_activated(schema->alignment(),schema->imagePath());
    }
    te->setColorTable(schema->table());
  }

  updateTitle();

  connect( se,SIGNAL(done(TESession*,int)),
           this,SLOT(doneSession(TESession*,int)) );
  connect( te, SIGNAL(configureRequest(TEWidget*, int, int, int)),
           this, SLOT(configureRequest(TEWidget*, int, int, int)) );

  connect( se,SIGNAL(updateTitle()), this,SLOT(updateTitle()) );
  connect( se,SIGNAL(renameSession(TESession*,const QString&)), this,SLOT(slotRenameSession(TESession*,const QString&)) );
  connect( se,SIGNAL(restoreAllListenToKeyPress()), this,SLOT(restoreAllListenToKeyPress()) );
  connect(se->getEmulation(),SIGNAL(changeColumns(int)), this,SLOT(changeColumns(int)) );

  connect( kapp,SIGNAL(backgroundChanged(int)),this, SLOT(slotBackgroundChanged(int)));

  // Send Signal Menu -------------------------------------------------------------
  KPopupMenu* m_signals = new KPopupMenu(this);
  m_signals->insertItem( i18n( "&Suspend Task" )   + " (STOP)", SIGSTOP);
  m_signals->insertItem( i18n( "&Continue Task" )  + " (CONT)", SIGCONT);
  m_signals->insertItem( i18n( "&Hangup" )         + " (HUP)", SIGHUP);
  m_signals->insertItem( i18n( "&Interrupt Task" ) + " (INT)", SIGINT);
  m_signals->insertItem( i18n( "&Terminate Task" ) + " (TERM)", SIGTERM);
  m_signals->insertItem( i18n( "&Kill Task" )      + " (KILL)", SIGKILL);
  connect(m_signals, SIGNAL(activated(int)), SLOT(sendSignal(int)));

  m_rightButton = new KPopupMenu(this);
  KActionCollection* actions = new KActionCollection(this);

  KAction *pasteClipboard = new KAction(i18n("&Paste"), "editpaste", 0,
                                        te, SLOT(pasteClipboard()), actions);
  pasteClipboard->plug(m_rightButton);
  m_rightButton->insertItem(i18n("&Send Signal"), m_signals);

  m_rightButton->insertSeparator();
  KAction *attachSession = new KAction(i18n("&Attach Session"), 0, this,
                                       SLOT(attachSession()), actions);
  attachSession->plug(m_rightButton);
  KAction *renameSession = new KAction(i18n("&Rename Session..."), 0, this,
                                       SLOT(renameSession()), actions);
  renameSession->plug(m_rightButton);

  m_rightButton->insertSeparator();
  KAction *closeSession = new KAction(i18n("&Close Session"), "fileclose", 0, this,
                                      SLOT(closeSession()), actions);
  closeSession->plug(m_rightButton );
  if (KGlobalSettings::insertTearOffHandle())
    m_rightButton->insertTearOffHandle();
}

void KonsoleChild::run() {
   te->currentSession=se;
   se->changeWidget(te);
   se->setConnect(true);

   kWinModule = new KWinModule();
   connect( kWinModule,SIGNAL(currentDesktopChanged(int)), this,SLOT(currentDesktopChanged(int)) );
}

void KonsoleChild::updateTitle()
{
  setCaption( se->fullTitle() );
  setIconText( se->IconText() );
}

void KonsoleChild::slotRenameSession(TESession*, const QString &)
{
  updateTitle();
}

void KonsoleChild::restoreAllListenToKeyPress()
{
  se->setListenToKeyPress(TRUE);
}

void KonsoleChild::setColLin(int columns, int lines)
{
  if ((columns==0) || (lines==0))
    resize(sizeForCentralWidgetSize(te->calcSize(80,24)));
  else
    resize(sizeForCentralWidgetSize(te->calcSize(columns, lines)));
}

void KonsoleChild::changeColumns(int columns)
{
  if (allowResize) {
    setColLin(columns,te->Lines());
    te->update();
  }
}

KonsoleChild::~KonsoleChild()
{
  se->setConnect(FALSE);
  delete te;
  if (session_terminated) {
    delete se;
    se=NULL;
  }
  emit doneChild(this,se);

  if( kWinModule )
    delete kWinModule;
  kWinModule = 0;
}

void KonsoleChild::configureRequest(TEWidget* te, int, int x, int y)
{
  m_rightButton->popup(te->mapToGlobal(QPoint(x,y)));
}

void KonsoleChild::doneSession(TESession*,int)
{
  se->setConnect(FALSE);
  session_terminated=true;
  delete this;
}

void KonsoleChild::sendSignal(int sn)
{
  se->sendSignal(sn);
}

void KonsoleChild::attachSession()
{
  delete this;
}

void KonsoleChild::renameSession() {
  QString name = se->Title();
  KLineEditDlg dlg(i18n("Session name"),name, this);
  dlg.setCaption(i18n("Rename Session"));
  if (dlg.exec()) {
    se->setTitle(dlg.text());
    updateTitle();
  }
}

void KonsoleChild::closeSession()
{
  se->sendSignal(SIGHUP);
}

void KonsoleChild::pixmap_menu_activated(int item,QString pmPath)
{
  if (item <= 1) pmPath = "";
  QPixmap pm(pmPath);
  if (pm.isNull()) {
    pmPath = "";
    item = 1;
    te->setBackgroundColor(te->getDefaultBackColor());
    return;
  }
  // FIXME: respect scrollbar (instead of te->size)
  switch (item)
  {
    case 1: // none
    case 2: // tile
            te->setBackgroundPixmap(pm);
    break;
    case 3: // center
            { QPixmap bgPixmap;
              bgPixmap.resize(te->size());
              bgPixmap.fill(te->getDefaultBackColor());
              bitBlt( &bgPixmap, ( te->size().width() - pm.width() ) / 2,
                                ( te->size().height() - pm.height() ) / 2,
                      &pm, 0, 0,
                      pm.width(), pm.height() );

              te->setBackgroundPixmap(bgPixmap);
            }
    break;
    case 4: // full
            {
              float sx = (float)te->size().width() / pm.width();
              float sy = (float)te->size().height() / pm.height();
              QWMatrix matrix;
              matrix.scale( sx, sy );
              te->setBackgroundPixmap(pm.xForm( matrix ));
            }
    break;
  }
}

void KonsoleChild::slotBackgroundChanged(int desk)
{
  // Only update rootxpm if window is visible on current desktop
  NETWinInfo info( qt_xdisplay(), winId(), qt_xrootwin(), NET::WMDesktop );

  if (session_transparent && info.desktop()==desk && (0 != rootxpm)) {
    //KONSOLEDEBUG << "Wallpaper changed on my desktop, " << desk << ", repainting..." << endl;
    //Check to see if we are on the current desktop. If not, delay the repaint
    //by setting wallpaperSource to 0. Next time our desktop is selected, we will
    //automatically update because we are saying "I don't have the current wallpaper"
    NETRootInfo rootInfo( qt_xdisplay(), NET::CurrentDesktop );
    rootInfo.activate();
    if( rootInfo.currentDesktop() == info.desktop() ) {
       //We are on the current desktop, go ahead and update
       //KONSOLEDEBUG << "My desktop is current, updating..." << endl;
       wallpaperSource = desk;
       rootxpm->repaint(true);
    }
    else {
       //We are not on the current desktop, mark our wallpaper source 'stale'
       //KONSOLEDEBUG << "My desktop is NOT current, delaying update..." << endl;
       wallpaperSource = 0;
    }
  }
}

void KonsoleChild::currentDesktopChanged(int desk) {
   //Get window info
   NETWinInfo info( qt_xdisplay(), winId(), qt_xrootwin(), NET::WMDesktop );
   bool bNeedUpdate = false;
 
   if( info.desktop()==NETWinInfo::OnAllDesktops ) {
      //This is a sticky window so it will always need updating
      bNeedUpdate = true;
   }
   else if( (info.desktop() == desk) && (wallpaperSource != desk) ) {
      bNeedUpdate = true;
   }
   else {
      //We are not sticky and already have the wallpaper for our desktop
      return;
   }

   //This window is transparent, update the root pixmap
   if( bNeedUpdate && session_transparent ) {
      wallpaperSource = desk;
      rootxpm->repaint(true);
   }
}

#include "konsole_child.moc"

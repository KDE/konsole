/* ---------------------------------------------------------------------- */
/*                                                                        */
/* [main.C]                        Konsole                                */
/*                                                                        */
/* ---------------------------------------------------------------------- */
/*                                                                        */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>        */
/*                                                                        */
/* This file is part of Konsole, an X terminal.                           */
/*                                                                        */
/* The material contained in here more or less directly orginates from    */
/* kvt, which is copyright (c) 1996 by Matthias Ettrich <ettrich@kde.org> */
/*                                                                        */
/* ---------------------------------------------------------------------- */


/*! \class Konsole

    \brief Konsole's main class and program

    The class Konsole handles the application level. Mainly, it is responsible
    for the configuration, taken from several files, from the command line
    and from the user. It hardly does anything interesting.
*/

/*TODO:
  - allow to set coded
  - officially declare this file to be hacked to death. ;^)
  - merge into konsole_part.
*/

/*STATE:

  konsole/kwin session management, parts stuff, config, menues
  are all in bad need for a complete rewrite.

  While the emulation core (TEmulation, TEVt102, TEScreen, TEWidget)
  are pretty stable, the upper level material has certainly drifted.

  Everything related to Sessions, Configuration has to be redesigned.
  It seems that the konsole now falls apart into individual sessions
  and a session manager.

Time to start a requirement list.

  - Rework the Emulation::setConnect logic.
    Together with session changing (Shift-Left/Right, Ctrl-D) it allows
    key events to propagate to other sessions.

  - Get rid of the unconfigurable, uncallable initial "Konsole" session.
    Leads to code replication in konsole_part and other issues. Related
    to the booting of konsole, thus matter of redesign.
*/

/*FIXME:
  - All the material in here badly sufferes from the fact that the
    configuration can originate from many places, so all is duplicated
    and falls out of service. Especially the command line is badly broken.
    The sources are:
    - command line
    - menu
    - configuration files
    - other events (e.g. resizing)
    We have to find a single-place method to better maintain this.
  - In general, the material contained in here and in session.C
    should be rebalanced. Much more material now comes from configuration
    files and the overall routines should better respect this.
  - Font+Size list should go to a configuration file, too.
  - Controling the widget is currently done by individual attributes.
    This lead to quite some amount of flicker when a whole bunch of
    attributes has to be set, e.g. in session swapping.
*/

#include"config.h"

#include <qdir.h>
#include <qevent.h>
#include <qdragobject.h>
#include <qobjectlist.h>
#include <ktoolbarbutton.h>

#include <stdio.h>
#include <stdlib.h>

#include <kfontdialog.h>
#include <kglobal.h>
#include <kstddirs.h>
#include <kconfig.h>
#include <kurl.h>
#include <qpainter.h>
#include <kmenubar.h>
#include <kmessagebox.h>
#include <krootpixmap.h>
#include <kaction.h>
#include <kstdaction.h>
#include <kpopupmenu.h>
#include <klineeditdlg.h>
#include <kdebug.h>

#include <qfontmetrics.h>

#include <klocale.h>
#include <sys/wait.h>
#include <assert.h>

#include <kiconloader.h>

#include "konsole.h"
#include "keytrans.h"


#define HERE printf("%s(%d): here\n",__FILE__,__LINE__)


class KonsoleFontSelectAction : public KSelectAction {
public:
    KonsoleFontSelectAction(const QString &text, int accel,
                            const QObject* receiver, const char* slot,
                            QObject* parent, const char* name = 0 )
        : KSelectAction(text, accel, receiver, slot, parent, name) {}

    virtual void slotActivated( int index );
};

void KonsoleFontSelectAction::slotActivated(int index) {
    // emit even if it's already activated
    if (currentItem() == index) {
        KSelectAction::slotActivated();
        return;
    } else {
        KSelectAction::slotActivated(index);
    }
}

template class QPtrDict<TESession>;
template class QIntDict<KSimpleConfig>;
template class QPtrDict<KRadioAction>;


const char *fonts[] = {
 "6x13",  // FIXME: "fixed" used in favor of this
 "5x7",   // tiny font, never used
 "6x10",  // small font
 "7x13",  // medium
 "9x15",  // large
 "10x20", // huge
 "-misc-console-medium-r-normal--16-160-72-72-c-160-iso10646-1", // "Linux"
 "-misc-fixed-medium-r-normal--15-140-75-75-c-90-iso10646-1",    // "Unicode"
 };
#define TOPFONT ((sizeof(fonts)/sizeof(char*))-1)

Konsole::Konsole(const char* name,
                 const char* _pgm, QStrList & _args,
                 int histon, bool toolbaron) : KMainWindow(0, name), pgm(_pgm), args(_args)
{
  no2command.setAutoDelete(true);
  session_no = 0;
  cmd_serial = 0;

  se = 0L;
  rootxpm = 0L;
  menubar = menuBar();
  b_scroll = histon;
  b_fullscreen = FALSE;
  n_keytab = 0;
  n_render = 0;


  // create terminal emulation framework ////////////////////////////////////

  te = new TEWidget(this);
  te->setMinimumSize(150,70);    // allow resizing, cause resize in TEWidget

  // Transparency handler ///////////////////////////////////////////////////
  rootxpm = new KRootPixmap(te);

  // create applications /////////////////////////////////////////////////////

  setCentralWidget(te);

  makeMenu();
  setDockEnabled( toolBar(), QMainWindow::Left, FALSE );
  setDockEnabled( toolBar(), QMainWindow::Right, FALSE );
  toolBar()->setFullSize( TRUE );
   //  toolBar()->setIconText( KToolBar::IconTextRight);

  // load session commands ///////////////////////////////////////////////////

  loadSessionCommands();
  m_file->insertSeparator();
  m_file->insertItem( i18n("&Quit"), this, SLOT(close()));

  // load schema /////////////////////////////////////////////////////////////

  curr_schema = 0;
  ColorSchema::loadAllSchemas();
  //FIXME: sort
  for (int i = 0; i < ColorSchema::count(); i++)
  { ColorSchema* s = ColorSchema::find(i);
    assert( s );
    m_schema->insertItem(s->title,s->numb);
  }

  // load keymaps ////////////////////////////////////////////////////////////

  KeyTrans::loadAll();
  //FIXME: sort
  for (int i = 0; i < KeyTrans::count(); i++)
  { KeyTrans* s = KeyTrans::find(i);
    assert( s );
    m_keytab->insertItem(s->hdr,s->numb);
  }
  m_keytab->setItemChecked(0,TRUE);

  // set global options ///////////////////////////////////////////////////////

  // construct initial session ///////////////////////////////////////////////
//FIXME: call newSession here, somehow, instead the stuff below.
  // Please do it ! It forces to duplicate code... (David)

  TESession* initial = new TESession(this,te,pgm,args,"xterm");

  connect( initial,SIGNAL(done(TESession*,int)),
           this,SLOT(doneSession(TESession*,int)) );
  connect( te, SIGNAL(configureRequest(TEWidget*, int, int, int)),
           this, SLOT(configureRequest(TEWidget*,int,int,int)) );

  title = (args.count() && (kapp->caption() == PACKAGE))
        ? QString(args.at(0))  // program executed in the title bar
        : kapp->caption();  // `konsole' or -caption
  initial->setTitle(title);
  //initial->setHistory(b_scroll); //FIXME: take from schema
//  setHistory(b_scroll); //FIXME: take from schema

  addSession(initial);

  // read and apply default values ///////////////////////////////////////////
  resize(321, 321); // Dummy.
  QSize currentSize = size();
  KConfig * config = KGlobal::config();
  config->setGroup("options");
  applyMainWindowSettings(config);
  if (currentSize != size())
     defaultSize = size();
  config->setGroup("options");
  readProperties(config);
  if (!toolbaron)
    toolBar()->hide();

  showToolbar->setChecked(!toolBar()->isHidden());
  showMenubar->setChecked(!menuBar()->isHidden());
  // activate and run first session //////////////////////////////////////////

  runSession(initial);
  // apply keytab
  keytab_menu_activated(n_keytab);
}

Konsole::~Konsole()
{
//FIXME: close all session properly and clean up
    // Delete the session if isn't in the session list any longer.
    if (sessions.find(se) == -1)
       delete se;
    sessions.setAutoDelete(true);
}

/* ------------------------------------------------------------------------- */
/*  Make menu                                                                */
/* ------------------------------------------------------------------------- */

void Konsole::makeMenu()
{
  // options (taken from kvt) //////////////////////////////////////

  m_file = new KPopupMenu(this);
  connect(m_file, SIGNAL(activated(int)), SLOT(newSession(int)));

  KToolBarPopupAction *newsession = new KToolBarPopupAction(i18n("&New"), "filenew",
                KStdAccel::key(KStdAccel::New), this, SLOT(newSession()),
                this, KStdAction::stdName(KStdAction::New));
  newsession->plug(toolBar());
  m_toolbarSessionsCommands = newsession->popupMenu();
  connect(m_toolbarSessionsCommands, SIGNAL(activated(int)), SLOT(newSession(int)));
  toolBar()->insertLineSeparator();

  KPopupMenu* m_signals = new KPopupMenu(this);
  m_signals->insertItem( i18n("STOP"), 17); // FIXME: comes with 3 values
  m_signals->insertItem( i18n("CONT"), 18); // FIXME: comes with 3 values
  m_signals->insertItem( i18n("HUP" ),  1);
  m_signals->insertItem( i18n("INT" ),  2);
  m_signals->insertItem( i18n("TERM"), 15);
  m_signals->insertItem( i18n("KILL"),  9);
  connect(m_signals, SIGNAL(activated(int)), SLOT(sendSignal(int)));

  m_sessions = new KPopupMenu(this);
  m_sessions->setCheckable(TRUE);
  m_sessions->insertItem( i18n("Send Signal"), m_signals );

  KAction *renameSession = new KAction(i18n("R&ename session..."), 0, this,
                             SLOT(slotRenameSession()), this);
  renameSession->plug(m_sessions);

  m_sessions->insertSeparator();

  m_schema = new KPopupMenu(this);
  m_schema->setCheckable(TRUE);
  connect(m_schema, SIGNAL(activated(int)), SLOT(schema_menu_activated(int)));

  m_keytab = new KPopupMenu(this);
  m_keytab->setCheckable(TRUE);
  connect(m_keytab, SIGNAL(activated(int)), SLOT(keytab_menu_activated(int)));

  m_codec  = new KPopupMenu(this);
  m_codec->setCheckable(TRUE);
  m_codec->insertItem( i18n("&locale"), 1 );
  m_codec->setItemChecked(1,TRUE);

  m_options = new KPopupMenu(this);
  // insert the rename schema here too, because they will not find it
  // on shift right click
  renameSession->plug(m_options);
  m_options->insertSeparator();
  // Menubar on/off
  showMenubar = KStdAction::showMenubar(this, SLOT(slotToggleMenubar()), this);
  showMenubar->plug(m_options);
  // Toolbar on/off
  showToolbar = KStdAction::showToolbar(this, SLOT(slotToggleToolbar()), this);
  showToolbar->plug(m_options);
  // Frame on/off
  showFrame = new KToggleAction(i18n("Show &Frame"), 0,
				this, SLOT(slotToggleFrame()), this);
  showFrame->plug(m_options);

  // Scrollbar
  selectScrollbar = new KSelectAction(i18n("Scrollbar"), 0, this,
			     SLOT(slotSelectScrollbar()), this);
  QStringList scrollitems;
  scrollitems << i18n("&Hide") << i18n("&Left") << i18n("&Right");
  selectScrollbar->setItems(scrollitems);
  selectScrollbar->plug(m_options);
  // Fullscreen
  m_options->insertSeparator();
  m_options->insertItem( i18n("F&ullscreen"), 5);
  m_options->setItemChecked(5,b_fullscreen);
  m_options->insertSeparator();
  // Select size
  selectSize = new KSelectAction(i18n("Size"), 0, this,
			     SLOT(slotSelectSize()), this);
  QStringList sizeitems;
  sizeitems << i18n("40x15 (&small)")
	    << i18n("80x24 (&vt100)")
	    << i18n("80x25 (&ibmpc)")
	    << i18n("80x40 (&xterm)")
	    << i18n("80x52 (ibmv&ga)");
  selectSize->setItems(sizeitems);
  selectSize->plug(m_options);
  // Select font
  selectFont = new KonsoleFontSelectAction(i18n("Font"), 0, this,
				 SLOT(slotSelectFont()), this);
  QStringList it;
  it << i18n("&Normal")
     << i18n("&Tiny")
     << i18n("&Small")
     << i18n("&Medium")
     << i18n("&Large")
     << i18n("&Huge")
     << ""
     << i18n("&Linux")
     << i18n("&Unicode")
     << ""
     << i18n("&Custom...");
  selectFont->setItems(it);
  selectFont->plug(m_options);
  // Schema
  m_options->insertItem( i18n("&Schema"), m_schema);
  m_options->insertSeparator();
  m_options->insertItem( i18n("&History"), 3 );
  m_options->setItemEnabled(3, false);
  m_options->insertSeparator();
  m_options->insertItem( i18n("&Codec"), m_codec);
  m_options->insertItem( i18n("&Keyboard"), m_keytab);
  m_options->insertSeparator();
  m_options->insertItem( i18n("Save &Options"), 8);
  connect(m_options, SIGNAL(activated(int)), SLOT(opt_menu_activated(int)));
  // Help and about menu
/*
  QString aboutAuthor = i18n("%1 version %2 - an X terminal\n"
                             "Copyright (c) 1997-2000 by\n"
                             "Lars Doelle <lars.doelle@on-line.de>\n"
                             "\n"
                             "This program is free software under the\n"
                             "terms of the GNU General Public License\n"
                             "and comes WITHOUT ANY WARRANTY.\n"
                             "See 'LICENSE.readme' for details.").arg(PACKAGE).arg(VERSION);
  KPopupMenu* m_help =  helpMenu(aboutAuthor, false);
*/
  KPopupMenu* m_help =  helpMenu(0, FALSE);
  m_help->insertItem( i18n("&Technical Reference"), this, SLOT(tecRef()),
                      0, -1, 1);
  m_options->installEventFilter( this );


  // For those who would like to add shortcuts here, be aware that
  // ALT-key combinations are heavily used by many programs. Thus,
  // activating shortcuts here means deactivating them in the other
  // programs.

  menubar->insertItem(i18n("File") , m_file);
  menubar->insertItem(i18n("Sessions"), m_sessions);
  menubar->insertItem(i18n("Options"), m_options);
  menubar->insertSeparator();
  menubar->insertItem(i18n("Help"), m_help);
}

/**
   This function calculates the size of the external widget
   needed for the internal widget to be
 */
QSize Konsole::calcSize(int columns, int lines) {
    QSize size = te->calcSize(columns, lines);
    if (!toolBar()->isHidden()) {
 	if ((toolBar()->barPos()==KToolBar::Top) ||
	    (toolBar()->barPos()==KToolBar::Bottom)) {
            int height = toolBar()->size().height();
            // The height of the toolbar is 1 during startup.
            if (height == 1)
               height = n_toolbarheight;
	    size += QSize(0, height);
	}
	if ((toolBar()->barPos()==KToolBar::Left) ||
	    (toolBar()->barPos()==KToolBar::Right)) {
	    size += QSize(toolBar()->size().width(), 0);
	}
    }
    if (!menuBar()->isHidden()) {
	size += QSize(0,menuBar()->size().height());
    }
    return size;
}

/**
    sets application window to a size based on columns X lines of the te
    guest widget. Call with (0,0) for setting default size.
*/

void Konsole::setColLin(int columns, int lines)
{
  if ((columns==0) || (lines==0))
  {
    if (defaultSize.isEmpty()) // not in config file : set default value
    {
      defaultSize = calcSize(80,24);
      notifySize(24,80); // set menu items (strange arg order !)
    }
    resize(defaultSize);
  } else {
    resize(calcSize(columns, lines));
    notifySize(lines,columns); // set menu items (strange arg order !)
  }
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void Konsole::configureRequest(TEWidget* te, int state, int x, int y)
{
//printf("Konsole::configureRequest(_,%d,%d)\n",x,y);
  ( (state & ShiftButton  ) ? m_sessions :
    (state & ControlButton) ? m_file :
                              m_options  )
  ->popup(te->mapToGlobal(QPoint(x,y)));
}


/* ------------------------------------------------------------------------- */
/*                                                                           */
/* Configuration                                                             */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void Konsole::saveGlobalProperties(KConfig* config)
{
  config->setGroup("global options");
  config->writeEntry("working directory", QDir::currentDirPath());
}

void Konsole::readGlobalProperties(KConfig* config)
{
  config->setGroup("global options");
  QDir::setCurrent(config->readEntry("working directory", QDir::currentDirPath()));
}

void Konsole::saveProperties(KConfig* config)
{
  n_toolbarheight = toolBar()->size().height();
  // bad! will no allow us to support multi windows
  config->writeEntry("toolbar height", n_toolbarheight);
  config->writeEntry("history",b_scroll);
  config->writeEntry("has frame",b_framevis);
  config->writeEntry("Fullscreen",b_fullscreen);
  config->writeEntry("font",n_font);
  config->writeEntry("defaultfont", defaultFont);
  config->writeEntry("schema",s_schema);
  config->writeEntry("scrollbar",n_scroll);
  config->writeEntry("keytab",n_keytab);

  if (args.count() > 0) config->writeEntry("konsolearguments", args);
  config->writeEntry("class",name());          
}


// Called by constructor (with config = KGlobal::config())
// and by session-management (with config = sessionconfig).
// So it has to apply the settings when reading them.
void Konsole::readProperties(KConfig* config)
{
/*FIXME: (merging) state of material below unclear.*/
  n_toolbarheight = config->readNumEntry("toolbar height",32); // Hack on hack...

  b_scroll = config->readBoolEntry("history",TRUE);
  setHistory(b_scroll);

  int n2_keytab = config->readNumEntry("keytab",0);
  keytab_menu_activated(n2_keytab); // act. the keytab for this session

  b_fullscreen = FALSE; // config->readBoolEntry("Fullscreen",FALSE);
  n_font     = QMIN(config->readUnsignedNumEntry("font",3),TOPFONT);
  n_scroll   = QMIN(config->readUnsignedNumEntry("scrollbar",TEWidget::SCRRIGHT),2);
  selectScrollbar->setCurrentItem(n_scroll);
  slotSelectScrollbar();
  s_schema   = config->readEntry("schema","");

  // Global options ///////////////////////

  b_framevis = config->readBoolEntry("has frame",TRUE);
  showFrame->setChecked( b_framevis );
  slotToggleFrame();

  // Options that should be applied to all sessions /////////////
  // (1) set menu items and Konsole members
  QFont tmpFont("fixed");
  defaultFont = config->readFontEntry("defaultfont", &tmpFont);
  setFont(QMIN(config->readUnsignedNumEntry("font",3),TOPFONT)); // sets n_font and menu item
  setSchema(config->readEntry("schema", ""));

  // (2) apply to sessions (currently only the 1st one)
  //  TESession* s = no2session.find(1);
  QPtrDictIterator<TESession> it( action2session ); // iterator for dict
  TESession* s = it.current();

  if (s)
  {
    s->setFontNo(n_font);
    s->setSchemaNo(ColorSchema::find(s_schema)->numb);
    s->setHistory(b_scroll);
  }
  else
  { fprintf(stderr,"session 1 not found\n"); } // oops

}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void Konsole::pixmap_menu_activated(int item)
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
  n_render= item;
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
    default: // oops
             n_render = 1;
  }
}

void Konsole::slotSelectScrollbar() {
    n_scroll = selectScrollbar->currentItem();
    te->setScrollbarLocation(n_scroll);
    activateSession(); // maybe helps in bg
}


void Konsole::slotSelectFont() {
  assert(se);
  int item = selectFont->currentItem();
  // kdDebug() << "slotSelectFont " << item << endl;
  if (item == 8) // this is the default
  {
    KFontDialog::getFont(defaultFont, true);
    item = 0;
  }
  setFont(item);
  activateSession(); // activates the current
}

void Konsole::schema_menu_activated(int item)
{
  assert(se);
  //FIXME: save schema name
  setSchema(item);
  activateSession(); // activates the current
}

void Konsole::keytab_menu_activated(int item)
{
  //  assert(se);
  //HERE; printf("keytab: %d\n",item);
  if (se) // not active at the beginning
    se->setKeymapNo(item);
  m_keytab->setItemChecked(n_keytab,FALSE);
  m_keytab->setItemChecked(item,TRUE);
  n_keytab = item;

}

void Konsole::setFont(int fontno)
{
  QFont f;
  if (fontno == 0)
    f = defaultFont;
  else
  if (fonts[fontno][0] == '-')
    f.setRawName( fonts[fontno] );
  else
  {
    f.setFamily(fonts[fontno]);
    f.setRawMode( TRUE );
  }
  if ( !f.exactMatch() && fontno != 0)
  {
    QString msg = i18n("Font `%1' not found.\nCheck README.linux.console for help.").arg(fonts[fontno]);
    KMessageBox::error(this,  msg);
    return;
  }
  if (se) se->setFontNo(fontno);
  selectFont->setCurrentItem(fontno);
  te->setVTFont(f);
  n_font = fontno;
}

void Konsole::slotToggleMenubar() {
  bool b_menuvis = showMenubar->isChecked();
  if (b_menuvis)
     menubar->show();
  else
     menubar->hide();
  if (!b_menuvis) {
    setCaption(i18n("Use the right mouse button to bring back the menu"));
    QTimer::singleShot(5000,this,SLOT(setHeader()));
  }
}

void Konsole::slotToggleToolbar() {
  bool b_toolbarvis = showToolbar->isChecked();
  if (b_toolbarvis)
     toolBar()->show();
  else
     toolBar()->hide();
}

void Konsole::slotToggleFrame() {
  b_framevis = showFrame->isChecked();
  te->setFrameStyle( b_framevis
                     ? ( QFrame::WinPanel | QFrame::Sunken )
                     : QFrame::NoFrame );
}


void Konsole::setHistory(bool on) {
  b_scroll = on;
  m_options->setItemChecked(3,b_scroll);
  if (se) se->setHistory( b_scroll );
}

void Konsole::opt_menu_activated(int item)
{
  switch( item )  {
    case 3: setHistory(!b_scroll);
            break;
    case 5: setFullScreen(!b_fullscreen);
            break;
    case 8:
            KConfig *config = KGlobal::config();
            config->setGroup("options");
            saveProperties(config);
            saveMainWindowSettings(config);
            config->sync();
            break;
  }
}

// --| color selection |-------------------------------------------------------

void Konsole::changeColumns(int columns)
{
  setColLin(columns,te->Lines());
  te->update();
}

void Konsole::slotSelectSize() {
    int item = selectSize->currentItem();
    switch (item) {
    case 0: setColLin(40,15); break;
    case 1: setColLin(80,24); break;
    case 2: setColLin(80,25); break;
    case 3: setColLin(80,40); break;
    case 4: setColLin(80,52); break;
    }
}


void Konsole::notifySize(int lines, int columns)
{
    selectSize->blockSignals(true);
    selectSize->setCurrentItem(-1);
    if (columns==40&&lines==15)
	selectSize->setCurrentItem(0);
    if (columns==80&&lines==24)
	selectSize->setCurrentItem(1);
    if (columns==80&&lines==25)
	selectSize->setCurrentItem(2);
    if (columns==80&&lines==40)
	selectSize->setCurrentItem(3);
    if (columns==80&&lines==52)
	selectSize->setCurrentItem(4);
    selectSize->blockSignals(false);
    if (n_render >= 3) pixmap_menu_activated(n_render);
}

void Konsole::setHeader()
{
  setCaption(title);
}

void Konsole::changeTitle(int, const QString& s)
{
  title = s; setHeader();
}

/*
   Konsole::showFullScreen() differes from QWidget::showFullScreen() in that
   we do not want to stay on top, since we want to be able to start X11 clients
   from a full screen konsole.
*/
void Konsole::showFullScreen()
{
    if ( !isTopLevel() )
	return;
    if ( topData()->fullscreen ) {
	show();
	raise();
	return;
    }
    if ( topData()->normalGeometry.width() < 0 )
	topData()->normalGeometry = QRect( pos(), size() );
    reparent( 0, WType_TopLevel | WStyle_Customize | WStyle_NoBorderEx, // | WStyle_StaysOnTop,
	      QPoint(0,0) );
    topData()->fullscreen = 1;
    resize( qApp->desktop()->size() );
    raise();
    show();
#if defined(_WS_X11_)
    extern void qt_wait_for_window_manager( QWidget* w ); // defined in qwidget_x11.cpp
    qt_wait_for_window_manager( this );
#endif

    setActiveWindow();
}

void Konsole::setFullScreen(bool on)
{
  if (on == b_fullscreen) return;
  if (on) showFullScreen(); else showNormal();
  b_fullscreen = on;
  m_options->setItemChecked(5,b_fullscreen);
}

// --| help |------------------------------------------------------------------

void Konsole::tecRef()
{
  kapp->invokeHTMLHelp(PACKAGE "/techref.html");
}

/* --| sessions |------------------------------------------------------------ */

//FIXME: activating sessions creates a lot flicker in the moment.
//       it comes from setting the attributes of a session individually.
//       ONE setImage call should actually be enough to match all cases.
//       These can be quite different:
//       - The screen size might have changed while the session was
//         detached. A propagation of the resize should in this case
//         make the drawEvent.
//       - font, background image and color palette should be set in one go.

void Konsole::sendSignal(int sn)
{
  if (se) se->kill(sn);
}


void Konsole::runSession(TESession* s)
{
    KRadioAction *ra = session2action.find(s);
    ra->setChecked(true);
    activateSession();

  // give some time to get through the
  // resize events before starting up.
    QTimer::singleShot(100,s,SLOT(run()));
}

void Konsole::addSession(TESession* s)
{
  session_no += 1;
  // create an action for the session
  QString title = i18n("%1 No %2").arg(s->Title()).arg(session_no);
  //  char buffer[30];
  //  int acc = CTRL+SHIFT+Key_0+session_no; // Lars: keys stolen by kwin.
  KRadioAction *ra = new KRadioAction(title,
                                     "openterm",
				      0,
				      this,
				      SLOT(activateSession()),
				      this);
				      //				      buffer);
  ra->setExclusiveGroup("sessions");
  ra->setChecked(true);
  // key accelerator
  //  accel->connectItem(accel->insertItem(acc), ra, SLOT(activate()));

  action2session.insert(ra, s);
  session2action.insert(s,ra);
  sessions.append(s);
  ra->plug(m_sessions);
  ra->plug(toolBar());
}

/**
   Activates a session (from the menu or by pressing a button)
 */
void Konsole::activateSession()
{
  TESession* s = NULL;
  // finds the session based on which button was activated
  QPtrDictIterator<TESession> it( action2session ); // iterator for dict
  while ( it.current() )
  {
    KRadioAction *ra = (KRadioAction*)it.currentKey();
    if (ra->isChecked()) { s = it.current(); break; }
    ++it;
  }
  if (s!=NULL) activateSession(s);
}

void Konsole::activateSession(TESession *s)
{
  if (se)
  { se->setConnect(FALSE);
    QObject::disconnect( se->getEmulation(),SIGNAL(prevSession()), this,SLOT(prevSession()) );
    QObject::disconnect( se->getEmulation(),SIGNAL(nextSession()), this,SLOT(nextSession()) );
    // Delete the session if isn't in the session list any longer.
    if (sessions.find(se) == -1)
       delete se;
  }
  se = s;
  session2action.find(se)->setChecked(true);
  QTimer::singleShot(1,this,SLOT(allowPrevNext())); // hack, hack, hack
  if (s->schemaNo()!=curr_schema)  {
    // the current schema has changed
    setSchema(s->schemaNo());
  } else  {
    // the schema was not changed
    ColorSchema* schema = ColorSchema::find(s->schemaNo());
    if (schema->usetransparency) {
	rootxpm->repaint(true); // this is a must, otherwise you loose the bg.
    }
  }
  te->currentSession = se;
  if (s->fontNo() != n_font)
      setFont(s->fontNo());
  s->setConnect(TRUE);
  title = s->Title(); // take title from current session
  setHeader();
  keytab_menu_activated(n_keytab); // act. the keytab for this session
}

void Konsole::allowPrevNext()
{
  QObject::connect( se->getEmulation(),SIGNAL(prevSession()), this,SLOT(prevSession()) );
  QObject::connect( se->getEmulation(),SIGNAL(nextSession()), this,SLOT(nextSession()) );
}

void Konsole::newSession()
{
  uint i=1;
  int session=0;
  while ( (session==0) && (i<=no2command.count()) )
  // antlarr: Why is first session session number 1 instead of number 0 ?
  {
    KSimpleConfig* co = no2command.find(i);

    if ( co && co->readEntry("Exec").isEmpty() ) session=i;
    i++;
  }

  if (session==0) session=1;

  newSession(session);
}

void Konsole::newSessionSelect()
{
  // Take into account the position of the toolBar to determine where we put the popup
  if((toolBar()->barPos() == KToolBar::Top) || (toolBar()->barPos() == KToolBar::Left)) {
    m_file->popup(te->mapToGlobal(QPoint(0,0)));
  } else if(toolBar()->barPos() == KToolBar::Right) {
    m_file->popup(te->mapToGlobal(QPoint(te->width()-m_file->sizeHint().width(), 0)));
  } else {
    m_file->popup(te->mapToGlobal(QPoint(0,te->height()-m_file->sizeHint().height())));
  }
}

void Konsole::newSession(int i)
{
  const char* shell = getenv("SHELL");
  if (shell == NULL || *shell == '\0') shell = "/bin/sh";

  KSimpleConfig* co = no2command.find(i);
  if (!co) return; // oops

  assert( se ); //FIXME: careful here.

  QString cmd = co->readEntry("Exec");
  QString nam = co->readEntry("Name");    // not null
  QCString emu = co->readEntry("Term").ascii();
  QString sch = co->readEntry("Schema");
  QString txt = co->readEntry("Comment"); // not null
  int     fno = QMIN(co->readUnsignedNumEntry("Font",se->fontNo()),TOPFONT);

  ColorSchema* schema = sch.isEmpty()
                      ? (ColorSchema*)NULL
                      : ColorSchema::find(sch);

  //FIXME: schema names here are absolut. Wrt. loadAllSchemas,
  //       relative pathes should be allowed, too.

  int schmno = schema?schema->numb:se->schemaNo();

  if (emu.isEmpty()) emu = se->emuName();

  QStrList args;
  args.append(shell);
  if (!cmd.isEmpty())
  {
    args.append("-c");
    args.append(QFile::encodeName(cmd));
  }

  TESession* s = new TESession(this,te,shell,args,emu);
  connect( s,SIGNAL(done(TESession*,int)),
           this,SLOT(doneSession(TESession*,int)) );
  connect( te, SIGNAL(configureRequest(TEWidget*, int, int, int)),
           this, SLOT(configureRequest(TEWidget*,int,int,int)) );

  s->setFontNo(fno);
  s->setSchemaNo(schmno);
  s->setTitle(txt);
  s->setHistory(b_scroll); //FIXME: take from schema

  addSession(s);
  runSession(s); // activate and run
}

//FIXME: If a child dies during session swap,
//       this routine might be called before
//       session swap is completed.

void Konsole::doneSession(TESession* s, int )
{
//printf("%s(%d): Exited:%d ExitStatus:%d\n",__FILE__,__LINE__,WIFEXITED(status),WEXITSTATUS(status));
#if 0 // die silently
  if (!WIFEXITED((status)) || WEXITSTATUS((status)))
  {
//FIXME: "Title" is not a precise locator for the message.
//       The command would be better.
    QString str = i18n("`%1' terminated abnormally.").arg(s->Title());
    if (WIFEXITED((status)))
    {char rcs[100]; sprintf(rcs,"%d.\n",WEXITSTATUS((status)));
      str = str + i18n("\nReturn code = ") + rcs;
    }
    KMessageBox::sorry(this, str);
  }
#endif
  KRadioAction *ra = session2action.find(s);
  ra->unplug(m_sessions);
  ra->unplug(toolBar());
  session2action.remove(s);
  action2session.remove(ra);
  sessions.remove(s);
  delete ra; // will the toolbar die?

  s->setConnect(FALSE);

  // This slot (doneSession) is activated from the TEPty when receiving a
  // SIGCHLD. A lot is done during the signal handler. Apparently deleting
  // the TEPty additionally is sometimes too much, causing something
  // to get messed up in rare cases. The following causes delete not to
  // be called from within the signal handler.

  QTimer::singleShot(1,s,SLOT(terminate()));

  if (s == se)
  { // pick a new session
    se = sessions.first();
    if ( se )
    {
      session2action.find(se)->setChecked(true);
      //FIXME: this Timer stupidity originated from the connected
      //       design of Emulations. By this the newly activated
      //       session might get a Ctrl(D) if the session has be
      //       terminated by this keypress. A likely problem
      //       can be found in the CMD_prev/nextSession processing.
      //       Since the timer approach only works at good weather,
      //       the whole construction is not suited to what it
      //       should do. Affected is the TEEmulation::setConnect.
      QTimer::singleShot(1,this,SLOT(activateSession()));
    }
    else
      close();
  }
}


/*! Cycle to previous session (if any) */

void Konsole::prevSession()
{
  sessions.find(se); sessions.prev();
  if (!sessions.current()) sessions.last();
  if (sessions.current()) activateSession(sessions.current());
}

/*! Cycle to next session (if any) */

void Konsole::nextSession()
{
  sessions.find(se); sessions.next();
  if (!sessions.current()) sessions.first();
  if (sessions.current()) activateSession(sessions.current());
}

// --| Session support |-------------------------------------------------------

void Konsole::addSessionCommand(const QString &path)
{
  KSimpleConfig* co = new KSimpleConfig(path,TRUE);
  co->setDesktopGroup();
  QString typ = co->readEntry("Type");
  QString txt = co->readEntry("Comment");
  // not used ?? !
  //  QString cmd = co->readEntry("Exec");
  QString nam = co->readEntry("Name");
  if (typ.isEmpty() || txt.isEmpty() || nam.isEmpty() ||
      typ != "KonsoleApplication")
  {
    delete co; return; // ignore
  }
  m_file->insertItem(txt, ++cmd_serial);
  m_toolbarSessionsCommands->insertItem(txt, cmd_serial);
  no2command.insert(cmd_serial,co);
}

void Konsole::loadSessionCommands()
{
  QStringList lst = KGlobal::dirs()->findAllResources("appdata", "*.desktop", false, true);

  for(QStringList::Iterator it = lst.begin(); it != lst.end(); ++it )
    addSessionCommand(*it);
}


// --| Schema support |-------------------------------------------------------

void Konsole::setSchema(int numb)
{
  ColorSchema* s = ColorSchema::find(numb);
  if (s) setSchema(s);
}

void Konsole::setSchema(const QString & path)
{
  ColorSchema* s = ColorSchema::find(path);
  if (s) setSchema(s);
}

void Konsole::setSchema(const ColorSchema* s)
{
  if (!s) return;
  m_schema->setItemChecked(curr_schema,FALSE);
  m_schema->setItemChecked(s->numb,TRUE);
  s_schema = s->path;
  curr_schema = s->numb;
  pmPath = s->imagepath;
  te->setColorTable(s->table); //FIXME: set twice here to work around a bug

  if (s->usetransparency) {
    rootxpm->setFadeEffect(s->tr_x, QColor(s->tr_r, s->tr_g, s->tr_b));
    rootxpm->start();
  } else {
    rootxpm->stop();
    pixmap_menu_activated(s->alignment);
  }

  te->setColorTable(s->table);
  if (se) se->setSchemaNo(s->numb);
}

void Konsole::slotRenameSession() {
  kdDebug() << "slotRenameSession\n";
  KRadioAction *ra = session2action.find(se);
  QString name = ra->text();
  KLineEditDlg dlg(i18n("Session name"),name, this);
  if (dlg.exec()) {
    ra->setText(dlg.text());
    ra->setIcon("openterm"); // I don't know why it is needed here
    toolBar()->updateRects();
  }
}

#include "konsole.moc"

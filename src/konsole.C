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
  - allow to set keytab
  - allow to set coded
  - fix setting history
  - fix resize (scroll)
  - fix reported crash in TEScreen.C (tcsh+TERM=ansi,CTRL-A)
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

#include <stdio.h>
#include <cstdlib>
//#include <unistd.h>

#include <kcolordlg.h>
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
#include <klineeditdlg.h>
#include <kdebug.h>

#include <qmessagebox.h>

#include <klocale.h>
#include <sys/wait.h>
#include <assert.h>

#include <kiconloader.h>

#include "konsole.h"
#include "keytrans.h"


#define HERE printf("%s(%d): here\n",__FILE__,__LINE__)



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

Konsole::Konsole(const QString& name,
                 const char* _pgm, QStrList & _args,
                 int histon) : KTMainWindow(name), pgm(_pgm), args(_args)
{

  session_no = 0;
  cmd_serial = 0;

  se = 0L;
  rootxpm = 0L;
  menubar = menuBar();
  b_scroll = histon;
  b_fullscreen = FALSE;
  n_keytab = 0;

  // create terminal emulation framework ////////////////////////////////////

  te = new TEWidget(this);
  te->setMinimumSize(150,70);    // allow resizing, cause resize in TEWidget

  // Transparency handler ///////////////////////////////////////////////////
  rootxpm = new KRootPixmap(te);

  // create applications /////////////////////////////////////////////////////

  setView(te,FALSE);
  makeMenu();
  // temporary default: show
  toolBar()->setIconText(KToolBar::IconTextRight);
  toolBar()->show();


  // Init DnD ////////////////////////////////////////////////////////////////

  //  setAcceptDrops(true);
  // FIXME: Now it is in TEWidget. Clean this up

  // load session commands ///////////////////////////////////////////////////

  loadSessionCommands();
  m_file->insertSeparator();
  m_file->insertItem( i18n("&Quit"), kapp, SLOT(quit()));

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

  if (b_menuvis) menubar->show(); else menubar->hide();
  te->setFrameStyle( b_framevis
		     ? (QFrame::WinPanel | QFrame::Sunken)
                     : QFrame::NoFrame );
  te->setScrollbarLocation(n_scroll);

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
  setHistory(b_scroll); //FIXME: take from schema

  addSession(initial);

  // read and apply default values ///////////////////////////////////////////

  readProperties(KGlobal::config());

  // activate and run first session //////////////////////////////////////////

  // WABA: Make sure all the xxxBars are in place so that
  //       we can resize ou configuration filesr mainWidget to its target size.
  updateRects();

  runSession(initial);
  // apply keytab
  keytab_menu_activated(n_keytab);
}

Konsole::~Konsole()
{
//FIXME: close all session properly and clean up
}

/* ------------------------------------------------------------------------- */
/*  Make menu                                                                */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void Konsole::makeMenu()
{
  // options (taken from kvt) //////////////////////////////////////

  KAction *newsession = KStdAction::openNew(this, SLOT(newDefaultSession()));
  newsession->plug(toolBar());
  toolBar()->insertLineSeparator();

  QPopupMenu* m_signals = new QPopupMenu;
  m_signals->insertItem( i18n("STOP"), 17); // FIXME: comes with 3 values
  m_signals->insertItem( i18n("CONT"), 18); // FIXME: comes with 3 values
  m_signals->insertItem( i18n("HUP" ),  1);
  m_signals->insertItem( i18n("INT" ),  2);
  m_signals->insertItem( i18n("TERM"), 15);
  m_signals->insertItem( i18n("KILL"),  9);
  connect(m_signals, SIGNAL(activated(int)), SLOT(sendSignal(int)));

  m_sessions = new QPopupMenu;
  m_sessions->setCheckable(TRUE);
  m_sessions->insertItem( i18n("Send Signal"), m_signals );

  KAction *act = new KAction(i18n("Rename session..."), 0, this,
                             SLOT(slotRenameSession()), this);
  act->plug(m_sessions);

  m_sessions->insertSeparator();
  m_file = new QPopupMenu;
  connect(m_file, SIGNAL(activated(int)), SLOT(newSession(int)));

  m_schema = new QPopupMenu;
  m_schema->setCheckable(TRUE);
  connect(m_schema, SIGNAL(activated(int)), SLOT(schema_menu_activated(int)));

  m_keytab = new QPopupMenu;
  m_keytab->setCheckable(TRUE);
  connect(m_keytab, SIGNAL(activated(int)), SLOT(keytab_menu_activated(int)));

  m_codec  = new QPopupMenu;
  m_codec->setCheckable(TRUE);
  m_codec->insertItem( i18n("&locale"), 1 );
  m_codec->setItemChecked(1,TRUE);

  m_options = new QPopupMenu;
  // Menubar on/off
  showMenubar = KStdAction::showMenubar(this, SLOT(slotToggleMenubar()));
  showMenubar->plug(m_options);
  // Toolbar on/off
  showToolbar = KStdAction::showToolbar(this, SLOT(slotToggleToolbar()));
  showToolbar->plug(m_options);
  // Frame on/off
  showFrame = new KToggleAction(i18n("Show &frame"), 0,
				this, SLOT(slotToggleFrame()), this);
  showFrame->plug(m_options);;
  // Scrollbar
  selectScrollbar = new KSelectAction(i18n("Scrollbar"), 0, this,
			     SLOT(slotSelectScrollbar()), this);
  QStringList scrollitems;
  scrollitems << i18n("&Hide") << i18n("&Left") << i18n("Right");
  selectScrollbar->setItems(scrollitems);
  selectScrollbar->plug(m_options);
  // Fullscreen
  m_options->insertSeparator();
  m_options->insertItem( i18n("&Fullscreen"), 5);
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
  selectFont = new KSelectAction(i18n("Fonts"), 0, this,
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
  m_options->insertSeparator();
  m_options->insertItem( i18n("&Codec"), m_codec);
  m_options->insertItem( i18n("&Keyboard"), m_keytab);
  m_options->insertSeparator();
  m_options->insertItem( i18n("&Save Options"), 8);
  connect(m_options, SIGNAL(activated(int)), SLOT(opt_menu_activated(int)));
  // Help and about menu
  QString aboutAuthor = i18n("%1 version %2 - an X terminal\n"
                             "Copyright (c) 1997-2000 by\n"
                             "Lars Doelle <lars.doelle@on-line.de>\n"
                             "\n"
                             "This program is free software under the\n"
                             "terms of the GNU General Public License\n"
                             "and comes WITHOUT ANY WARRANTY.\n"
                             "See 'LICENSE.readme' for details.").arg(PACKAGE).arg(VERSION);
  QPopupMenu* m_help =  helpMenu(aboutAuthor, false);
  m_help->insertItem( i18n("&Technical Reference ..."), this, SLOT(tecRef()),
                      0, -1, 1);
  /*
  // Popup menu for drag & drop
  m_drop = new QPopupMenu;
  m_drop->insertItem( i18n("Paste"), 0);
  m_drop->insertItem( i18n("cd"),    1);
  connect(m_drop, SIGNAL(activated(int)), SLOT(drop_menu_activated(int)));
  */

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

/*!
    sets application window to a size
    based on columns X lines of the te
    guest widget. Call with (0,0) for setting default size.
*/

void Konsole::setColLin(int columns, int lines)
{
  if (columns==0 && lines==0)
  {
    if (defaultSize.isNull()) // not in config file : set default value
    {
      defaultSize = te->calcSize(80,24);
      notifySize(24,80); // set menu items (strange arg order !)
    }
    te->resize(defaultSize);
  }
  else
  {
    QSize size = te->calcSize(columns, lines);
    te->resize(size);
    notifySize(lines,columns); // set menu items (strange arg order !)
  }
  adjustSize();
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
  config->setGroup("options");
  // bad! will no allow us to support multi windows
  config->writeEntry("menubar visible",b_menuvis);
  config->writeEntry("toolbar visible", b_toolbarvis);
  config->writeEntry("toolbar position", int(toolBar()->barPos()));
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
  config->writeEntry("defaultheight", te->height());
  // for "save options". Not used by SM.
  config->writeEntry("defaultwidth", te->width());
  // for "save options". Not used by SM.
  //config->writeEntry("kmenubar",                 //FIXME:Float
  //                   menubar->menuBarPos() == KMenuBar::Bottom ? "bottom" : "top");
  // geometry (placement) done by KTMainWindow
  config->sync();
}



// Called by constructor (with config = KGlobal::config())
// and by session-management (with config = sessionconfig).
// So it has to apply the settings when reading them.
void Konsole::readProperties(KConfig* config)
{
  config->setGroup("options"); // bad! will no allow us to support multi windows
/*FIXME: (merging) state of material below unclear.*/
  b_menuvis  = config->readBoolEntry("menubar visible",TRUE);
  b_toolbarvis  = config->readBoolEntry("toolbar visible",TRUE);
  n_toolbarpos  = config->readUnsignedNumEntry("toolbar position",
  		     (unsigned int) KToolBar::Bottom);

  b_scroll = config->readBoolEntry("history",TRUE);
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

  // Default values for startup, changed by "save options". Not used by SM.
  defaultSize.setWidth ( config->readNumEntry("defaultwidth", 0) );
  defaultSize.setHeight( config->readNumEntry("defaultheight", 0) );

  showToolbar->setChecked(b_toolbarvis);
  slotToggleToolbar();
  showMenubar->setChecked(b_menuvis);
  slotToggleMenubar();

  toolBar()->setBarPos((KToolBar::BarPosition)n_toolbarpos);
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
  kdDebug() << "slotSelectFont " << item << endl;
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
  }
  else
  {
    te->setVTFont(f);
  }
  n_font = fontno;
  if (se) se->setFontNo(fontno);
}

void Konsole::slotToggleMenubar() {
  b_menuvis = showMenubar->isChecked();
  if (b_menuvis) menubar->show(); else menubar->hide();
  if (!b_menuvis) {
    setCaption(i18n("Use the right mouse button to bring back the menu"));
    QTimer::singleShot(5000,this,SLOT(setHeader()));
  }
}


void Konsole::slotToggleToolbar() {
  b_toolbarvis = showToolbar->isChecked();
  enableToolBar(b_toolbarvis ? KToolBar::Show : KToolBar::Hide);
}

void Konsole::slotToggleFrame() {
  b_framevis = showFrame->isChecked();
  te->setFrameStyle( b_framevis
                     ? ( QFrame::WinPanel | QFrame::Sunken )
                     : QFrame::NoFrame );
}


void Konsole::setHistory(bool on)
{
//HERE; printf("setHistory: %s, having%s session.\n",on?"on":"off",se?"":" no");
  b_scroll = on;
  m_options->setItemChecked(3,b_scroll);
  if (se) se->setHistory( b_scroll );
}

void Konsole::opt_menu_activated(int item)
{
  switch( item )
  {
    /*
    case 2: setFrameVisible(!b_framevis);
            break;
    */
    case 3: setHistory(!b_scroll);
            break;
    case 5: setFullScreen(!b_fullscreen);
            break;
    case 8: saveProperties(KGlobal::config());
            break;
  }
}

// --| color selection |-------------------------------------------------------

void Konsole::changeColumns(int columns)
{
  setColLin(columns,24); // VT100, FIXME: keep lines?
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

void Konsole::setFullScreen(bool on)
{
  if (on == b_fullscreen) return;
  if (on)
  {
    _saveGeometry = geometry();
    setGeometry(kapp->desktop()->geometry());
  }
  else
  {
    setGeometry(_saveGeometry);
  }
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
  char buffer[30];
  sprintf(buffer,"%d",session_no);
  KRadioAction *ra = new KRadioAction(title, "openterm", 0, this, SLOT(activateSession()), this, buffer);
  ra->setExclusiveGroup("sessions");
  ra->setChecked(true);

  action2session.insert(ra, s);
  session2action.insert(s,ra);
  ra->plug(m_sessions);
  ra->plug(toolBar());
}

/*
   Activate session
 */
void Konsole::activateSession()
{
  TESession* s = NULL;
  QPtrDictIterator<TESession> it( action2session ); // iterator for dict
  while ( it.current() )
  {
    KRadioAction *ra = (KRadioAction*)it.currentKey();
    if (ra->isChecked()) { s = it.current(); break; }
    ++it;
  }
  if (s==NULL) {
    return;
  }
  //if (se == s) {  // This happens on startup
  //  kdDebug() << "Hello, you pressed the same button!!!" << endl;
  //}
  if (se) { se->setConnect(FALSE); }
  se = s;
  if (!s) { fprintf(stderr,"session not found\n"); return; } // oops
  if (s->schemaNo()!=curr_schema)
  {
    setSchema(s->schemaNo());
    //Set Font. Now setConnect should do the appropriate action.
    //if the size has changed, a resize event (noticable to the application)
    //should happen. Else, we  could even start the application
    s->setConnect(TRUE);     // does a bulkShow (setImage)
    setFont(s->fontNo());    //FIXME: creates flicker?
    title = s->Title(); // take title from current session
  }
  else
  {
    // if the schema uses transparency we MUST redo it...
    // even if it flickers. There might be a flickerless solution for this
    // but please don't change it without finding a solution first.
    ColorSchema* schema = ColorSchema::find(s->schemaNo());
    if (schema->usetransparency)
      setSchema(s->schemaNo());
    s->setConnect(TRUE);
  }
  setHeader();
  te->currentSession = se;
  keytab_menu_activated(n_keytab); // act. the keytab for this session
}

void Konsole::newDefaultSession()
{
  // (Lotzi says) for some funny reason, this is the third in the list
  // (Lars says)  it's random, really. The list is read in, and should
  //              best be presented.
  newSession(3);
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
  QString emu = co->readEntry("Term");
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
    args.append(cmd);
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
 
  // Lotzi B: I don't know why it was here, but commenting it out
  // fixes the annoying bug of the jumping of the scrollbar when 
  // creating a new menu
  //setHistory(b_scroll); //FIXME: take from schema

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
  delete ra; // will the toolbar die?

  s->setConnect(FALSE);

  // This slot (doneSession) is activated from the TEPty when receiving a
  // SIGCHLD. A lot is done during the signal handler. Apparently deleting
  // the TEPty additionally is sometimes too much, causing something
  // to get messed up in rare cases. The following causes delete not to
  // be called from within the signal handler.

  QTimer::singleShot(100,s,SLOT(terminate()));

  if (s == se)
  { // pick a new session
    se = NULL;
    //    QIntDictIterator<TESession> it( no2session );
    QPtrDictIterator<KRadioAction> it( session2action);
    if ( it.current() ) {
        it.current()->setChecked(true);
        activateSession();
    } else
      kapp->quit();
  }
}

// --| Session support |-------------------------------------------------------

void Konsole::addSessionCommand(const char* path)
{
  KSimpleConfig* co = new KSimpleConfig(path,TRUE);
  co->setDesktopGroup();
  QString typ = co->readEntry("Type");
  QString txt = co->readEntry("Comment");
  QString cmd = co->readEntry("Exec");
  QString nam = co->readEntry("Name");
  if (typ.isEmpty() || txt.isEmpty() || nam.isEmpty() ||
      typ != "KonsoleApplication")
  {
    delete co; return; // ignore
  }
  m_file->insertItem(txt, ++cmd_serial);
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
    toolBar()->updateRects();
  }
}

#include "konsole.moc"

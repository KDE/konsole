/* -------------------------------------------------------------------------- */
/*                                                                            */
/* [main.C]                 Testbed for TE framework                          */
/*                                                                            */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>            */
/*                                                                            */
/* This file is part of Konsole, an X terminal.                               */
/*                                                                            */
/* The material contained in here more or less directly orginates from        */
/* kvt, which is copyright (c) 1996 by Matthias Ettrich <ettrich@kde.org>     */
/*                                                                            */
/* -------------------------------------------------------------------------- */

/*! \class TEDemo

    \brief Konsole's main program

    The class TEDemo handles the application level. Mainly, it is responsible
    for the configuration, taken from several files, from the command line
    and from the user. It hardly does anything interesting.
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
    We have to find a single-place method to maintain this.
    Scedule: post kde 1.2
  - Controling the widget is currently done by individual attributes.
    This lead to quite some amount of flicker when a whole bunch of
    attributes has to be set, e.g. in session swapping.
    Scedule: post kde 1.2
  - The schema file name in session config files is not location
    transparent.
*/

/* TODO
- don't reread the pixmap every time
*/
#ifdef HAVE_CONFIG_H
#include"config.h"
#endif

#include <qdir.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "main.h"

#include <kcolordlg.h>
#include <kfontdialog.h>
#include <kmsgbox.h>
#include <qpainter.h>

#include <kimgio.h>

#include <qintdict.h>
#include <qptrdict.h>

#include <kwm.h>
#include <sys/wait.h>
#include <assert.h>

#include <locale.h>

#define HERE printf("%s(%d): here\n",__FILE__,__LINE__)

inline int MIN(int A, int B) { return ((A>B) ? B : A); }

#undef PACKAGE
#undef VERSION
#define PACKAGE "konsole"
#define VERSION "0.9.9"

#define WITH_VGA

char *fonts[] = {"6x13", "5x7", "6x10", "7x13", "9x15", "10x20",
                 "linux8x16", "linux8x8" };

static QIntDict<TESession> no2session;
static QPtrDict<void>      session2no;
static int session_no = 0;

static QIntDict<KSimpleConfig> no2command;
static int cmd_serial = 0;

TEDemo::TEDemo(char* name, QStrList & _args, int login_shell) : KTMainWindow(name), args(_args)
{
  se = 0L;
  menubar = menuBar();
  setMinimumSize(200,100);
  
  // session management
  setUnsavedData( true ); // terminals cannot store their contents

  // create terminal emulation framework ////////////////////////////////////

  te = new TEWidget(this);
  te->setMinimumSize(150,70);    // allow resizing, cause resize in TEWidget

  // create applications /////////////////////////////////////////////////////

  setView(te,FALSE);
  makeMenu();
  makeStatusbar();

  // Init DnD: Set up drop zone and drop handler /////////////////////////////

  dropZone = new KDNDDropZone( this, DndURL );
  connect( dropZone, SIGNAL( dropAction( KDNDDropZone* )),
                     SLOT( onDrop( KDNDDropZone*)));

  // load session commands ///////////////////////////////////////////////////

  loadSessionCommands();
  m_file->insertSeparator();
  m_file->insertItem( i18n("E&xit"), kapp, SLOT(quit()));

  // load schema /////////////////////////////////////////////////////////////

  curr_schema = 0;
  ColorSchema::loadAllSchemas();
  for (int i = 0; i < ColorSchema::count(); i++)
  { ColorSchema* s = ColorSchema::find(i);
    assert( s );
    m_schema->insertItem(s->title.data(),s->numb);
  }

//FIXME: we should build a complete session before running it.

  // construct initial session ///////////////////////////////////////////////

  TESession* initial = new TESession(this,te,args,"xterm",login_shell);

  title = (args.count() && !strcmp(kapp->getCaption(),PACKAGE))
        ? args.at(0)           // program executed in the title bar
        : kapp->getCaption();  // `konsole' or -caption
  initial->setTitle(title);

  addSession(initial);

  // read and apply default values ///////////////////////////////////////////

  readProperties(kapp->getConfig());

  // activate and run first session //////////////////////////////////////////

  runSession(initial);

}

/*!
    sets application window to a size
    based on columns X lines of the te
    guest widget. Call with (0,0) for setting default size.
*/

void TEDemo::setColLin(int columns, int lines)
{
  if (columns==0 && lines==0)
  {
    if (defaultSize.isNull()) // not in config file : set default value
    {
      defaultSize = te->calcSize(80,24);
      notifySize(24,80); // set menu items (strange arg order !)
    }
    resize(defaultSize);
  }
  else
  {
    resize(te->calcSize(columns,lines));
    notifySize(lines,columns); // set menu items (strange arg order !)
  }
  updateRects();
}

TEDemo::~TEDemo()
{
//FIXME: close all session properly and clean up
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TEDemo::onDrop( KDNDDropZone* _zone )
{
    // The current behaviour when url(s) are dropped is
    // * if there is only ONE url and if it's a LOCAL one, ask for paste or cd
    // * in all other cases, just paste
    //   (for non-local ones, or for a list of URLs, 'cd' is nonsense)
  QStrList strlist;
  KURL *url;
  int file_count = 0;
  char *p;
  dropText = "";
  bool bPopup = true;

  strlist = _zone->getURLList();
  if (strlist.count())
  {
    p = strlist.first();
    while(p != 0)
    {
      if(file_count++ > 0)
      {
        dropText += " ";
        bPopup = false; // more than one file, don't popup
      }
      url = new KURL( p );
      if (!strcmp(url->protocol(),"file"))
      {
        dropText += url->path(); // local URL : remove protocol
      }
      else
      {
        dropText += p;
        bPopup = false; // a non-local file, don't popup
      }
      delete url;
      p = strlist.next();
    }
    if (bPopup)
        m_drop->popup(QPoint(_zone->getMouseX(),_zone->getMouseY()));
    else
        se->getEmulation()->sendString(dropText.data());
  }
}

void TEDemo::drop_menu_activated(int item)
{
  switch (item)
  {
    case 0: // paste
      se->getEmulation()->sendString(dropText.data());
//    KWM::activate((Window)this->winId());
      break;
    case 1: // cd ...
      se->getEmulation()->sendString("cd ");
      KURL url( dropText );
      se->getEmulation()->sendString(url.directory());
      se->getEmulation()->sendString("\n");
//    KWM::activate((Window)this->winId());
      break;
  }
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TEDemo::configureRequest(TEWidget* te, int state, int x, int y)
{
//printf("TEDemo::configureRequest(_,%d,%d)\n",x,y);
  ( (state & ShiftButton  ) ? m_sessions :
    (state & ControlButton) ? m_file :
                              m_options  )
  ->popup(te->mapToGlobal(QPoint(x,y)));
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TEDemo::makeStatusbar()
{
//statusbar = new KStatusBar(this);
//setStatusBar(statusbar);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TEDemo::makeMenu()
{
  // options (taken from kvt) //////////////////////////////////////

//m_commands = new QPopupMenu;
//connect(m_commands, SIGNAL(activated(int)), SLOT(newSession(int)));

  m_sessions = new QPopupMenu;
  m_sessions->setCheckable(TRUE);
  connect(m_sessions, SIGNAL(activated(int)), SLOT(activateSession(int)));

  m_file = new QPopupMenu;
  connect(m_file, SIGNAL(activated(int)), SLOT(newSession(int)));

  m_font = new QPopupMenu;
  m_font->setCheckable(TRUE);
  m_font->insertItem( i18n("&Normal"), 0);
  m_font->insertSeparator();
//m_font->insertItem( i18n("&Tiny"),   1);
  m_font->insertItem( i18n("&Small"),  2);
  m_font->insertItem( i18n("&Medium"), 3);
  m_font->insertItem( i18n("&Large"),  4);
  m_font->insertItem( i18n("&Huge"),   5);
  m_font->insertSeparator();
  m_font->insertItem( i18n("&Linux"),  6);
  m_font->insertItem( i18n("Linux (small)"),7);
#ifdef ANY_FONT
  m_font->insertSeparator();
  m_font->insertItem( i18n("&More ..."), 1000); // for other fonts
#endif
  connect(m_font, SIGNAL(activated(int)), SLOT(font_menu_activated(int)));

  m_scrollbar = new QPopupMenu;
  m_scrollbar->setCheckable(TRUE);
  m_scrollbar->insertItem( i18n("&Hide"), SCRNONE);
  m_scrollbar->insertItem( i18n("&Left"), SCRLEFT);
  m_scrollbar->insertItem( i18n("&Right"), SCRRIGHT);
  connect(m_scrollbar, SIGNAL(activated(int)), SLOT(scrollbar_menu_activated(int)));

  m_size = new QPopupMenu;
  m_size->setCheckable(TRUE);
  m_size->insertItem( i18n("40x15 (&small)"), 0);
  m_size->insertItem( i18n("80x24 (&vt100)"), 1);
  m_size->insertItem( i18n("80x25 (&ibmpc)"), 2);
  m_size->insertItem( i18n("80x40 (&xterm)"), 3);
  m_size->insertItem( i18n("80x52 (ibmv&ga)"), 4);
  connect(m_size, SIGNAL(activated(int)), SLOT(size_menu_activated(int)));

  m_schema = new QPopupMenu;
  m_schema->setCheckable(TRUE);
  connect(m_schema, SIGNAL(activated(int)), SLOT(schema_menu_activated(int)));

  m_options = new QPopupMenu;
  m_options->setCheckable(TRUE);
  m_options->insertItem( i18n("&Menubar"), 1 );
  m_options->insertItem( i18n("&Frame"), 2 );
  m_options->insertItem( i18n("Scroll&bar"), m_scrollbar);
  m_options->insertSeparator();
  m_options->insertItem( i18n("BS sends &DEL"), 4 );
  m_options->insertSeparator();
  m_options->insertItem( i18n("&Font"), m_font);
  m_options->insertItem( i18n("&Size"), m_size);
  m_options->insertItem( i18n("&Schema"), m_schema);
  m_options->insertSeparator();
  m_options->insertItem( i18n("&Save options"), 8);
  connect(m_options, SIGNAL(activated(int)), SLOT(opt_menu_activated(int)));

  QPopupMenu* m_help = new QPopupMenu;
  m_help->insertItem( i18n("&About ..."), this, SLOT(about()));
  m_help->insertItem( i18n("&User's Manual ..."), this, SLOT(help()));
  m_help->insertItem( i18n("&Technical Reference ..."), this, SLOT(tecRef()));

  m_drop = new QPopupMenu;
  m_drop->insertItem( i18n("Paste"), 0);
  m_drop->insertItem( i18n("cd"),    1);
  connect(m_drop, SIGNAL(activated(int)), SLOT(drop_menu_activated(int)));

  m_options->installEventFilter( this );

  // For those who would like to add shortcuts here, be aware that
  // ALT-key combinations are heavily used by many programs. Thus,
  // activating shortcuts here means deactivating them in the other
  // programs.

  menubar->insertItem(i18n("File") , m_file);

//menubar->insertItem(i18n("New"), m_commands);
  menubar->insertItem(i18n("Sessions"), m_sessions);

  menubar->insertItem(i18n("Options"), m_options);
  menubar->insertSeparator();
  menubar->insertItem(i18n("Help"), m_help);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* Configuration                                                             */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TEDemo::saveProperties(KConfig* config)
{
  config->setGroup("options"); // bad! will no allow us to support multi windows
  config->writeEntry("menubar visible",b_menuvis);
  config->writeEntry("has frame",b_framevis);
  config->writeEntry("BS hack",b_bshack);
  config->writeEntry("font",n_font);
  config->writeEntry("schema",s_schema);
  config->writeEntry("scrollbar",n_scroll);
  if (args.count() > 0) config->writeEntry("konsolearguments", args);
  config->writeEntry("class",name());
  config->writeEntry("defaultheight", height()); // for "save options". Not used by SM.
  config->writeEntry("defaultwidth", width()); // for "save options". Not used by SM.
  config->writeEntry("kmenubar", //FIXME:Float
                     menubar->menuBarPos() == KMenuBar::Bottom ? "bottom" : "top");
  // geometry (placement) done by KTMainWindow
  config->sync();
}

// Called by constructor (with config = kapp->getConfig())
// and by session-management (with config = sessionconfig).
// So it has to apply the settings when reading them.
void TEDemo::readProperties(KConfig* config)
{
  config->setGroup("options"); // bad! will no allow us to support multi windows

  // Global options ///////////////////////

  setMenuVisible(config->readBoolEntry("menubar visible",TRUE));
  setFrameVisible(config->readBoolEntry("has frame",TRUE));
  
  scrollbar_menu_activated(MIN(config->readUnsignedNumEntry("scrollbar",SCRRIGHT),2));

  // not necessary for SM (KTMainWindow does it after), but useful for default settings
  if (menubar->menuBarPos() != KMenuBar::Floating)
  { QString entry = config->readEntry("kmenubar");
    if (!entry.isEmpty() && entry == "floating")
    {
      menubar->setMenuBarPos(KMenuBar::Floating);
      QString geo = config->readEntry("kmenubargeometry");
      if (!geo.isEmpty()) menubar->setGeometry(KWM::setProperties(menubar->winId(), geo));
    }
    else if (!entry.isEmpty() && entry == "top") menubar->setMenuBarPos(KMenuBar::Top);
    else if (!entry.isEmpty() && entry == "bottom") menubar->setMenuBarPos(KMenuBar::Bottom);
  }
  // (geometry stuff removed) done by KTMainWindow for SM, and not needed otherwise

  // Options that should be applied to all sessions /////////////
  // (1) set menu items and TEDemo members
  setBsHack(config->readBoolEntry("BS hack",TRUE));
  setFont(MIN(config->readUnsignedNumEntry("font",3),7)); // sets n_font and menu item
  setSchema(config->readEntry("schema",""));
  // (2) apply to sessions (currently only the 1st one)
  TESession* s = no2session.find(1);
  if (s) {
    s->setFontNo(n_font);
    s->setSchemaNo(ColorSchema::find(s_schema)->numb);
  } else { fprintf(stderr,"session 1 not found\n"); } // oops

  // Default values for startup, changed by "save options". Not used by SM.
  defaultSize.setWidth ( config->readNumEntry("defaultwidth", 0) );
  defaultSize.setHeight( config->readNumEntry("defaultheight", 0) );
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TEDemo::pixmap_menu_activated(int item)
{
  if (item <= 1) pmPath = "";
  QPixmap pm(pmPath.data());
  if (pm.isNull()) { pmPath = ""; item = 1; }
  // FIXME: respect scrollbar (instead of te->size)
  n_render = item;
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

void TEDemo::scrollbar_menu_activated(int item)
{
  m_scrollbar->setItemChecked(n_scroll,FALSE);
  m_scrollbar->setItemChecked(item,    TRUE);
  n_scroll = item;
  te->setScrollbarLocation(item);
}

void TEDemo::font_menu_activated(int item)
{
  assert(se);
  se->setFontNo(item);
  activateSession((int)session2no.find(se)); // for attribute change
  // setFont(item) is probably enough
}

void TEDemo::schema_menu_activated(int item)
{
  assert(se);
  //FIXME: save schema name
  se->setSchemaNo(item);
  activateSession((int)session2no.find(se)); // for attribute change
  // setSchema(item) is probably enough
}

void TEDemo::setFont(int fontno)
{
  QFont f( fonts[fontno] );
  f.setRawMode( TRUE );
  if ( !f.exactMatch() )
  {
    QString msg;
    msg.sprintf(i18n("Font `%s' not found.\nCheck README.linux.console for help."), fonts[fontno]);
    KMsgBox::message
    ( this,
      i18n("Error"), msg,
      KMsgBox::EXCLAMATION );
  }
  else
  {
    te->setVTFont(f);
  }
  m_font->setItemChecked(n_font,FALSE);
  m_font->setItemChecked(fontno, TRUE);
  n_font = fontno;
}

void TEDemo::setMenuVisible(bool visible)
{
  b_menuvis = visible;
  m_options->setItemChecked(1,b_menuvis);
  if (b_menuvis) menubar->show(); else menubar->hide();
  updateRects();
}

void TEDemo::setFrameVisible(bool visible)
{
  b_framevis = visible;
  m_options->setItemChecked(2,b_framevis);
  te->setFrameStyle( b_framevis
                     ? QFrame::WinPanel | QFrame::Sunken
                     : QFrame::NoFrame );
}

void TEDemo::setBsHack(bool bshack)
{
  b_bshack = bshack;
  m_options->setItemChecked(4,b_bshack);
  //FIXME: somewhat fuzzy...  - should be done for all sessions ?
  if (se)
    if (b_bshack)
      ((VT102Emulation*)se->getEmulation())->setMode(MODE_BsHack);
    else
      ((VT102Emulation*)se->getEmulation())->resetMode(MODE_BsHack);
}

void TEDemo::opt_menu_activated(int item)
{
  switch( item )
  {
    case 1: setMenuVisible(!b_menuvis);
            if (!b_menuvis)
            {
              setCaption("Use the right mouse button to bring back the menu");
              QTimer::singleShot(5000,this,SLOT(setHeader()));
            }
            break;
    case 2: setFrameVisible(!b_framevis);
            break;
    case 4: setBsHack(!b_bshack);
            break;
    case 8: saveProperties(kapp->getConfig());
            break;
  }
}

// --| color selection |-------------------------------------------------------

void TEDemo::changeColumns(int columns)
{
  setColLin(columns,24); // VT100, FIXME: keep lines?
  te->update();
}

void TEDemo::size_menu_activated(int item)
{
  switch (item)
  {
    case 0: setColLin(40,15); break;
    case 1: setColLin(80,24); break;
    case 2: setColLin(80,25); break;
    case 3: setColLin(80,40); break;
    case 4: setColLin(80,52); break;
  }
}

void TEDemo::notifySize(int lines, int columns)
{
// printf("notifySize(%d,%d)\n",lines,columns);
/*
  if (lines != lincol.height() || columns != lincol.width())
  { char buf[100];
    sprintf(buf,i18n("(%d columns x %d lines)"),columns,lines);
    setCaption(buf);
    QTimer::singleShot(2000,this,SLOT(setHeader()));
  }
*/
  m_size->setItemChecked(0,columns==40&&lines==15);
  m_size->setItemChecked(1,columns==80&&lines==24);
  m_size->setItemChecked(2,columns==80&&lines==25);
  m_size->setItemChecked(3,columns==80&&lines==40);
  m_size->setItemChecked(4,columns==80&&lines==52);
  if (n_render >= 3) pixmap_menu_activated(n_render);
}

void TEDemo::setHeader()
{
  setCaption(title);
}

void TEDemo::changeTitle(int, const char*s)
{
  title = s; setHeader();
}

// --| help |------------------------------------------------------------------

void TEDemo::about()
//FIXME: make this a little nicer
{
  QString title, msg(1000);

  title.sprintf(i18n("About %s"), PACKAGE);
  msg.sprintf(i18n(
    "%s version %s - an X terminal\n"
    "\n"
    "Copyright (c) 1998 by Lars Doelle <lars.doelle@on-line.de>\n"
    "\n"
    "This program is free software under the\n"
    "terms of the Artistic License and comes\n"
    "WITHOUT ANY WARRANTY.\n"
    "See `LICENSE.readme´ for details."), PACKAGE, VERSION);
  KMsgBox::message( 0, title, msg );
}

void TEDemo::help()
{
  kapp->invokeHTMLHelp(PACKAGE "/konsole.html","");
}

void TEDemo::tecRef()
{
  kapp->invokeHTMLHelp(PACKAGE "/techref.html","");
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

void TEDemo::activateSession(int sn)
{
  TESession* s = no2session.find(sn);
  if (se)
  {
    se->setConnect(FALSE);
    int no = (int)session2no.find(se);
    m_sessions->setItemChecked(no,FALSE);
  }
  se = s;
  if (!s) { fprintf(stderr,"session not found\n"); return; } // oops
  m_sessions->setItemChecked(sn,TRUE);
  setSchema(s->schemaNo());               //FIXME: creates flicker? Do only if differs
//Set Font. Now setConnect should do the appropriate action.
//if the size has changed, a resize event (noticable to the application)
//should happen. Else, we  could even start the application
  s->setConnect(TRUE);                    // does a bulkShow (setImage)
  setFont(s->fontNo());                   //FIXME: creates flicker?
                                          //FIXME: check here if we're still alife.
                                          //       if not, quit, otherwise,
                                          //       start propagating quit.
  title = s->Title(); // take title from current session
  setHeader();
}

void TEDemo::runSession(TESession* s)
{
  int session_no = (int)session2no.find(s);
  activateSession(session_no);

  // give some time to get through the
  // resize events before starting up.
  QTimer::singleShot(100,s,SLOT(run()));
}

void TEDemo::addSession(TESession* s)
{
  session_no += 1;
  no2session.insert(session_no,s);
  session2no.insert(s,(void*)session_no);
  m_sessions->insertItem(s->Title(), session_no);
}

void TEDemo::newSession(int i)
{
  char* shell = getenv("SHELL");
  if (shell == NULL || *shell == '\0') shell = "/bin/sh";

  KSimpleConfig* co = no2command.find(i);
  if (!co) return; // oops

  assert( se ); //FIXME: careful here.

  QString cmd = co->readEntry("Exec");    // not null
  QString nam = co->readEntry("Name");    // not null
  QString emu = co->readEntry("Term");
  QString sch = co->readEntry("Schema");
  QString txt = co->readEntry("Comment"); // not null
  int     fno = MIN(co->readUnsignedNumEntry("Font",se->fontNo()),7);

  ColorSchema* schema = sch.isEmpty()
                      ? (ColorSchema*)NULL
                      : ColorSchema::find(sch);

  //FIXME: schema names here are absolut. Wrt. loadAllSchemas,
  //       relative pathes should be allowed, too.

  int schmno = schema?schema->numb:se->schemaNo();

  if (emu.isEmpty()) emu = se->emuName();

  QStrList args;
  args.append(shell);
  args.append("-c");
  args.append(cmd);

  TESession* s = new TESession(this,te,args,emu.data(),0);
  s->setFontNo(fno);
  s->setSchemaNo(schmno);
  s->setTitle(txt.data());

  addSession(s);
  runSession(s); // activate and run
}

//FIXME: If a child dies during session swap,
//       this routine might be called before
//       session swap is completed.

void TEDemo::doneSession(TESession* s, int status)
{
//printf("%s(%d): Exited:%d ExitStatus:%d\n",__FILE__,__LINE__,WIFEXITED(status),WEXITSTATUS(status));
  if (!WIFEXITED((status)) || WEXITSTATUS((status)))
  {
    QString str;
    str.sprintf(i18n("`%s' terminated abnormally."), s->Title());
    if (WIFEXITED((status)))
    {char rcs[100]; sprintf(rcs,"%d.\n",WEXITSTATUS((status)));
      str = str + i18n("\nReturn code = ") + rcs;
    }
    KMsgBox::message( this, i18n("Error"), str, KMsgBox::EXCLAMATION );

  }
  int no = (int)session2no.find(s);
  if (!no) return; // oops
  no2session.remove(no);
  session2no.remove(s);
  m_sessions->removeItem(no);

  s->setConnect(FALSE);

  // This slot (doneSession) is activated from the Shell when receiving a
  // SIGCHLD. A lot is done during the signal handler, apparently deleting
  // the Shell additionally, is sometimes too much, causing something
  // to get messed up in rare cases. The following causes delete not to
  // be called from within the signal handler.

  QTimer::singleShot(100,s,SLOT(terminate()));

  if (s == se)
  { // pick a new session
    se = NULL;
    QIntDictIterator<TESession> it( no2session );
    if ( it.current() )
      activateSession(it.currentKey());
    else
      kapp->quit();
  }
}

// --| Session support |-------------------------------------------------------

void TEDemo::addSessionCommand(const char* path)
{
  KSimpleConfig* co = new KSimpleConfig(path,TRUE);
  co->setGroup("KDE Desktop Entry");
  QString typ = co->readEntry("Type");
  QString txt = co->readEntry("Comment");
  QString cmd = co->readEntry("Exec");
  QString nam = co->readEntry("Name");
  if (typ.isEmpty() || txt.isEmpty() || cmd.isEmpty() || nam.isEmpty() ||
      strcmp(typ.data(),"KonsoleApplication"))
  {
    delete co; return; // ignore
  }
  m_file->insertItem(txt, ++cmd_serial);
  no2command.insert(cmd_serial,co);
}

void TEDemo::loadSessionCommands()
{
  for (int local=0; local<=1; local++)
  {
    // KApplication could support this technique better
    QString path = local
                 ? kapp->localkdedir() + "/share/apps/konsole"
                 : kapp->kde_datadir() + "/konsole";
    QDir d( path );
    if(!d.exists()) return;
    d.setFilter( QDir::Files | QDir::Readable );
    d.setNameFilter( "*.kdelnk" );
    const QFileInfoList *list = d.entryInfoList();
    QFileInfoListIterator it( *list );
    for(QFileInfo *fi; (fi=it.current()); ++it )
      addSessionCommand(fi->filePath());
  }
}

// --| Schema support |-------------------------------------------------------

void TEDemo::setSchema(int numb)
{
  ColorSchema* s = ColorSchema::find(numb);
  if (s) setSchema(s);
}

void TEDemo::setSchema(const char* path)
{
  ColorSchema* s = ColorSchema::find(path);
  if (s) setSchema(s);
}

void TEDemo::setSchema(const ColorSchema* s)
{
  if (!s) return;
  m_schema->setItemChecked(curr_schema,FALSE);
  m_schema->setItemChecked(s->numb,TRUE);
  s_schema = s->path;
  curr_schema = s->numb;
  pmPath = s->imagepath;
  te->setColorTable(s->table); //FIXME: set twice here to work around a bug
  pixmap_menu_activated(s->alignment);
  te->setColorTable(s->table);
  if (se) se->setSchemaNo(s->numb);
}

/* --| main |---------------------------------------------------------------- */

static void usage()
{
  fprintf
  (stderr,
   "usage: %s [option ...]\n"
   "%s version %s, an X terminal for KDE.\n"
   "\n"
   " -e Command Parameter ... Execute command instead of shell\n"
   " -name .................. Set Window Class\n"
   " -h ..................... This text\n"
   " -ls .................... Start login shell\n"
   " -nowelcome ............. Suppress greeting\n"
   " -sl <number> ........... Save number lines in scroll-back buffer\n"
   " -vt_sz CCxLL ........... terminal size in columns x lines \n"
   "\n"
   "Other options due to man:X(1x), Qt and KDE, among them:\n"
   "\n"
   " -caption 'Text'......... Set title\n"
   " -display <display> ..... Set the X-Display\n"
  ,PACKAGE,PACKAGE,VERSION
  );
}

extern int maxHistLines;

int main(int argc, char* argv[])
{
  setuid(getuid()); setgid(getgid()); // drop privileges

  kimgioRegister(); // add io for additional image formats

  // deal with shell/command ////////////////////////////
  int login_shell=0;
  int welcome=1;
  char* shell = getenv("SHELL");
  char* wname = PACKAGE;
  if (shell == NULL || *shell == '\0') shell = "/bin/sh";

  QString sz = "";

  QStrList eargs;
  eargs.append(shell);

  setlocale( LC_ALL, "" );
  KApplication a(argc, argv, PACKAGE);

  for (int i = 1; i < argc; i++)
  {
    if (!strcmp(argv[i],"-e") && i+1 < argc) // handle command
    {
      if (login_shell) fprintf(stderr,"-e excludes -ls.\n");
      login_shell = 0; // does not make sense here.
      eargs.clear();
      int j;
      for (j = 0; j+i+1 < argc; j++) eargs.append( argv[i+j+1] );
      break;
    }
    if (!strcmp(argv[i],"-vt_sz") && i+1 < argc) sz = argv[++i];
    if (!strcmp(argv[i],"-sl") && i+1 < argc)  {
      QString a(argv[++i]);
      maxHistLines = a.toInt();
    }
    if (!strcmp(argv[i],"-name") && i+1 < argc) wname = argv[++i];
    if (!strcmp(argv[i],"-ls") ) login_shell=1;
    if (!strcmp(argv[i],"-nowelcome")) welcome=0;
    if (!strcmp(argv[i],"-h")) { usage(); exit(0); }
    if (!strcmp(argv[i],"-help")) { usage(); exit(0); }
    if (!strcmp(argv[i],"--help")) { usage(); exit(0); }
    //FIXME: more: font, menu, scrollbar, schema, session ...
  }
  // ///////////////////////////////////////////////

  putenv("COLORTERM="); //FIXME: for mc, which cannot detect color terminals

  int c = 0, l = 0;
  if ( (strcmp("", sz) != 0) )
  { char *ls = strchr( sz, 'x' );
    if ( ls != NULL )
    { *ls='\0'; ls++; c=atoi(sz); l=atoi(ls); }
    else
    { fprintf(stderr, "expected -vt_sz <#columns>x<#lines> ie. 80x40\n" ); }
  }
  if (a.isRestored())
  {
    KConfig * sessionconfig = a.getSessionConfig();
    sessionconfig->setGroup("options");
    sessionconfig->readListEntry("konsolearguments", eargs);
    wname = sessionconfig->readEntry("class",wname).data();
    RESTORE( TEDemo(wname,eargs,login_shell) )
  }
  else
  {  
    TEDemo*  m = new TEDemo(wname,eargs,login_shell);
    m->setColLin(c,l); // will use default height and width if called with (0,0)

    if (welcome)
    {
      m->setCaption(i18n("Welcome to the console"));
      QTimer::singleShot(5000,m,SLOT(setHeader()));
    }
    m->show();
  }

  return a.exec();
}

#include "main.moc"

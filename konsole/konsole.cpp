/* ---------------------------------------------------------------------- */
/*                                                                        */
/* [konsole.cpp]                   Konsole                                */
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
    Everything is created as late as possible to make it startup fast.
*/

/*TODO:
  - allow to set codec
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
  - Controlling the widget is currently done by individual attributes.
    This lead to quite some amount of flicker when a whole bunch of
    attributes has to be set, e.g. in session swapping.
*/

#include <config.h>

#include <ktoolbarbutton.h>

#include <qspinbox.h>
#include <qcheckbox.h>
#include <qlayout.h>

#include <stdio.h>
#include <stdlib.h>

#include <kfiledialog.h>

#include <kfontdialog.h>
#include <kstandarddirs.h>
#include <qpainter.h>
#include <kmenubar.h>
#include <kmessagebox.h>
#include <krootpixmap.h>
#include <kstdaction.h>
#include <klineeditdlg.h>
#include <kdebug.h>
#include <kipc.h>
#include <dcopclient.h>

#include <klocale.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>

#include <kiconloader.h>
#include <kstringhandler.h>
#include <ktip.h>

#include "konsole.h"
#include <netwm.h>

#define KONSOLEDEBUG    kdDebug(1211)

// KonsoleFontSelectAction is now also used for selectSize!
class KonsoleFontSelectAction : public KSelectAction {
public:
    KonsoleFontSelectAction(const QString &text, int accel,
                            const QObject* receiver, const char* slot,
                            QObject* parent, const char* name = 0 )
        : KSelectAction(text, accel, receiver, slot, parent, name) {}
    KonsoleFontSelectAction( const QString &text, const QIconSet& pix,
                             int accel, const QObject* receiver,
                             const char* slot, QObject* parent,
                             const char* name = 0 )
        : KSelectAction(text, pix, accel, receiver, slot, parent, name) {}

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
 "13", 
 "7",   // tiny font, never used
 "10",  // small font
 "13",  // medium
 "15",  // large
 "20", // huge
 "-misc-console-medium-r-normal--16-160-72-72-c-160-iso10646-1", // "Linux"
 "-misc-fixed-medium-r-normal--15-140-75-75-c-90-iso10646-1",    // "Unicode"
 };
#define TOPFONT (sizeof(fonts)/sizeof(char*))
#define DEFAULTFONT TOPFONT

#define DEFAULT_HISTORY_SIZE 1000


Konsole::Konsole(const char* name, const QString& _program, QStrList & _args, int histon,
                 bool menubaron, bool toolbaron, bool frameon, bool scrollbaron, const QString &_icon,
                 const QString &_title, QCString type, const QString &_term, bool b_inRestore)
:KMainWindow(0, name),
DCOPObject( "konsole" )
,m_defaultSession(0)
,m_defaultSessionFilename("")
,te(0)
,se(0)
,se_previous(0)
,m_initialSession(0)
,colors(0)
,rootxpm(0)
,kWinModule(0)
,menubar(0)
,statusbar(0)
,m_session(0)
,m_edit(0)
,m_view(0)
,m_options(0)
,m_schema(0)
,m_keytab(0)
,m_toolbarSessionsCommands(0)
,m_signals(0)
,m_help(0)
,m_rightButton(0)
,monitorActivity(0)
,monitorSilence(0)
,masterMode(0)
,showToolbar(0)
,showMenubar(0)
,showScrollbar(0)
,selectSize(0)
,selectFont(0)
,selectScrollbar(0)
,selectBell(0)
,m_clearHistory(0)
,m_findHistory(0)
,m_saveHistory(0)
,m_moveSessionLeft(0)
,m_moveSessionRight(0)
,m_finddialog(0)
,m_find_pattern("")
,cmd_serial(0)
,cmd_first_screen(-1)
,n_keytab(0)
,n_defaultKeytab(0)
,n_render(0)
,curr_schema(0)
,wallpaperSource(0)
,sessionIdCounter(0)
,s_kconfigSchema("")
,b_fullscreen(false)
,m_menuCreated(false)
,skip_exit_query(false) // used to skip the query when closed by the session management
,b_warnQuit(false)
,m_histSize(DEFAULT_HISTORY_SIZE)
{
  isRestored = b_inRestore;
  connect( kapp,SIGNAL(backgroundChanged(int)),this, SLOT(slotBackgroundChanged(int)));

  no2command.setAutoDelete(true);
  no2filename.setAutoDelete(true);
  menubar = menuBar();

  // create terminal emulation framework ////////////////////////////////////

  te = new TEWidget(this);
  //KONSOLEDEBUG<<"Konsole ctor() after new TEWidget() "<<time.elapsed()<<" msecs elapsed"<<endl;
  te->setMinimumSize(150,70);    // allow resizing, cause resize in TEWidget
  // we need focus so that the auto-hide cursor feature works (Carsten)
  // but a part shouldn't force that it receives the focus, so we do it here (David)
  te->setFocus();

  // Transparency handler ///////////////////////////////////////////////////
  rootxpm = new KRootPixmap(te);
  //KONSOLEDEBUG<<"Konsole ctor() after new RootPixmap() "<<time.elapsed()<<" msecs elapsed"<<endl;

  // create applications /////////////////////////////////////////////////////

  setCentralWidget(te);

  makeBasicGUI();
  //KONSOLEDEBUG<<"Konsole ctor() after makeBasicGUI "<<time.elapsed()<<" msecs elapsed"<<endl;

  colors = new ColorSchemaList();
  colors->checkSchemas();

  KeyTrans::loadAll();
  //KONSOLEDEBUG<<"Konsole ctor() after KeyTrans::loadAll() "<<time.elapsed()<<" msecs elapsed"<<endl;

  // read and apply default values ///////////////////////////////////////////
  resize(321, 321); // Dummy.
  QSize currentSize = size();
  KConfig * config = KGlobal::config();
  config->setDesktopGroup();
  applyMainWindowSettings(config);
  if (currentSize != size())
     defaultSize = size();
  //KONSOLEDEBUG<<"Konsole ctor(): readProps() type="<<type<<endl;

  KSimpleConfig *co;
  if (!type.isEmpty())
    setDefaultSession(type+".desktop");
  co = defaultSession();

  co->setDesktopGroup();
  QString schema = co->readEntry("Schema");
  //KONSOLEDEBUG << "my Looking for schema " << schema << endl;
  readProperties(config, schema, false);
  //KONSOLEDEBUG<<"Konsole ctor() after readProps "<<time.elapsed()<<" msecs elapsed"<<endl;
  //KONSOLEDEBUG<<"Konsole ctor(): toolbar"<<endl;

  if (!menubaron)
    menubar->hide();
  if (!toolbaron)
    toolBar()->hide();
  toolBar()->setText(i18n("Session Toolbar"));
  if (!frameon) {
    b_framevis=false; 
    te->setFrameStyle( QFrame::NoFrame );
  }
  if (!scrollbaron) {
    n_scroll = TEWidget::SCRNONE;
    te->setScrollbarLocation(TEWidget::SCRNONE);
  }
  if (!histon)
    b_histEnabled=false;

  // activate and run first session //////////////////////////////////////////
  // FIXME: this slows it down if --type is given, but prevents a crash (malte)
  //KONSOLEDEBUG << "Konsole pgm: " << _program << endl;
  newSession(co, _program, _args, _term, _icon, _title);

  //KONSOLEDEBUG<<"Konsole ctor() ends "<<time.elapsed()<<" msecs elapsed"<<endl;
  //KONSOLEDEBUG<<"Konsole ctor(): done"<<endl;

  kapp->dcopClient()->setDefaultObject( "konsole" );
}

Konsole::~Konsole()
{
//FIXME: close all session properly and clean up
    // Delete the session if isn't in the session list any longer.
    sessions.setAutoDelete(true);

    if (no2command.isEmpty())
       delete m_defaultSession;

    delete colors;
    colors=0;

    delete kWinModule;
    kWinModule = 0;
}

void Konsole::run() {
   kWinModule = new KWinModule();
   connect( kWinModule,SIGNAL(currentDesktopChanged(int)), this,SLOT(currentDesktopChanged(int)) );
}

void Konsole::showTip()
{
   KTipDialog::showTip(this,QString::null,true);
}

void Konsole::showTipOnStart()
{
   KTipDialog::showTip(this);
}

/* ------------------------------------------------------------------------- */
/*  Make menu                                                                */
/* ------------------------------------------------------------------------- */

void Konsole::makeGUI()
{
   if (m_menuCreated) return;
   //not longer needed
   disconnect(m_toolbarSessionsCommands,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   disconnect(m_session,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   disconnect(m_options,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   disconnect(m_help,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   disconnect(m_rightButton,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   disconnect(m_edit,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   disconnect(m_view,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   //KONSOLEDEBUG<<"Konsole::makeGUI()"<<endl;
   connect(m_toolbarSessionsCommands,SIGNAL(aboutToShow()),this,SLOT(loadScreenSessions()));
   connect(m_session,SIGNAL(aboutToShow()),this,SLOT(loadScreenSessions()));
   m_menuCreated=true;

   // Remove the empty separator Qt inserts if the menu is empty on popup,
   // not sure if this will be "fixed" in Qt, for now use this hack (malte)
   if(!(isRestored)) {
     if (sender()->inherits("QPopupMenu") &&
       static_cast<const QPopupMenu *>(sender())->count() == 1)
       const_cast<QPopupMenu *>(static_cast<const QPopupMenu *>(sender()))->removeItemAt(0);
       }

   KActionCollection* actions = new KActionCollection(this);

   // Send Signal Menu -------------------------------------------------------------
   m_signals = new KPopupMenu(this);
   m_signals->insertItem( i18n( "&Suspend Task" )   + " (STOP)", SIGSTOP);
   m_signals->insertItem( i18n( "&Continue Task" )  + " (CONT)", SIGCONT);
   m_signals->insertItem( i18n( "&Hangup" )         + " (HUP)", SIGHUP);
   m_signals->insertItem( i18n( "&Interrupt Task" ) + " (INT)", SIGINT);
   m_signals->insertItem( i18n( "&Terminate Task" ) + " (TERM)", SIGTERM);
   m_signals->insertItem( i18n( "&Kill Task" )      + " (KILL)", SIGKILL);
   connect(m_signals, SIGNAL(activated(int)), SLOT(sendSignal(int)));

   // Edit Menu ----------------------------------------------------------------
   KAction *pasteClipboard = new KAction(i18n("&Paste"), "editpaste", 0,
     te, SLOT(pasteClipboard()), actions);
   pasteClipboard->plug(m_edit);

   m_edit->setCheckable(TRUE);
   m_edit->insertItem( i18n("&Send Signal"), m_signals );

   m_edit->insertSeparator();
   m_findHistory = new KAction(i18n("&Find in History..."), "find", 0, this,
                                       SLOT(slotFindHistory()), actions);
   m_findHistory->setEnabled( se->history().isOn() );
   m_findHistory->plug(m_edit);

   m_saveHistory = new KAction(i18n("S&ave History As..."), "filesaveas", 0, this,
                                       SLOT(slotSaveHistory()), actions);
   m_saveHistory->setEnabled( se->history().isOn() );
   m_saveHistory->plug(m_edit);

   m_edit->insertSeparator();
   m_clearHistory = new KAction(i18n("Clear &History"), "history_clear", 0, this,
                                       SLOT(slotClearHistory()), actions);
   m_clearHistory->setEnabled( se->history().isOn() );
   m_clearHistory->plug(m_edit);

   KAction *clearAllSessionHistories = new KAction(i18n("Clear All H&istories"), "history_clear", 0,
     this, SLOT(slotClearAllSessionHistories()), actions);
   clearAllSessionHistories->plug(m_edit);

   // View Menu
   KAction *renameSession = new KAction(i18n("&Rename Session..."), 0, this,
                                        SLOT(slotRenameSession()), actions);
   renameSession->plug(m_view);

   m_view->insertSeparator();
   monitorActivity = new KToggleAction ( i18n( "Monitor for &Activity" ), "idea", 0, this,
                                     SLOT( slotToggleMonitor() ), this );
   monitorActivity->plug ( m_view );

   monitorSilence = new KToggleAction ( i18n( "Monitor for &Silence" ), "ktip", 0, this,
                                     SLOT( slotToggleMonitor() ), this );
   monitorSilence->plug ( m_view );

   masterMode = new KToggleAction ( i18n( "Send &Input to All Sessions" ), "remote", 0, this,
                                     SLOT( slotToggleMasterMode() ), this );
   masterMode->plug ( m_view );

   m_view->insertSeparator();
   m_moveSessionLeft = new KAction(i18n("&Move Session Left"), QApplication::reverseLayout() ? "forward" : "back", 0, this,
                                        SLOT(moveSessionLeft()), actions);
   m_moveSessionLeft->setEnabled( false );
   m_moveSessionLeft->plug(m_view);

   m_moveSessionRight = new KAction(i18n("M&ove Session Right"), QApplication::reverseLayout() ? "back" : "forward", 0, this,
                                        SLOT(moveSessionRight()), actions);
   m_moveSessionRight->setEnabled( false );
   m_moveSessionRight->plug(m_view);

   m_view->insertSeparator();
//   KRadioAction *ra = session2action.find(m_initialSession);
   KRadioAction *ra = session2action.find(se);
   if (ra!=0) ra->plug(m_view);

   // Schema Options Menu -----------------------------------------------------
   m_schema = new KPopupMenu(this);
   m_schema->setCheckable(TRUE);
   connect(m_schema, SIGNAL(activated(int)), SLOT(schema_menu_activated(int)));
   connect(m_schema, SIGNAL(aboutToShow()), SLOT(schema_menu_check()));

   // Keyboard Options Menu ---------------------------------------------------
   m_keytab = new KPopupMenu(this);
   m_keytab->setCheckable(TRUE);
   connect(m_keytab, SIGNAL(activated(int)), SLOT(keytab_menu_activated(int)));

   //options menu
   // Menubar on/off
   showMenubar = new KToggleAction ( i18n( "Show &Menubar" ), "showmenu", 0, this,
                                     SLOT( slotToggleMenubar() ), actions );
   showMenubar->plug ( m_options );

   // Toolbar on/off
   showToolbar = new KToggleAction ( i18n( "Show &Toolbar" ), 0, this,
                                     SLOT( slotToggleToolbar() ), actions );
   showToolbar->plug(m_options);

   // Scrollbar
   selectScrollbar = new KSelectAction(i18n("Sc&rollbar"), 0, this,
                                       SLOT(slotSelectScrollbar()), actions);
   QStringList scrollitems;
   scrollitems << i18n("&Hide") << i18n("&Left") << i18n("&Right");
   selectScrollbar->setItems(scrollitems);
   selectScrollbar->plug(m_options);

   // Fullscreen
   m_options->insertSeparator();
   m_options->insertItem( SmallIconSet( "window_fullscreen" ), i18n("F&ull-Screen"), 5);
   m_options->setItemChecked(5,b_fullscreen);
   m_options->insertSeparator();

   // Select Bell
   selectBell = new KSelectAction(i18n("&Bell"), SmallIconSet( "bell"), 0 , this,
                                  SLOT(slotSelectBell()), actions);
   QStringList bellitems;
   bellitems << i18n("&None")
             << i18n("&System Notification")
             << i18n("&Visible Bell");
   selectBell->setItems(bellitems);
   selectBell->plug(m_options);

   // Select font
   selectFont = new KonsoleFontSelectAction( i18n( "&Font" ),
          SmallIconSet( "text" ), 0, this, SLOT(slotSelectFont()), actions);
   QStringList it;
   it << i18n("&Normal")
      << i18n("&Tiny")
      << i18n("&Small")
      << i18n("&Medium")
      << i18n("&Large")
      << i18n("&Huge")
      << ""
      << i18n("L&inux")
      << i18n("&Unicode")
      << ""
      << i18n("&Custom...");
   selectFont->setItems(it);
   selectFont->plug(m_options);

   m_options->insertItem( SmallIconSet( "key_bindings" ), i18n( "&Keyboard" ), m_keytab );

   // Schema
   m_options->insertItem( SmallIconSet( "colorize" ), i18n( "Sch&ema" ), m_schema);

   // Select size
   selectSize = new KonsoleFontSelectAction(i18n("S&ize"), 0, this,
                                  SLOT(slotSelectSize()), actions);
   QStringList sizeitems;
   sizeitems << i18n("40x15 (&Small)")
      << i18n("80x24 (&VT100)")
      << i18n("80x25 (&IBM PC)")
      << i18n("80x40 (&XTerm)")
      << i18n("80x52 (IBM V&GA)")
      << ""
      << i18n("&Custom...");
   selectSize->setItems(sizeitems);
   selectSize->plug(m_options);

   KAction *historyType = new KAction(i18n("&History..."), "history", 0, this,
                                      SLOT(slotHistoryType()), actions);
   historyType->plug(m_options);

   m_options->insertSeparator();

   KAction *save_settings = KStdAction::saveOptions(this, SLOT(slotSaveSettings()), actions);
   save_settings->plug(m_options);

   m_options->insertSeparator();
   
   KAction *configure = KStdAction::preferences(this, SLOT(slotConfigure()), actions);
   configure->plug(m_options);
   
   m_options->insertTearOffHandle();

   connect(m_options, SIGNAL(activated(int)), SLOT(opt_menu_activated(int)));
   m_options->installEventFilter( this );

   //help menu
   m_help->insertSeparator(1);
   m_help->insertItem(i18n("&Tip of the Day..."), this, SLOT(showTip()), 0, -1, 2);

   //the different session types
   loadSessionCommands();
   loadScreenSessions();

   m_session->insertSeparator();
   KAction *closeSession = new KAction(i18n("&Close Session"), "fileclose", 0, this,
                                        SLOT(closeCurrentSession()), actions);
   closeSession->plug(m_session);

   m_session->insertSeparator();
   m_session->insertItem( SmallIconSet( "exit" ), i18n("&Quit"), this, SLOT( close() ) );


   connect(m_session, SIGNAL(activated(int)), SLOT(newSession(int)));

   // Right mouse button menu
   showMenubar->plug ( m_rightButton );
   m_rightButton->insertSeparator();
   pasteClipboard->plug(m_rightButton);
   m_rightButton->insertItem(i18n("&Send Signal"), m_signals);

   m_rightButton->insertSeparator();
   renameSession->plug(m_rightButton);
   m_rightButton->insertItem(i18n("S&ettings"), m_options);
   m_rightButton->insertSeparator();
   closeSession->plug(m_rightButton );
   m_rightButton->insertTearOffHandle();


   delete colors;
   colors = new ColorSchemaList();
   //KONSOLEDEBUG<<"Konsole::makeGUI(): curr_schema "<<curr_schema<<" path: "<<s_schema<<endl;
   colors->checkSchemas();
   colors->sort();
   //KONSOLEDEBUG<<"Konsole::makeGUI() updateSchemas()"<<endl;
   updateSchemaMenu();
   ColorSchema *sch=colors->find(s_schema);
   //KONSOLEDEBUG<<"Konsole::makeGUI(): curr_schema "<<curr_schema<<" path: "<<s_schema<<endl;
   if (sch)
        curr_schema=sch->numb();
   else
        curr_schema = 0;
   for (uint i=0; i<m_schema->count(); i++)
      m_schema->setItemChecked(i,false);

   m_schema->setItemChecked(curr_schema,true);
//   m_initialSession->setSchemaNo(curr_schema);
   while (se == NULL) {}
   se->setSchemaNo(curr_schema);

   // insert keymaps into menu
   //FIXME: sort
   for (int i = 0; i < KeyTrans::count(); i++)
   {
      KeyTrans* ktr = KeyTrans::find(i);
      assert( ktr );
      m_keytab->insertItem(ktr->hdr(),ktr->numb());
   }
   applySettingsToGUI();
   isRestored = false;
};

void Konsole::makeBasicGUI()
{
  //KONSOLEDEBUG<<"Konsole::makeBasicGUI()"<<endl;
  KToolBarPopupAction *newsession = new KToolBarPopupAction(i18n("&New"), "filenew",
                0 , this, SLOT(newSession()),this, KStdAction::stdName(KStdAction::New));
  newsession->plug(toolBar());
  toolBar()->insertLineSeparator();
  m_toolbarSessionsCommands = newsession->popupMenu();
  connect(m_toolbarSessionsCommands, SIGNAL(activated(int)), SLOT(newSessionToolbar(int)));

  toolBar()->setFullSize( TRUE );

  m_session = new KPopupMenu(this);
  m_edit = new KPopupMenu(this);
  m_view = new KPopupMenu(this);
  m_options = new KPopupMenu(this);
  m_help =  helpMenu(0, FALSE);
  m_rightButton = new KPopupMenu(this);

  // For those who would like to add shortcuts here, be aware that
  // ALT-key combinations are heavily used by many programs. Thus,
  // activating shortcuts here means deactivating them in the other
  // programs.

  connect(m_toolbarSessionsCommands,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  connect(m_session,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  connect(m_options,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  connect(m_help,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  connect(m_rightButton,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  connect(m_edit,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  connect(m_view,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));

  menubar->insertItem(i18n("Session") , m_session);
  menubar->insertItem(i18n("Edit"), m_edit);
  menubar->insertItem(i18n("View"), m_view);
  menubar->insertItem(i18n("Settings"), m_options);
  menubar->insertItem(i18n("Help"), m_help);
};

/**
   Make menubar available via escape sequence (Default: Ctrl+Alt+m)
 */
void Konsole::activateMenu()
{
  menubar->activateItemAt(0);
  if ( !showMenubar->isChecked() ) {
    menubar->show();
    showMenubar->setChecked(true);
  }
}

/**
   Ask for Quit confirmation - Martijn Klingens
   Asks for confirmation if there are still open shells when the 'Warn on
   Quit' option is set.
 */
bool Konsole::queryClose()
{
   if ( (!skip_exit_query) && b_warnQuit)
   {
        if( (sessions.count()>1) &&
            ( KMessageBox::warningYesNo( this,
                                         i18n( "You have open sessions (besides the current one). "
                                               "These will be killed if you continue.\n"
                                               "Are you sure you want to quit?" ),
					 i18n("Are you sure you want to quit?"),
					 i18n("Quit"), i18n("Cancel") )
	      
              == KMessageBox::No )
            ) {
            return false;
        }
    }
    // WABA: Don't close if there are any sessions left.
    // Tell them to go away.
    if (!skip_exit_query && sessions.count())
    {
        sessions.first();
        bool allOK=true;
        while(sessions.current())
        {
 	    if (!sessions.current()->sendSignal(SIGHUP))
              allOK=false;
            sessions.next();
        }
        if (!allOK)
          KMessageBox::information( this, i18n("Not all sessions could be closed. "
                                               "Please end all sessions running under other user IDs. "
                                               "In most cases typing 'exit' at the prompt will end them.") );
        return false;
    }
    // If there is no warning requested or required or if warnQuit is a NULL
    // pointer for some reason, just assume closing is safe
    return true;
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
      defaultSize = sizeForCentralWidgetSize(te->calcSize(80,24));
      notifySize(24,80); // set menu items (strange arg order !)
    }
    resize(defaultSize);
  } else {
    resize(sizeForCentralWidgetSize(te->calcSize(columns, lines)));
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
   if (!m_menuCreated)
      makeGUI();
  ( (state & ControlButton) ? m_session :
                              m_rightButton  )
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

void Konsole::saveProperties(KConfig* config) {
  uint counter=0;
  uint active=0;
  QString key;
  config->setDesktopGroup();

  if (config != KGlobal::config())
  {
     // called by the session manager
     skip_exit_query = true;
     config->writeEntry("numSes",sessions.count());
     sessions.first();
     while(counter < sessions.count())
     {
        key = QString("Title%1").arg(counter);
        config->writeEntry(key, sessions.current()->Title());
        key = QString("Schema%1").arg(counter);
        config->writeEntry(key, sessions.current()->schemaNo());
        key = QString("Args%1").arg(counter);
        config->writeEntry(key, sessions.current()->getArgs());
        key = QString("Pgm%1").arg(counter);
        config->writeEntry(key, sessions.current()->getPgm());
        key = QString("Font%1").arg(counter);
        config->writeEntry(key, sessions.current()->fontNo());
        key = QString("Term%1").arg(counter);
        config->writeEntry(key, sessions.current()->Term());
        key = QString("KeyTab%1").arg(counter);
        config->writeEntry(key, sessions.current()->keymap());
        key = QString("Icon%1").arg(counter);
        config->writeEntry(key, sessions.current()->IconName());
        key = QString("MonitorActivity%1").arg(counter);
        config->writeEntry(key, sessions.current()->isMonitorActivity());
        key = QString("MonitorSilence%1").arg(counter);
        config->writeEntry(key, sessions.current()->isMonitorSilence());
        key = QString("MasterMode%1").arg(counter);
        config->writeEntry(key, sessions.current()->isMasterMode());
        if (sessions.current()==se)
	  active=counter;
        sessions.next();
        counter++;
     }
  }
  config->setDesktopGroup();
  config->writeEntry("Fullscreen",b_fullscreen);
  config->writeEntry("font",n_defaultFont);
  config->writeEntry("defaultfont", defaultFont);
  config->writeEntry("schema",s_kconfigSchema);
  config->writeEntry("scrollbar",n_scroll);
  config->writeEntry("bellmode",n_bell);
  config->writeEntry("keytab",n_defaultKeytab);
  config->writeEntry("ActiveSession", active);
  config->writeEntry("DefaultSession", m_defaultSessionFilename);

  if (se) {
    config->writeEntry("history", se->history().getSize());
    config->writeEntry("historyenabled", b_histEnabled);
  }

  config->writeEntry("class",name());
}


// Called by constructor (with config = KGlobal::config())
// and by session-management (with config = sessionconfig).
// So it has to apply the settings when reading them.
void Konsole::readProperties(KConfig* config)
{
    readProperties(config, QString::null, false);
}

// If --type option was given, load the corresponding schema instead of
// default
//
// When globalConfigOnly is true only the options that are shared among all 
// konsoles are being read.
void Konsole::readProperties(KConfig* config, const QString &schema, bool globalConfigOnly)
{
   config->setDesktopGroup();
   //KONSOLEDEBUG<<"Konsole::readProps()"<<endl;

   if (config==KGlobal::config())
   {
     b_warnQuit=config->readBoolEntry( "WarnQuit", TRUE );

     s_word_seps= config->readEntry("wordseps",":@-./_~");
     te->setWordCharacters(s_word_seps);

     b_framevis = config->readBoolEntry("has frame",TRUE);
     te->setFrameStyle( b_framevis?(QFrame::WinPanel|QFrame::Sunken):QFrame::NoFrame );

     te->setBlinkingCursor(config->readBoolEntry("BlinkingCursor",FALSE));
     te->setCtrlDrag(config->readBoolEntry("CtrlDrag",FALSE));
     te->setTerminalSizeHint( config->readBoolEntry("TerminalSizeHint",true) );
     te->setLineSpacing( config->readUnsignedNumEntry( "LineSpacing", 0 ) );
   }

   ColorSchema* sch = 0;
   if (!globalConfigOnly)
   {
      n_defaultKeytab=config->readNumEntry("keytab",0); // act. the keytab for this session
      b_fullscreen = config->readBoolEntry("Fullscreen",FALSE);
      n_defaultFont = n_font = QMIN(config->readUnsignedNumEntry("font",3),TOPFONT);
      n_scroll   = QMIN(config->readUnsignedNumEntry("scrollbar",TEWidget::SCRRIGHT),2);
      n_bell = QMIN(config->readUnsignedNumEntry("bellmode",TEWidget::BELLSYSTEM),2);

      // Options that should be applied to all sessions /////////////
      // (1) set menu items and Konsole members
      QFont tmpFont("fixed");
      defaultFont = config->readFontEntry("defaultfont", &tmpFont);
      setFont(QMIN(config->readUnsignedNumEntry("font",3),TOPFONT));

      //set the schema
      s_kconfigSchema=config->readEntry("schema", "");
      sch = colors->find(schema.isEmpty() ? s_kconfigSchema : schema);
      if (!sch)
      {
         kdWarning() << "Could not find schema named " <<s_kconfigSchema<< endl;
         sch=(ColorSchema*)colors->at(0);  //the default one
      }
      if (sch->hasSchemaFileChanged()) sch->rereadSchemaFile();
      s_schema = sch->relPath();
      curr_schema = sch->numb();
      pmPath = sch->imagePath();
      te->setColorTable(sch->table()); //FIXME: set twice here to work around a bug

      if (sch->useTransparency())
      {
         //KONSOLEDEBUG << "Setting up transparency" << endl;
         rootxpm->setFadeEffect(sch->tr_x(), QColor(sch->tr_r(), sch->tr_g(), sch->tr_b()));
         rootxpm->start();
         rootxpm->repaint(true);
      }
      else
      {
         //KONSOLEDEBUG << "Stopping transparency" << endl;
         rootxpm->stop();
         pixmap_menu_activated(sch->alignment());
      }

      te->setColorTable(sch->table());
      te->setScrollbarLocation(n_scroll);
      te->setBellMode(n_bell);

      // History
      m_histSize = config->readNumEntry("history",DEFAULT_HISTORY_SIZE);
      b_histEnabled = config->readBoolEntry("historyenabled",true);
      //KONSOLEDEBUG << "Hist size : " << m_histSize << endl;
   }

   if (m_menuCreated)
   {
      applySettingsToGUI();
      activateSession();
   };
}

void Konsole::applySettingsToGUI()
{
   if (!m_menuCreated) return;
   selectFont->setCurrentItem(n_font);
   notifySize(te->Lines(),te->Columns());
   showToolbar->setChecked(!toolBar()->isHidden());
   showMenubar->setChecked(!menuBar()->isHidden());
   selectScrollbar->setCurrentItem(n_scroll);
   selectBell->setCurrentItem(n_bell);
   updateKeytabMenu();
};


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

void Konsole::slotSelectBell() {
  n_bell = selectBell->currentItem();
  te->setBellMode(n_bell);
}

void Konsole::slotSelectScrollbar() {
   if (m_menuCreated)
      n_scroll = selectScrollbar->currentItem();
   te->setScrollbarLocation(n_scroll);
   activateSession(); // maybe helps in bg
}

void Konsole::slotSelectFont() {
  assert(se);
  int item = selectFont->currentItem();
  // KONSOLEDEBUG << "slotSelectFont " << item << endl;
  if (item == DEFAULTFONT) 
  {
    if ( KFontDialog::getFont(defaultFont, true) == QDialog::Rejected )
    {
      selectFont->setCurrentItem(n_font);
      return;
    }
  }
  setFont(item);
  n_defaultFont = n_font; // This is the new default
  activateSession(); // activates the current
}

void Konsole::schema_menu_activated(int item)
{
  assert(se);
  //FIXME: save schema name
//        KONSOLEDEBUG << "Item " << item << " selected from schema menu"
//                << endl;
  setSchema(item);
  s_kconfigSchema = s_schema; // This is the new default
  activateSession(); // activates the current
}

/* slot */ void Konsole::schema_menu_check()
{
        if (colors->checkSchemas())
        {
                colors->sort();
                updateSchemaMenu();
        }
}

void Konsole::updateSchemaMenu()
{
//        KONSOLEDEBUG << "Updating schema menu with "
//                << colors->count()
//                << " items."
//                << endl;

  m_schema->clear();
  for (int i = 0; i < (int) colors->count(); i++)
  {
     ColorSchema* s = (ColorSchema*)colors->at(i);
    assert( s );
    m_schema->insertItem(s->title(),s->numb(),0);
  }

  if (te && te->currentSession)
  {
//        KONSOLEDEBUG << "Current session has schema "
//                << te->currentSession->schemaNo()
//                << endl;
        m_schema->setItemChecked(te->currentSession->schemaNo(),true);
  }

}

void Konsole::updateKeytabMenu()
{
  if (m_menuCreated)
  {
     m_keytab->setItemChecked(n_keytab,FALSE);
     m_keytab->setItemChecked(se->keymapNo(),TRUE);
  };
  n_keytab = se->keymapNo();
}

void Konsole::keytab_menu_activated(int item)
{
  se->setKeymapNo(item);
  n_defaultKeytab = item;
  updateKeytabMenu();
}

void Konsole::setFont(int fontno)
{
  QFont f;
  if (fontno == DEFAULTFONT)
    f = defaultFont;
  else
  if (fonts[fontno][0] == '-')
  {
    f.setRawName( fonts[fontno] );
    if ( !f.exactMatch() && fontno != DEFAULTFONT)
    {
      // Ugly hack to prevent bug #20487
      fontNotFound_par=fonts[fontno];
      QTimer::singleShot(1,this,SLOT(fontNotFound()));
      return;
    }
  }
  else
  {
    f.setFamily("fixed");
    f.setFixedPitch(true);
    f.setPixelSize(QString(fonts[fontno]).toInt());
  }
  if (se) se->setFontNo(fontno);
  if (m_menuCreated)
     selectFont->setCurrentItem(fontno);
  te->setVTFont(f);
  n_font = fontno;
}

void Konsole::fontNotFound()
{
  QString msg = i18n("Font `%1' not found.\nCheck README.linux.console for help.").arg(fontNotFound_par);
  KMessageBox::error(this,  msg);
}

/**
     Toggle the Menubar visibility
 */
void Konsole::slotToggleMenubar() {
  if ( showMenubar->isChecked() )
     menubar->show();
  else
     menubar->hide();
  if (!showMenubar->isChecked()) {
    setCaption(i18n("Use the right mouse button to bring back the menu"));
    QTimer::singleShot(5000,this,SLOT(updateTitle()));
  }
}

/**
    Toggle the Toolbar visibility
 */
void Konsole::slotToggleToolbar() {
  if (showToolbar->isChecked())
     toolBar()->show();
  else
     toolBar()->hide();
}

void Konsole::opt_menu_activated(int item)
{
  switch( item )  {
    case 5: setFullScreen(!b_fullscreen);
            break;
  }
}

void Konsole::slotSaveSettings()
{
  KConfig *config = KGlobal::config();
  config->setDesktopGroup();
  saveProperties(config);
  saveMainWindowSettings(config);
  config->sync();
}

void Konsole::slotConfigure()
{
  QStringList args;
  args << "kcmkonsole";
  KApplication::kdeinitExec( "kcmshell", args );
}

void Konsole::reparseConfiguration()
{
  KGlobal::config()->reparseConfiguration();
  readProperties(KGlobal::config(), QString::null, true);
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
    case 5: SizeDialog dlg(te->Columns(), te->Lines(), this);
            if (dlg.exec())
              setColLin(dlg.columns(),dlg.lines());
            break;
   }
}


void Konsole::notifySize(int lines, int columns)
{
   if (!m_menuCreated) return;

    selectSize->blockSignals(true);
    selectSize->setCurrentItem(-1);
    if (columns==40&&lines==15)
        selectSize->setCurrentItem(0);
    else if (columns==80&&lines==24)
        selectSize->setCurrentItem(1);
    else if (columns==80&&lines==25)
        selectSize->setCurrentItem(2);
    else if (columns==80&&lines==40)
        selectSize->setCurrentItem(3);
    else if (columns==80&&lines==52)
        selectSize->setCurrentItem(4);
    else
        selectSize->setCurrentItem(5);
    selectSize->blockSignals(false);
    if (n_render >= 3) pixmap_menu_activated(n_render);
}

void Konsole::updateTitle()
{
  setCaption( te->currentSession->fullTitle() );
  setIconText( te->currentSession->IconText() );
}

void Konsole::initFullScreen()
{
  //This function is to be called from main.C to initialize the state of the Konsole (fullscreen or not).  It doesn't appear to work
  //from inside the Konsole constructor
  if (b_fullscreen) {
   setColLin(0,0);
   }
  setFullScreen(b_fullscreen);
}

void Konsole::initSessionSchema(int schemaNo) {
  setSchema(schemaNo);
}

void Konsole::initSessionFont(int fontNo) {
  if (fontNo == -1) return; // Don't change
  setFont(fontNo);
}

void Konsole::initSessionKeyTab(const QString &keyTab) {
  se->setKeymap(keyTab);
}

void Konsole::setFullScreen(bool on)
{
//  if (on == b_fullscreen) {
//    KONSOLEDEBUG << "On and b_Fullscreen both equal " << b_fullscreen << "." << endl;
//    }
    if (on) {
      showFullScreen();
      b_fullscreen = on;
      }
    else {
      showNormal();
      updateTitle(); // restore caption of window
      b_fullscreen = false;
//      KONSOLEDEBUG << "On is false, b_fullscreen is " << b_fullscreen << ". Set to Normal view and set caption." << endl;
    }
//  return;
    m_options->setItemChecked(5,b_fullscreen);

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

void Konsole::clearAllListenToKeyPress()
{
  for (TESession *ses = sessions.first(); ses; ses = sessions.next())
    ses->setListenToKeyPress(false);
}
 
void Konsole::restoreAllListenToKeyPress()
{
  if(se->isMasterMode())
    for (TESession *ses = sessions.first(); ses; ses = sessions.next())
      ses->setListenToKeyPress(TRUE);
  else
    se->setListenToKeyPress(TRUE);
}

void Konsole::feedAllSessions(const QString &text)
{
  for (TESession *ses = sessions.first(); ses; ses = sessions.next())
    ses->setListenToKeyPress(TRUE);
  if (te)
    te->emitText(text);
  if(!se->isMasterMode()) {
    for (TESession *ses = sessions.first(); ses; ses = sessions.next())
      ses->setListenToKeyPress(FALSE);
    se->setListenToKeyPress(TRUE);
  }
}

void Konsole::sendAllSessions(const QString &text)
{
  QString newtext=text;
  newtext.append("\r");
  feedAllSessions(newtext);
}

void Konsole::sendSignal(int sn)
{
  if (se) se->sendSignal(sn);
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
  QString newTitle = s->Title();

  bool nameOk;
  int count = 1;
  do {
     nameOk = true;
     for (TESession *ses = sessions.first(); ses; ses = sessions.next())
     {
        if (newTitle == ses->Title())
        {
           nameOk = false;
           break;
        }
     }
     if (!nameOk)
     {
       count++;
       newTitle = i18n("%1 No. %2").arg(s->Title()).arg(count);
     }
  }
  while (!nameOk);

  s->setTitle(newTitle);

  // create an action for the session
  //  char buffer[30];
  //  int acc = CTRL+SHIFT+Key_0+session_no; // Lars: keys stolen by kwin.
  KRadioAction *ra = new KRadioAction(newTitle,
                                      s->IconName(),
                                      0,
                                      this,
                                      SLOT(activateSession()),
                                      this);
                                      //                                      buffer);
  ra->setExclusiveGroup("sessions");
  ra->setChecked(true);
  // key accelerator
  //  accel->connectItem(accel->insertItem(acc), ra, SLOT(activate()));

  action2session.insert(ra, s);
  session2action.insert(s,ra);
  sessions.append(s);
  if (m_menuCreated)
     ra->plug(m_view);

  int button_id=ra->itemId( ra->plug(toolBar()) );
  KToolBarButton* ktb=toolBar()->getButton(button_id);  
  connect(ktb,SIGNAL(doubleClicked(int)), this,SLOT(slotRenameSession(int)));
  session2button.insert(s,ktb);
}

QString Konsole::currentSession()
{
  return se->SessionId();
}

QString Konsole::sessionId(const int position)
{
  if (position<=0 || position>sessions.count())
    return "";
  
  return sessions.at(position-1)->SessionId();
}

void Konsole::activateSession(const int position)
{
  if (position<=0 || position>sessions.count())
    return;
  activateSession( sessions.at(position-1) );
}

void Konsole::activateSession(const QString &sessionId)
{
  TESession* activate=NULL;

  sessions.first();
  while(sessions.current())
  {
    if (sessions.current()->SessionId()==sessionId)
      activate=sessions.current();
    sessions.next();
  }

  if (activate)
    activateSession( activate );
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
  {
     se->setConnect(FALSE);
     if(se->isMasterMode())
       for (TESession *se = sessions.first(); se; se = sessions.next())
         se->setListenToKeyPress(FALSE);

     QObject::disconnect( se->getEmulation(),SIGNAL(prevSession()), this,SLOT(prevSession()) );
     QObject::disconnect( se->getEmulation(),SIGNAL(nextSession()), this,SLOT(nextSession()) );
     QObject::disconnect( se->getEmulation(),SIGNAL(newSession()), this,SLOT(newSession()) );
     QObject::disconnect( se->getEmulation(),SIGNAL(renameSession()), this,SLOT(slotRenameSession()) );
     QObject::disconnect( se->getEmulation(),SIGNAL(activateMenu()), this,SLOT(activateMenu()) );
     QObject::disconnect( se->getEmulation(),SIGNAL(moveSessionLeft()), this,SLOT(moveSessionLeft()) );
     QObject::disconnect( se->getEmulation(),SIGNAL(moveSessionRight()), this,SLOT(moveSessionRight()) );
     notifySessionState(se,NOTIFYNORMAL);
     // Delete the session if isn't in the session list any longer.
     if (sessions.find(se) == -1)
        delete se;
  }
  se_previous = se;
  se = s;
  session2action.find(se)->setChecked(true);
  QTimer::singleShot(1,this,SLOT(allowPrevNext())); // hack, hack, hack
  if (s->schemaNo()!=curr_schema)
  {
     // the current schema has changed
     setSchema(s->schemaNo());
  }

  te->currentSession = se;
  if (s->fontNo() != n_font)
  {
      setFont(s->fontNo());
  }
  s->setConnect(TRUE);
  if(se->isMasterMode())
    for (TESession *se = sessions.first(); se; se = sessions.next())
      se->setListenToKeyPress(TRUE);
  updateTitle();
  if (!m_menuCreated)
    makeGUI();
  updateKeytabMenu(); // act. the keytab for this session
  m_clearHistory->setEnabled( se->history().isOn() );
  m_findHistory->setEnabled( se->history().isOn() );
  se->getEmulation()->findTextBegin();
  m_saveHistory->setEnabled( se->history().isOn() );
  monitorActivity->setChecked( se->isMonitorActivity() );
  monitorSilence->setChecked( se->isMonitorSilence() );
  masterMode->setChecked( se->isMasterMode() );
  sessions.find(se);
  uint position=sessions.at();
  m_moveSessionLeft->setEnabled(position>0);
  m_moveSessionRight->setEnabled(position<sessions.count()-1);
}

void Konsole::allowPrevNext()
{
  QObject::connect( se->getEmulation(),SIGNAL(prevSession()), this,SLOT(prevSession()) );
  QObject::connect( se->getEmulation(),SIGNAL(nextSession()), this,SLOT(nextSession()) );
  QObject::connect( se->getEmulation(),SIGNAL(newSession()), this,SLOT(newSession()) );
  QObject::connect( se->getEmulation(),SIGNAL(renameSession()), this,SLOT(slotRenameSession()) );
  QObject::connect( se->getEmulation(),SIGNAL(activateMenu()), this,SLOT(activateMenu()) );
  QObject::connect( se->getEmulation(),SIGNAL(moveSessionLeft()), this,SLOT(moveSessionLeft()) );
  QObject::connect( se->getEmulation(),SIGNAL(moveSessionRight()), this,SLOT(moveSessionRight()) );
  notifySessionState(se,NOTIFYNORMAL);
}

KSimpleConfig *Konsole::defaultSession()
{
  if (!m_defaultSession)
    setDefaultSession("shell.desktop");
  return m_defaultSession;
}

void Konsole::setDefaultSession(const QString &filename)
{
  delete m_defaultSession;
  m_defaultSession = new KSimpleConfig(locate("appdata", filename), true /* read only */);
  m_defaultSessionFilename=filename;
}

void Konsole::newSession(const QString &pgm, const QStrList &args, const QString &term, const QString &icon)
{
  KSimpleConfig *co = defaultSession();
  newSession(co, pgm, args, term, icon);
}

QString Konsole::newSession()
{
  KSimpleConfig *co = defaultSession();
  return newSession(co, QString::null, QStrList());
}

void Konsole::newSession(int i)
{
  KSimpleConfig* co = no2command.find(i);
  if (co) newSession(co);
}

void Konsole::newSessionToolbar(int i)
{
  KSimpleConfig* co = no2command.find(i);
  if (co) {
    setDefaultSession(*no2filename.find(i));
    newSession(co);
  }
}

QString Konsole::newSession(const QString &type)
{
  KSimpleConfig *co;
  if (type.isEmpty())
     co = defaultSession();
  else
     co = new KSimpleConfig(locate("appdata", type + ".desktop"), true /* read only */);
  return newSession(co);
}

QString Konsole::newSession(KSimpleConfig *co, QString program, const QStrList &args, const QString &_term,const QString &_icon,const QString &_title)
{
  QString emu = "xterm";
  QString icon = "openterm";
  QString key;
  QString sch = s_kconfigSchema;
  QString txt;
  QString cwd;
  unsigned int     fno = n_defaultFont;
  QStrList cmdArgs;

  if (co) {
     co->setDesktopGroup();
     emu = co->readEntry("Term", emu);
     key = co->readEntry("KeyTab", key);
     sch = co->readEntry("Schema", sch);
     txt = co->readEntry("Name");
     fno = co->readUnsignedNumEntry("Font", fno);
     icon = co->readEntry("Icon", icon);
     cwd = co->readPathEntry("Cwd");
  }

  if (!_term.isEmpty())
     emu = _term;

  if (!_icon.isEmpty())
     icon = _icon;

  if (!_title.isEmpty())
     txt = _title;

  if (!program.isEmpty()) {
     cmdArgs = args;
  }
  else {
     program = QFile::decodeName(konsole_shell(cmdArgs));

     if (co) {
        co->setDesktopGroup();
        QString cmd = co->readEntry("Exec");

        if (!cmd.isEmpty()) {
          cmdArgs.append("-c");
          cmdArgs.append(QFile::encodeName(cmd));
        }
     }
  }

  ColorSchema* schema = colors->find(sch);
  if (!schema)
      schema=(ColorSchema*)colors->at(0);  //the default one
  int schmno = schema->numb();

  QString sessionId="session-"+QString::number(++sessionIdCounter);
  TESession* s = new TESession(this,te, QFile::encodeName(program),cmdArgs,emu,sessionId,cwd);
  connect( s,SIGNAL(done(TESession*,int)),
           this,SLOT(doneSession(TESession*,int)) );
  connect( te, SIGNAL(configureRequest(TEWidget*, int, int, int)),
           this, SLOT(configureRequest(TEWidget*,int,int,int)) );
  connect( s, SIGNAL( updateTitle() ),
           this, SLOT( updateTitle() ) );
  connect( s, SIGNAL( notifySessionState(TESession*, int) ),
           this, SLOT( notifySessionState(TESession*, int)) );

  s->setFontNo(QMIN(fno, TOPFONT));
  s->setSchemaNo(schmno);
  if (key.isEmpty())
    s->setKeymapNo(n_defaultKeytab);
  else
    s->setKeymap(key);
  s->setTitle(txt);
  s->setIconName(icon);

  if (b_histEnabled && m_histSize)
    s->setHistory(HistoryTypeBuffer(m_histSize));
  else if (b_histEnabled && !m_histSize)
    s->setHistory(HistoryTypeFile());
  else
    s->setHistory(HistoryTypeNone());

  addSession(s);
  runSession(s); // activate and run

  return sessionId;
}

void Konsole::closeCurrentSession()
{
  if( sessions.count()>1 )
    se->sendSignal(SIGHUP);
  else
    emit close();
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
  ra->unplug(m_view);
  ra->unplug(toolBar());
  session2action.remove(s);
  session2button.remove(s);
  action2session.remove(ra);
  int sessionIndex = sessions.findRef(s);
  sessions.remove(s);
  delete ra; // will the toolbar die?

  s->setConnect(FALSE);
  if(s->isMasterMode())
    for (TESession *se = sessions.first(); se; se = sessions.next())
      se->setListenToKeyPress(FALSE);

  delete s;
  if (s == se_previous)
    se_previous=NULL;
  if (s == se)
  { // pick a new session
    se = 0;
    if (sessions.count())
    {
      if (se_previous)
        se = se_previous;
      else
        se = sessions.at(sessionIndex ? sessionIndex - 1 : 0);
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
  else {
    sessions.find(se);
    uint position=sessions.at();
    m_moveSessionLeft->setEnabled(position>0);
    m_moveSessionRight->setEnabled(position<sessions.count()-1);
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

/* Move session forward in session list if possible */
void Konsole::moveSessionLeft()
{
  sessions.find(se);
  uint position=sessions.at();
  if (position==0)
    return;

  sessions.remove(position);
  sessions.insert(position-1,se);

  KRadioAction *ra = session2action.find(se);
  ra->unplug(m_view);
  ra->plug(m_view,(m_view->count()-sessions.count()+1)+position-1);

  ra->unplug(toolBar());
  session2button.remove(se);
  int button_id=ra->itemId( ra->plug(toolBar(),position-1+2 ));  // +2 because of "New" and separator
  KToolBarButton* ktb=toolBar()->getButton(button_id);
  if(se->isMasterMode())
    ktb->setIcon("remote");
  connect(ktb,SIGNAL(doubleClicked(int)), this,SLOT(slotRenameSession(int)));
  session2button.insert(se,ktb);

  if (!m_menuCreated)
    makeGUI();
  m_moveSessionLeft->setEnabled(position-1>0);
  m_moveSessionRight->setEnabled(true);
}

/* Move session back in session list if possible */
void Konsole::moveSessionRight()
{
  sessions.find(se);
  uint position=sessions.at();

  if (position==sessions.count()-1)
    return;

  sessions.remove(position);
  sessions.insert(position+1,se);

  KRadioAction *ra = session2action.find(se);
  ra->unplug(m_view);
  ra->plug(m_view,(m_view->count()-sessions.count()+1)+position+1);

  ra->unplug(toolBar());
  session2button.remove(se);
  int button_id=ra->itemId( ra->plug(toolBar(),position+1+2) );  // +2 because of "New" and separator
  KToolBarButton* ktb=toolBar()->getButton(button_id);
  if(se->isMasterMode())
    ktb->setIcon("remote");
  connect(ktb,SIGNAL(doubleClicked(int)), this,SLOT(slotRenameSession(int)));
  session2button.insert(se,ktb);

  if (!m_menuCreated)
    makeGUI();
  m_moveSessionLeft->setEnabled(true);
  m_moveSessionRight->setEnabled(position+1<sessions.count()-1);
}

void Konsole::initMonitorActivity(bool state)
{
  monitorActivity->setChecked(state);
  slotToggleMonitor();
}

void Konsole::initMonitorSilence(bool state)
{
  monitorSilence->setChecked(state);
  slotToggleMonitor();
}

void Konsole::slotToggleMonitor()
{
  se->setMonitorActivity( monitorActivity->isChecked() );
  se->setMonitorSilence( monitorSilence->isChecked() );
  notifySessionState(se,NOTIFYNORMAL);
}

void Konsole::initMasterMode(bool state)
{
  masterMode->setChecked(state);
  slotToggleMasterMode();
}

void Konsole::slotToggleMasterMode()
{
  bool _masterMode=masterMode->isChecked();
  se->setMasterMode( _masterMode );
  if(_masterMode)
    for (TESession *ses = sessions.first(); ses; ses = sessions.next())
      ses->setListenToKeyPress(TRUE);
  else {
    for (TESession *ses = sessions.first(); ses; ses = sessions.next())
      ses->setListenToKeyPress(FALSE);
    se->setListenToKeyPress(TRUE);
  }
  notifySessionState(se,NOTIFYNORMAL);
}

void Konsole::notifySessionState(TESession* session, int state)
{
  KToolBarButton* ktb=session2button.find(session);
  QString state_iconname;
  switch(state)
  {
    case NOTIFYNORMAL  : if(session->isMasterMode())
                           state_iconname = "remote";
                         else
                           state_iconname = session->IconName();
                         break;
    case NOTIFYBELL    : state_iconname = "bell";
                         break;
    case NOTIFYACTIVITY: state_iconname = "idea";
                         break;
    case NOTIFYSILENCE : state_iconname = "ktip";
			 break;
  }
  if (!state_iconname.isEmpty()
      && session->testAndSetStateIconName(state_iconname))
    ktb->setIcon (state_iconname);
}

// --| Session support |-------------------------------------------------------

void Konsole::clearSessionHistory(TESession & session)
{
  if (b_histEnabled == true) {
    session.setHistory(HistoryTypeNone());
    if (m_histSize)
      session.setHistory(HistoryTypeBuffer(m_histSize));
    else
      session.setHistory(HistoryTypeFile());
  }
}

void Konsole::addSessionCommand(const QString &path)
{
  KSimpleConfig* co;
  QString filename=path;
  if (path.isEmpty()) {
    co = new KSimpleConfig(locate("appdata", "shell.desktop"), true /* read only */);
    filename="shell.desktop";
  }
  else
    co = new KSimpleConfig(path,TRUE);
  co->setDesktopGroup();
  QString typ = co->readEntry("Type");
  QString txt = co->readEntry("Name");
  if (typ.isEmpty() || txt.isEmpty() || typ != "KonsoleApplication")
  {
    if (!path.isEmpty())
       delete co; 
    return; // ignore
  }
  QString icon = co->readEntry("Icon", "openterm");
  m_toolbarSessionsCommands->insertItem( SmallIconSet( icon ), txt, ++cmd_serial );
  QString comment = co->readEntry("Comment");
  if (comment.isEmpty())
    comment=txt.prepend(i18n("New "));
  m_session->insertItem( SmallIconSet( icon ), comment, cmd_serial );
  no2command.insert(cmd_serial,co);

  int j = filename.findRev('/');
  if (j > -1)
    filename = filename.mid(j+1);
  no2filename.insert(cmd_serial,new QString(filename));
}

void Konsole::loadSessionCommands()
{
  addSessionCommand(QString::null);
  m_session->insertSeparator();
  QStringList lst = KGlobal::dirs()->findAllResources("appdata", "*.desktop", false, true);

  for(QStringList::Iterator it = lst.begin(); it != lst.end(); ++it )
    if (!(*it).endsWith("/shell.desktop"))
       addSessionCommand(*it);
}

void Konsole::addScreenSession(const QString &socket)
{
  // In-memory only
  KSimpleConfig *co = new KSimpleConfig(QString::null, true);
  co->setDesktopGroup();
  co->writeEntry("Name", socket);
  QString txt = i18n("Screen is a program controlling screens!", "Screen at %1").arg(socket);
  co->writeEntry("Comment", txt);
  co->writeEntry("Exec", QString::fromLatin1("screen -r %1").arg(socket));
  QString icon = "openterm"; // FIXME use another icon (malte)
  cmd_serial++;
  m_session->insertItem( SmallIconSet( icon ), txt, cmd_serial, cmd_serial - 1 );
  m_toolbarSessionsCommands->insertItem( SmallIconSet( icon ), txt, cmd_serial );
  no2command.insert(cmd_serial,co);
  no2filename.insert(cmd_serial,new QString(""));
}

void Konsole::loadScreenSessions()
{
  QCString screenDir = getenv("SCREENDIR");
  if (screenDir.isEmpty())
    screenDir = QFile::encodeName(QDir::homeDirPath()) + "/.screen/";
  QStringList sessions;
  // Can't use QDir as it doesn't support FIFOs :(
  DIR *dir = opendir(screenDir);
  if (dir)
  {
    struct dirent *entry;
    while ((entry = readdir(dir)))
    {
      QCString path = screenDir + "/" + entry->d_name;
      struct stat st;
      if (stat(path, &st) != 0)
        continue;

      int fd;
      if (S_ISFIFO(st.st_mode) && !(st.st_mode & 0111) && // xbit == attached
          (fd = open(path, O_WRONLY | O_NONBLOCK)) != -1)
      {
        ::close(fd);
        sessions.append(QFile::decodeName(entry->d_name));
      }
    }
    closedir(dir);
  }
  if (cmd_first_screen == -1)
    cmd_first_screen = cmd_serial + 1;
  else
  {
    for (int i = cmd_first_screen; i <= cmd_serial; ++i)
    {
      m_session->removeItem(i);
      m_toolbarSessionsCommands->removeItem(i);
      no2command.remove(i);
      no2filename.remove(i);
    }
    cmd_serial = cmd_first_screen - 1;
  }
  for (QStringList::ConstIterator it = sessions.begin(); it != sessions.end(); ++it)
    addScreenSession(*it);
}

// --| Schema support |-------------------------------------------------------

void Konsole::setSchema(int numb)
{
  ColorSchema* s = colors->find(numb);
  if (!s)
  {
        kdWarning() << "No schema found. Using default." << endl;
        s=(ColorSchema*)colors->at(0);
  }
  if (s->numb() != numb)
  {
        kdWarning() << "No schema with number " << numb << endl;
  }

  if (s->hasSchemaFileChanged())
  {
        const_cast<ColorSchema *>(s)->rereadSchemaFile();
  }
  if (s) setSchema(s);
}

void Konsole::setSchema(const QString & path)
{
  ColorSchema* s = colors->find(path);
  if (!s)
  {
        kdWarning() << "Could not find schema named " << path << endl;
        s=(ColorSchema*)colors->at(0);
  }
  if (s->hasSchemaFileChanged())
  {
        const_cast<ColorSchema *>(s)->rereadSchemaFile();
  }
  if (s) setSchema(s);
}

void Konsole::setSchema(ColorSchema* s)
{
  if (!s) return;

//        KONSOLEDEBUG << "Checking menu items" << endl;
  if (m_schema)
  {
    m_schema->setItemChecked(curr_schema,FALSE);
    m_schema->setItemChecked(s->numb(),TRUE);
  }
//        KONSOLEDEBUG << "Remembering schema data" << endl;

  s_schema = s->relPath();
  curr_schema = s->numb();
  pmPath = s->imagePath();
  te->setColorTable(s->table()); //FIXME: set twice here to work around a bug

  if (s->useTransparency()) {
//        KONSOLEDEBUG << "Setting up transparency" << endl;
    rootxpm->setFadeEffect(s->tr_x(), QColor(s->tr_r(), s->tr_g(), s->tr_b()));
    rootxpm->start();
    rootxpm->repaint(true);
  } else {
//        KONSOLEDEBUG << "Stopping transparency" << endl;
    rootxpm->stop();
    pixmap_menu_activated(s->alignment());
  }

  te->setColorTable(s->table());
  if (se) se->setSchemaNo(s->numb());
}

void Konsole::slotRenameSession() {
//  KONSOLEDEBUG << "slotRenameSession\n";
  KRadioAction *ra = session2action.find(se);
  QString name = se->Title();
  KLineEditDlg dlg(i18n("Session name"),name, this);
  dlg.setCaption(i18n("Rename Session"));
  if (dlg.exec()) {
    se->setTitle(dlg.text());
    ra->setText(dlg.text());
    ra->setIcon( se->IconName() ); // I don't know why it is needed here
    if(se->isMasterMode())
      session2button.find(se)->setIcon("remote");
    toolBar()->updateRects();
    updateTitle();
  }
}

void Konsole::slotRenameSession(int) {
  slotRenameSession();
}

void Konsole::slotRenameSession(TESession* ses, const QString &name)
{
  KRadioAction *ra = session2action.find(ses);
  ses->setTitle(name);
  ra->setText(name);
  ra->setIcon( ses->IconName() ); // I don't know why it is needed here
  if(ses->isMasterMode())
    session2button.find(ses)->setIcon("remote");
  toolBar()->updateRects();
  updateTitle();
}

void Konsole::initSessionTitle(const QString &_title) {
  KRadioAction *ra = session2action.find(se);

  se->setTitle(_title);
  ra->setText(_title);
  ra->setIcon( se->IconName() ); // I don't know why it is needed here
  toolBar()->updateRects();
  updateTitle();
}

void Konsole::slotClearAllSessionHistories() {
  for (TESession *se = sessions.first(); se; se = sessions.next())
    clearSessionHistory(*se);
}

//////////////////////////////////////////////////////////////////////

HistoryTypeDialog::HistoryTypeDialog(const HistoryType& histType,
                                     unsigned int histSize,
                                     QWidget *parent)
  : KDialogBase(Plain, i18n("History Configuration"),
                Help | Default | Ok | Cancel, Ok,
                parent)
{
  QFrame *mainFrame = plainPage();

  QHBoxLayout *hb = new QHBoxLayout(mainFrame);

  m_btnEnable    = new QCheckBox(i18n("&Enable"), mainFrame);

  QObject::connect(m_btnEnable, SIGNAL(toggled(bool)),
                   this,      SLOT(slotHistEnable(bool)));

  m_size = new QSpinBox(0, 10 * 1000 * 1000, 100, mainFrame);
  m_size->setValue(histSize);
  m_size->setSpecialValueText(i18n("Unlimited (number of lines)", "Unlimited"));

  hb->addWidget(m_btnEnable);
  hb->addWidget(new QLabel(i18n("Number of lines : "), mainFrame));
  hb->addWidget(m_size);

  if ( ! histType.isOn()) {
    m_btnEnable->setChecked(false);
    slotHistEnable(false);
  } else {
    m_btnEnable->setChecked(true);
    m_size->setValue(histType.getSize());
    slotHistEnable(true);
  }
  setHelp("configure-history");
}

void HistoryTypeDialog::slotDefault()
{
  m_btnEnable->setChecked(true);
  m_size->setValue(DEFAULT_HISTORY_SIZE);
  slotHistEnable(true);
}

void HistoryTypeDialog::slotHistEnable(bool b)
{
  m_size->setEnabled(b);
  if (b) m_size->setFocus();
}

unsigned int HistoryTypeDialog::nbLines() const
{
  return m_size->value();
}

bool HistoryTypeDialog::isOn() const
{
  return m_btnEnable->isChecked();
}




void Konsole::slotHistoryType()
{
//  KONSOLEDEBUG << "Konsole::slotHistoryType()\n";
  if (!se) return;

  HistoryTypeDialog dlg(se->history(), m_histSize, this);
  if (dlg.exec()) {
    m_clearHistory->setEnabled( dlg.isOn() );
    m_findHistory->setEnabled( dlg.isOn() );
    m_saveHistory->setEnabled( dlg.isOn() );
    if (dlg.isOn()) {
      if (dlg.nbLines() > 0) {
         se->setHistory(HistoryTypeBuffer(dlg.nbLines()));
         m_histSize = dlg.nbLines();
         b_histEnabled = true;

      } else {

         se->setHistory(HistoryTypeFile());
         m_histSize = 0;
         b_histEnabled = true;

      }

    } else {

      se->setHistory(HistoryTypeNone());
      m_histSize = dlg.nbLines();
      b_histEnabled = false;

    }
  }
}

void Konsole::slotClearHistory()
{
  clearSessionHistory(*se);
}

void Konsole::slotFindHistory()
{
  if( !m_finddialog ) {
    m_finddialog = new KEdFind( this, "konsolefind", false);
    connect(m_finddialog,SIGNAL(search()),this,SLOT(slotFind()));
    connect(m_finddialog,SIGNAL(done()),this,SLOT(slotFindDone()));
  }

  QString string;
  string = m_finddialog->getText();
  m_finddialog->setText(string.isEmpty() ? m_find_pattern : string);

  m_find_first = true;
  m_find_found = false;

  m_finddialog->show();
  m_finddialog->result();
}

void Konsole::slotFind()
{
  if (m_find_first) {
    se->getEmulation()->findTextBegin();
    m_find_first = false;
  }

  bool forward = !m_finddialog->get_direction();
  m_find_pattern = m_finddialog->getText();

  if (se->getEmulation()->findTextNext(m_find_pattern,forward,m_finddialog->case_sensitive()))
    m_find_found = true;
  else
    if (m_find_found) {
      if (forward) {
        if ( KMessageBox::questionYesNo( this,
             i18n("End of history reached.\n" "Continue from the beginning?"),
  	     i18n("Find") ) == KMessageBox::Yes ) {
          m_find_first = true;
    	  slotFind();
        }
      }
      else {
        if ( KMessageBox::questionYesNo( this,
             i18n("Beginning of history reached.\n" "Continue from the end?"),
  	     i18n("Find") ) == KMessageBox::Yes ) {
          m_find_first = true;
   	  slotFind();
        }
      }
    }
  else
    KMessageBox::information( this, 
    	i18n( "Search string '%1' not found." ).arg(KStringHandler::csqueeze(m_find_pattern)),
	i18n( "Find" ) );
}

void Konsole::slotFindDone()
{
  if (!m_finddialog)
    return;

  se->getEmulation()->clearSelection();
  m_finddialog->hide();
}

void Konsole::slotSaveHistory()
{
  KURL url = KFileDialog::getSaveURL(QString::null, QString::null, 0L, i18n("Save History..."));
  if( url.isEmpty())
      return;
  if( !url.isLocalFile() ) {
    KMessageBox::sorry(this, i18n("This is not a local file.\n"));
    return;
  }

  int query = KMessageBox::Yes;
  QFileInfo info;
  QString name( url.path() );
  info.setFile( name );
  if( info.exists() )
    query = KMessageBox::warningYesNoCancel( this,
      i18n( "A file with this name already exists.\nDo you want to overwrite it?" ) );

  if (query==KMessageBox::Yes) {
    QFile file(url.path());
    if(!file.open(IO_WriteOnly)) {
      KMessageBox::sorry(this, i18n("Unable to write to file."));
      return;
    }

    QTextStream textStream(&file);
    sessions.current()->getEmulation()->streamHistory( &textStream );

    file.close();
    if(file.status()) {
      KMessageBox::sorry(this, i18n("Could not save history."));
      return;
    }
  }
}

//////////////////////////////////////////////////////////////////////

SizeDialog::SizeDialog(const unsigned int columns,
                       const unsigned int lines,
                       QWidget *parent)
  : KDialogBase(Plain, i18n("Size Configuration"),
                Help | Default | Ok | Cancel, Ok,
                parent)
{
  QFrame *mainFrame = plainPage();

  QHBoxLayout *hb = new QHBoxLayout(mainFrame);

  m_columns = new QSpinBox(20,1000,1,mainFrame);
  m_columns->setValue(columns);

  m_lines = new QSpinBox(4,1000,1,mainFrame);
  m_lines->setValue(lines);

  hb->addWidget(new QLabel(i18n("Number of columns : "), mainFrame));
  hb->addWidget(m_columns);
  hb->addSpacing(10);
  hb->addWidget(new QLabel(i18n("Number of lines : "), mainFrame));
  hb->addWidget(m_lines);

  setHelp("configure-size");
}

void SizeDialog::slotDefault()
{
  m_columns->setValue(80);
  m_lines->setValue(24);
}

unsigned int SizeDialog::columns() const
{
  return m_columns->value();
}

unsigned int SizeDialog::lines() const
{
  return m_lines->value();
}

//////////////////////////////////////////////////////////////////////

void Konsole::slotBackgroundChanged(int desk)
{
  ColorSchema* s = colors->find(curr_schema);
  if (s==0) return;

  // Only update rootxpm if window is visible on current desktop
  NETWinInfo info( qt_xdisplay(), winId(), qt_xrootwin(), NET::WMDesktop );

  if (s->useTransparency() && info.desktop()==desk && (0 != rootxpm)) {
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

void Konsole::currentDesktopChanged(int desk) {
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

   //Check to see if we are transparent too
   ColorSchema* s = colors->find(curr_schema);
   if (s==0) 
      return;

   //This window is transparent, update the root pixmap
   if( bNeedUpdate && s->useTransparency() ) {
      wallpaperSource = desk;
      rootxpm->repaint(true);
   }
}

#include "konsole.moc"

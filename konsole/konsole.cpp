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

#include <qspinbox.h>
#include <qcheckbox.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qhbox.h>
#include <qtoolbutton.h>
#include <qtooltip.h>

#include <stdio.h>
#include <stdlib.h>

#include <kfiledialog.h>
#include <kurlrequesterdlg.h>

#include <kfontdialog.h>
#include <kkeydialog.h>
#include <kstandarddirs.h>
#include <qpainter.h>
#include <kmenubar.h>
#include <kmessagebox.h>
#include <krootpixmap.h>
#include <krun.h>
#include <kstdaction.h>
#include <kinputdialog.h>
#include <kdebug.h>
#include <kipc.h>
#include <dcopclient.h>
#include <kglobalsettings.h>
#include <knotifydialog.h>
#include <kprinter.h>

#include <kaction.h>
#include <qlabel.h>
#include <kpopupmenu.h>
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
#include <kprocctrl.h>
#include <ktabwidget.h>
#include <kregexpeditorinterface.h>
#include <kparts/componentfactory.h>

#include "konsole.h"
#include <netwm.h>
#include "printsettings.h"

#define KONSOLEDEBUG    kdDebug(1211)

#define POPUP_NEW_SESSION_ID 121
#define POPUP_SETTINGS_ID 212

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


static const char * const fonts[] = {
 "13",
 "7",   // tiny font, never used
 "10",  // small font
 "13",  // medium
 "15",  // large
 "20", // huge
 "-misc-console-medium-r-normal--16-160-72-72-c-80-iso10646-1", // "Linux"
 "-misc-fixed-medium-r-normal--15-140-75-75-c-90-iso10646-1",    // "Unicode"
 };
#define TOPFONT (sizeof(fonts)/sizeof(char*))
#define DEFAULTFONT TOPFONT

#define DEFAULT_HISTORY_SIZE 1000

Konsole::Konsole(const char* name, const QString& _program, QStrList & _args, int histon,
                 bool menubaron, bool tabbaron, bool frameon, bool scrollbaron, const QString &_icon,
                 const QString &_title, QCString type, const QString &_term, bool b_inRestore,
                 const int wanted_tabbar, const QString &_cwd)
:DCOPObject( "konsole" )
,KMainWindow(0, name)
,m_defaultSession(0)
,m_defaultSessionFilename("")
,tabwidget(0)
,te(0)
,se(0)
,se_previous(0)
,m_initialSession(0)
,colors(0)
,kWinModule(0)
,menubar(0)
,statusbar(0)
,m_session(0)
,m_edit(0)
,m_view(0)
,m_bookmarks(0)
,m_bookmarksSession(0)
,m_options(0)
,m_schema(0)
,m_keytab(0)
,m_tabbarSessionsCommands(0)
,m_signals(0)
,m_help(0)
,m_rightButton(0)
,m_zmodemUpload(0)
,monitorActivity(0)
,monitorSilence(0)
,masterMode(0)
,showMenubar(0)
,m_fullscreen(0)
,selectSize(0)
,selectFont(0)
,selectScrollbar(0)
,selectTabbar(0)
,selectBell(0)
,m_clearHistory(0)
,m_findHistory(0)
,m_saveHistory(0)
,m_detachSession(0)
,m_moveSessionLeft(0)
,m_moveSessionRight(0)
,bookmarkHandler(0)
,bookmarkHandlerSession(0)
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
,monitorSilenceSeconds(10)
,s_kconfigSchema("")
,b_fullscreen(false)
,m_menuCreated(false)
,skip_exit_query(false) // used to skip the query when closed by the session management
,b_warnQuit(false)
,b_allowResize(true) // Whether application may resize
,b_fixedSize(false) // Whether user may resize
,b_addToUtmp(true)
,b_xonXoff(false)
,b_bidiEnabled(false)
,b_fullScripting(false)
,m_histSize(DEFAULT_HISTORY_SIZE)
{
  isRestored = b_inRestore;
  connect( kapp,SIGNAL(backgroundChanged(int)),this, SLOT(slotBackgroundChanged(int)));
  connect( &m_closeTimeout, SIGNAL(timeout()), this, SLOT(slotCouldNotClose()));

  no2command.setAutoDelete(true);
  no2tempFile.setAutoDelete(true);
  no2filename.setAutoDelete(true);
  menubar = menuBar();

  colors = new ColorSchemaList();
  colors->checkSchemas();

  KeyTrans::loadAll();
  //KONSOLEDEBUG<<"Konsole ctor() after KeyTrans::loadAll() "<<time.elapsed()<<" msecs elapsed"<<endl;

  // create applications /////////////////////////////////////////////////////
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

  makeBasicGUI();
  //KONSOLEDEBUG<<"Konsole ctor() after makeBasicGUI "<<time.elapsed()<<" msecs elapsed"<<endl;

  if (isRestored)
    n_tabbar = wanted_tabbar;

  if (!tabbaron)
    n_tabbar = TabNone;

  if (n_tabbar!=TabNone) {
    makeTabWidget();
    setCentralWidget(tabwidget);
    if (n_tabbar==TabTop)
      tabwidget->setTabPosition(QTabWidget::Top);
    else
      tabwidget->setTabPosition(QTabWidget::Bottom);
  }
  else {
    // create terminal emulation framework ////////////////////////////////////
    te = new TEWidget(this);
    connect( te, SIGNAL(configureRequest(TEWidget*, int, int, int)),
             this, SLOT(configureRequest(TEWidget*,int,int,int)) );

    //KONSOLEDEBUG<<"Konsole ctor() after new TEWidget() "<<time.elapsed()<<" msecs elapsed"<<endl;
    te->setMinimumSize(150,70);    // allow resizing, cause resize in TEWidget
    // we need focus so that the auto-hide cursor feature works (Carsten)
    // but a part shouldn't force that it receives the focus, so we do it here (David)
    te->setFocus();

    readProperties(config, schema, false);
    n_tabbar = TabNone;
    setCentralWidget(te);
  }

  b_histEnabled=histon;

  if (!menubaron)
    menubar->hide();
  if (!frameon) {
    b_framevis=false;
    te->setFrameStyle( QFrame::NoFrame );
  }
  if (!scrollbaron) {
    n_scroll = TEWidget::SCRNONE;
    te->setScrollbarLocation(TEWidget::SCRNONE);
  }

  // activate and run first session //////////////////////////////////////////
  newSession(co, _program, _args, _term, _icon, _title, _cwd);

  //KONSOLEDEBUG<<"Konsole ctor() ends "<<time.elapsed()<<" msecs elapsed"<<endl;

  kapp->dcopClient()->setDefaultObject( "konsole" );
}


Konsole::~Konsole()
{
    while (detached.count()) {
        KonsoleChild* child=detached.first();
        delete child;
        detached.remove(child);
    }

    sessions.first();
    while(sessions.current())
    {
      sessions.current()->closeSession();
      sessions.next();
    }

    // Wait a bit for all childs to clean themselves up.
#if KDE_VERSION >=306
    while(sessions.count() && KProcessController::theKProcessController->waitForProcessExit(1))
        ;
//#else
//    while(sessions.count())
//        ;
#endif

    sessions.setAutoDelete(true);

    resetScreenSessions();
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

void Konsole::setAutoClose(bool on)
{
    if (sessions.first())
       sessions.first()->setAutoClose(on);
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

void Konsole::updateRMBMenu()
{
   if (!m_rightButton) return;
   int index = 0;
   if (!showMenubar->isChecked())
   {
      // Only show when menubar is hidden
      if (!showMenubar->isPlugged( m_rightButton ))
      {
         showMenubar->plug ( m_rightButton, index );
         m_rightButton->insertSeparator( index+1 );
      }
      index = 2;
      m_rightButton->setItemVisible(POPUP_NEW_SESSION_ID,true);
      m_rightButton->setItemVisible(POPUP_SETTINGS_ID,true);
   }
   else
   {
      if (showMenubar->isPlugged( m_rightButton ))
      {
         showMenubar->unplug ( m_rightButton );
         m_rightButton->removeItemAt(index);
      }
      index = 0;
      m_rightButton->setItemVisible(POPUP_NEW_SESSION_ID,false);
      m_rightButton->setItemVisible(POPUP_SETTINGS_ID,false);
   }

   if (!m_fullscreen)
      return;
   if (b_fullscreen)
   {
      if (!m_fullscreen->isPlugged( m_rightButton ))
      {
         m_fullscreen->plug ( m_rightButton, index );
         m_rightButton->insertSeparator( index+1 );
      }
   }
   else
   {
      if (m_fullscreen->isPlugged( m_rightButton ))
      {
         m_fullscreen->unplug ( m_rightButton );
         m_rightButton->removeItemAt(index);
      }
   }
}

// Be carefull !!
// This function consumes a lot of time, that's why it is called delayed on demand.
// Be careful not to introduce function calls which lead to the execution of this
// function when starting konsole
// Be careful not to access stuff which is created in this function before this
// function was called ! you can check this using m_menuCreated, aleXXX
void Konsole::makeGUI()
{
   if (m_menuCreated) return;
   //not longer needed
   if (m_tabbarSessionsCommands)
      disconnect(m_tabbarSessionsCommands,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   disconnect(m_session,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   if (m_options)
      disconnect(m_options,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   if (m_help)
      disconnect(m_help,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   if (m_rightButton)
      disconnect(m_rightButton,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   disconnect(m_edit,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   disconnect(m_view,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   if (m_bookmarks)
      disconnect(m_bookmarks,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   if (m_bookmarksSession)
      disconnect(m_bookmarksSession,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
   //KONSOLEDEBUG<<"Konsole::makeGUI()"<<endl;
   if (m_tabbarSessionsCommands)
      connect(m_tabbarSessionsCommands,SIGNAL(aboutToShow()),this,SLOT(loadScreenSessions()));
   connect(m_session,SIGNAL(aboutToShow()),this,SLOT(loadScreenSessions()));
   m_menuCreated=true;

   // Remove the empty separator Qt inserts if the menu is empty on popup,
   // not sure if this will be "fixed" in Qt, for now use this hack (malte)
   if(!(isRestored)) {
     if (sender() && sender()->inherits("QPopupMenu") &&
       static_cast<const QPopupMenu *>(sender())->count() == 1)
       const_cast<QPopupMenu *>(static_cast<const QPopupMenu *>(sender()))->removeItemAt(0);
       }

   KActionCollection* actions = actionCollection();

   // Send Signal Menu -------------------------------------------------------------
   if (kapp->authorizeKAction("send_signal"))
   {
      m_signals = new KPopupMenu(this);
      m_signals->insertItem( i18n( "&Suspend Task" )   + " (STOP)", SIGSTOP);
      m_signals->insertItem( i18n( "&Continue Task" )  + " (CONT)", SIGCONT);
      m_signals->insertItem( i18n( "&Hangup" )         + " (HUP)", SIGHUP);
      m_signals->insertItem( i18n( "&Interrupt Task" ) + " (INT)", SIGINT);
      m_signals->insertItem( i18n( "&Terminate Task" ) + " (TERM)", SIGTERM);
      m_signals->insertItem( i18n( "&Kill Task" )      + " (KILL)", SIGKILL);
      m_signals->insertItem( i18n( "User Signal &1")   + " (USR1)", SIGUSR1);
      m_signals->insertItem( i18n( "User Signal &2")   + " (USR2)", SIGUSR2);
      connect(m_signals, SIGNAL(activated(int)), SLOT(sendSignal(int)));
   }

   // Edit Menu ----------------------------------------------------------------
   m_copyClipboard->plug(m_edit);
   m_pasteClipboard->plug(m_edit);

   m_edit->setCheckable(true);
   if (m_signals)
      m_edit->insertItem( i18n("&Send Signal"), m_signals );

   m_edit->insertSeparator();
   m_zmodemUpload->plug( m_edit );

   m_edit->insertSeparator();
   m_clearTerminal->plug(m_edit);

   m_resetClearTerminal->plug(m_edit);

   m_edit->insertSeparator();
   m_findHistory->plug(m_edit);
   m_findNext->plug(m_edit);
   m_findPrevious->plug(m_edit);
   m_saveHistory->plug(m_edit);
   m_edit->insertSeparator();
   m_clearHistory->plug(m_edit);
   m_clearAllSessionHistories->plug(m_edit);

   // View Menu
   m_detachSession->plug(m_view);
   m_renameSession->plug(m_view);

   m_view->insertSeparator();
   monitorActivity->plug ( m_view );
   monitorSilence->plug ( m_view );

   masterMode->plug ( m_view );

   m_view->insertSeparator();
   m_moveSessionLeft = new KAction(i18n("&Move Session Left"), QApplication::reverseLayout() ? "forward" : "back",
                                        QApplication::reverseLayout() ? Qt::CTRL+Qt::SHIFT+Qt::Key_Right : Qt::CTRL+Qt::SHIFT+Qt::Key_Left, this,
                                        SLOT(moveSessionLeft()), m_shortcuts, "move_session_left");
   m_moveSessionLeft->setEnabled( false );
   m_moveSessionLeft->plug(m_view);

   m_moveSessionRight = new KAction(i18n("M&ove Session Right"), QApplication::reverseLayout() ? "back" : "forward",
                                        QApplication::reverseLayout() ? Qt::CTRL+Qt::SHIFT+Qt::Key_Left : Qt::CTRL+Qt::SHIFT+Qt::Key_Right, this,
                                        SLOT(moveSessionRight()), m_shortcuts, "move_session_right");
   m_moveSessionRight->setEnabled( false );
   m_moveSessionRight->plug(m_view);

   m_view->insertSeparator();
   KRadioAction *ra = session2action.find(se);
   if (ra!=0) ra->plug(m_view);

   //bookmarks menu
   if (bookmarkHandler)
      connect( bookmarkHandler, SIGNAL( openURL( const QString&, const QString& )),
            SLOT( enterURL( const QString&, const QString& )));
   if (bookmarkHandlerSession)
      connect( bookmarkHandlerSession, SIGNAL( openURL( const QString&, const QString& )),
            SLOT( newSession( const QString&, const QString& )));
   if (m_bookmarks)
      connect(m_bookmarks, SIGNAL(aboutToShow()), SLOT(bookmarks_menu_check()));
   if (m_bookmarksSession)
      connect(m_bookmarksSession, SIGNAL(aboutToShow()), SLOT(bookmarks_menu_check()));

   // Schema Options Menu -----------------------------------------------------
   m_schema = new KPopupMenu(this);
   m_schema->setCheckable(true);
   connect(m_schema, SIGNAL(activated(int)), SLOT(schema_menu_activated(int)));
   connect(m_schema, SIGNAL(aboutToShow()), SLOT(schema_menu_check()));

   // Keyboard Options Menu ---------------------------------------------------
   m_keytab = new KPopupMenu(this);
   m_keytab->setCheckable(true);
   connect(m_keytab, SIGNAL(activated(int)), SLOT(keytab_menu_activated(int)));

   //options menu
   if (m_options)
   {
      // Menubar on/off
      showMenubar->plug ( m_options );

      // Tabbar
      selectTabbar = new KSelectAction(i18n("&Tabbar"), 0, this,
                                       SLOT(slotSelectTabbar()), actions, "tabbar" );
      QStringList tabbaritems;
      tabbaritems << i18n("&Hide") << i18n("&Top") << i18n("&Bottom");
      selectTabbar->setItems(tabbaritems);
      selectTabbar->plug(m_options);

      // Scrollbar
      selectScrollbar = new KSelectAction(i18n("Sc&rollbar"), 0, this,
                                       SLOT(slotSelectScrollbar()), actions, "scrollbar" );
      QStringList scrollitems;
      scrollitems << i18n("&Hide") << i18n("&Left") << i18n("&Right");
      selectScrollbar->setItems(scrollitems);
      selectScrollbar->plug(m_options);

      // Fullscreen
      m_options->insertSeparator();
      if (m_fullscreen)
      {
        m_fullscreen->plug(m_options);
        m_options->insertSeparator();
      }

      // Select Bell
      selectBell = new KSelectAction(i18n("&Bell"), SmallIconSet( "bell"), 0 , this,
                                  SLOT(slotSelectBell()), actions, "bell");
      QStringList bellitems;
      bellitems << i18n("System &Bell")
                << i18n("System &Notification")
                << i18n("&Visible Bell");
      selectBell->setItems(bellitems);
      selectBell->plug(m_options);

      // Select font
      selectFont = new KonsoleFontSelectAction( i18n( "&Font" ),
          SmallIconSet( "text" ), 0, this, SLOT(slotSelectFont()), actions, "font");
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


      if (kapp->authorizeKAction("keyboard"))
         m_options->insertItem( SmallIconSet( "key_bindings" ), i18n( "&Keyboard" ), m_keytab );

      // Schema
      if (kapp->authorizeKAction("schema"))
         m_options->insertItem( SmallIconSet( "colorize" ), i18n( "Sch&ema" ), m_schema);

      // Select size
      if (!b_fixedSize)
      {
         selectSize = new KonsoleFontSelectAction(i18n("S&ize"), 0, this,
                                  SLOT(slotSelectSize()), actions, "size");
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
      }

      KAction *historyType = new KAction(i18n("Hist&ory..."), "history", 0, this,
                                      SLOT(slotHistoryType()), actions, "history");
      historyType->plug(m_options);

      m_options->insertSeparator();

      KAction *save_settings = new KAction(i18n("&Save as Default"), "filesave", 0, this,
                                        SLOT(slotSaveSettings()), actions, "save_default");
      save_settings->plug(m_options);

      m_options->insertSeparator();

      m_saveProfile->plug(m_options);

      m_options->insertSeparator();

      KStdAction::configureNotifications(this, SLOT(slotConfigureNotifications()), actionCollection())->plug(m_options);
      KStdAction::keyBindings(this, SLOT(slotConfigureKeys()), actionCollection())->plug(m_options);
      KAction *configure = KStdAction::preferences(this, SLOT(slotConfigure()), actions);
      configure->plug(m_options);

      if (KGlobalSettings::insertTearOffHandle())
         m_options->insertTearOffHandle();
   }

   //help menu
   if (m_help)
   {
      m_help->insertSeparator(1);
      m_help->insertItem(SmallIcon( "idea" ), i18n("&Tip of the Day"),
            this, SLOT(showTip()), 0, -1, 2);
   }

   //the different session menus
   buildSessionMenus();

   connect(m_session, SIGNAL(activated(int)), SLOT(newSession(int)));

   // Right mouse button menu
   if (m_rightButton)
   {
      updateRMBMenu(); // show menubar / exit fullscreen

      KAction* selectionEnd = new KAction(i18n("Set Selection End"), 0, this,
                               SLOT(slotSetSelectionEnd()), actions, "selection_end");
      selectionEnd->plug(m_rightButton);

      m_copyClipboard->plug(m_rightButton);
      m_pasteClipboard->plug(m_rightButton);
      if (m_signals)
         m_rightButton->insertItem(i18n("&Send Signal"), m_signals);

      m_rightButton->insertSeparator();
      if (m_tabbarSessionsCommands)
         m_rightButton->insertItem( i18n("New Sess&ion"), m_tabbarSessionsCommands, POPUP_NEW_SESSION_ID );
      m_detachSession->plug(m_rightButton);
      m_renameSession->plug(m_rightButton);

      if (m_bookmarks)
      {
         m_rightButton->insertSeparator();
         m_rightButton->insertItem(i18n("&Bookmarks"), m_bookmarks);
      }

      if (m_options)
      {
         m_rightButton->insertSeparator();
         m_rightButton->insertItem(i18n("S&ettings"), m_options, POPUP_SETTINGS_ID);
      }
      m_rightButton->insertSeparator();
      m_closeSession->plug(m_rightButton );
      if (KGlobalSettings::insertTearOffHandle())
         m_rightButton->insertTearOffHandle();
   }

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
   while (se == NULL) {}
   se->setSchemaNo(curr_schema);

   // insert keymaps into menu
   //FIXME: sort
   for (int i = 0; i < KeyTrans::count(); i++)
   {
      KeyTrans* ktr = KeyTrans::find(i);
      assert( ktr );
      QString title=ktr->hdr();
      m_keytab->insertItem(title.replace('&',"&&"),ktr->numb());
   }
   applySettingsToGUI();
   isRestored = false;

   new KAction(i18n("Goto Previous Session"), QApplication::reverseLayout() ? Qt::SHIFT+Qt::Key_Right : Qt::SHIFT+Qt::Key_Left,
               this, SLOT(prevSession()), m_shortcuts, "previous_session");
   new KAction(i18n("Goto Next Session"), QApplication::reverseLayout() ? Qt::SHIFT+Qt::Key_Left : Qt::SHIFT+Qt::Key_Right,
               this, SLOT(nextSession()), m_shortcuts, "next_session");

   new KAction(i18n("Switch to Session 1"), 0, this, SLOT(switchToSession1()), m_shortcuts, "switch_to_session_1");
   new KAction(i18n("Switch to Session 2"), 0, this, SLOT(switchToSession2()), m_shortcuts, "switch_to_session_2");
   new KAction(i18n("Switch to Session 3"), 0, this, SLOT(switchToSession3()), m_shortcuts, "switch_to_session_3");
   new KAction(i18n("Switch to Session 4"), 0, this, SLOT(switchToSession4()), m_shortcuts, "switch_to_session_4");
   new KAction(i18n("Switch to Session 5"), 0, this, SLOT(switchToSession5()), m_shortcuts, "switch_to_session_5");
   new KAction(i18n("Switch to Session 6"), 0, this, SLOT(switchToSession6()), m_shortcuts, "switch_to_session_6");
   new KAction(i18n("Switch to Session 7"), 0, this, SLOT(switchToSession7()), m_shortcuts, "switch_to_session_7");
   new KAction(i18n("Switch to Session 8"), 0, this, SLOT(switchToSession8()), m_shortcuts, "switch_to_session_8");
   new KAction(i18n("Switch to Session 9"), 0, this, SLOT(switchToSession9()), m_shortcuts, "switch_to_session_9");
   new KAction(i18n("Switch to Session 10"), 0, this, SLOT(switchToSession10()), m_shortcuts, "switch_to_session_10");
   new KAction(i18n("Switch to Session 11"), 0, this, SLOT(switchToSession11()), m_shortcuts, "switch_to_session_11");
   new KAction(i18n("Switch to Session 12"), 0, this, SLOT(switchToSession12()), m_shortcuts, "switch_to_session_12");

   new KAction(i18n("Bigger Font"), 0, this, SLOT(biggerFont()), m_shortcuts, "bigger_font");
   new KAction(i18n("Smaller Font"), 0, this, SLOT(smallerFont()), m_shortcuts, "smaller_font");

   new KAction(i18n("Toggle Bidi"), Qt::CTRL+Qt::ALT+Qt::Key_B, this, SLOT(toggleBidi()), m_shortcuts, "toggle_bidi");

   m_shortcuts->readShortcutSettings();
}

void Konsole::makeTabWidget()
{
  tabwidget = new KTabWidget(this);
  tabwidget->setTabReorderingEnabled(true);
  connect(tabwidget, SIGNAL(movedTab(int,int)), SLOT(slotMovedTab(int,int)));
  connect(tabwidget, SIGNAL(mouseDoubleClick(QWidget*)), SLOT(slotRenameSession()));
  connect(tabwidget, SIGNAL(currentChanged(QWidget*)), SLOT(activateSession(QWidget*)));

  if (kapp->authorize("shell_access")) {
    QToolButton* newsession = new QToolButton( tabwidget );
//    newsession->setTextLabel("New");
//    newsession->setTextPosition(QToolButton::Right);
//    newsession->setUsesTextLabel(true);
    QToolTip::add(newsession,i18n("Click for new standard session\nClick and hold for session menu"));
    newsession->setIconSet( SmallIcon( "tab_new" ) );
    newsession->adjustSize();
    newsession->setPopup( m_tabbarSessionsCommands );
    connect(newsession, SIGNAL(clicked()), SLOT(newSession()));
    tabwidget->setCornerWidget( newsession, BottomLeft );
  }
}

void Konsole::makeBasicGUI()
{
  //KONSOLEDEBUG<<"Konsole::makeBasicGUI()"<<endl;
  if (kapp->authorize("shell_access")) {
    m_tabbarSessionsCommands = new KPopupMenu( this );
    connect(m_tabbarSessionsCommands, SIGNAL(activated(int)), SLOT(newSessionTabbar(int)));
  }

  m_session = new KPopupMenu(this);
  m_edit = new KPopupMenu(this);
  m_view = new KPopupMenu(this);
  if (kapp->authorizeKAction("bookmarks"))
  {
    bookmarkHandler = new KonsoleBookmarkHandler( this, true );
    m_bookmarks = bookmarkHandler->menu();
    // call manually to disable accelerator c-b for add-bookmark initially.
    bookmarks_menu_check();
  }

  if (kapp->authorizeKAction("settings"))
     m_options = new KPopupMenu(this);

  if (kapp->authorizeKAction("help"))
     m_help = helpMenu(0, false);

  if (kapp->authorizeKAction("konsole_rmb"))
     m_rightButton = new KPopupMenu(this);

  if (kapp->authorizeKAction("bookmarks"))
  {
    // Bookmarks that open new sessions.
    bookmarkHandlerSession = new KonsoleBookmarkHandler( this, false );
    m_bookmarksSession = bookmarkHandlerSession->menu();
  }

  // For those who would like to add shortcuts here, be aware that
  // ALT-key combinations are heavily used by many programs. Thus,
  // activating shortcuts here means deactivating them in the other
  // programs.

  if (m_tabbarSessionsCommands)
     connect(m_tabbarSessionsCommands,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  connect(m_session,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  if (m_options)
     connect(m_options,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  if (m_help)
     connect(m_help,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  if (m_rightButton)
     connect(m_rightButton,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  connect(m_edit,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  connect(m_view,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  if (m_bookmarks)
     connect(m_bookmarks,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));
  if (m_bookmarksSession)
     connect(m_bookmarksSession,SIGNAL(aboutToShow()),this,SLOT(makeGUI()));

  menubar->insertItem(i18n("Session") , m_session);
  menubar->insertItem(i18n("Edit"), m_edit);
  menubar->insertItem(i18n("View"), m_view);
  if (m_bookmarks)
     menubar->insertItem(i18n("Bookmarks"), m_bookmarks);
  if (m_options)
     menubar->insertItem(i18n("Settings"), m_options);
  if (m_help)
     menubar->insertItem(i18n("Help"), m_help);

  m_shortcuts = new KActionCollection(this);

  m_copyClipboard = new KAction(i18n("&Copy"), "editcopy", 0, this,
                                 SLOT(slotCopyClipboard()), m_shortcuts, "edit_copy");
  m_pasteClipboard = new KAction(i18n("&Paste"), "editpaste", Qt::SHIFT+Qt::Key_Insert, this,
                                 SLOT(slotPasteClipboard()), m_shortcuts, "edit_paste");
  m_pasteSelection = new KAction(i18n("Paste Selection"), Qt::CTRL+Qt::SHIFT+Qt::Key_Insert, this,
                                 SLOT(slotPasteSelection()), m_shortcuts, "pasteselection");

  m_clearTerminal = new KAction(i18n("C&lear Terminal"), 0, this,
                                SLOT(slotClearTerminal()), m_shortcuts, "clear_terminal");
  m_resetClearTerminal = new KAction(i18n("&Reset && Clear Terminal"), 0, this,
                                     SLOT(slotResetClearTerminal()), m_shortcuts, "reset_clear_terminal");
  m_findHistory = new KAction(i18n("&Find in History..."), "find", 0, this,
                              SLOT(slotFindHistory()), m_shortcuts, "find_history");
  m_findHistory->setEnabled(b_histEnabled);

  m_findNext = new KAction(i18n("Find &Next"), "next", 0, this,
                           SLOT(slotFindNext()), m_shortcuts, "find_next");
  m_findNext->setEnabled(b_histEnabled);

  m_findPrevious = new KAction(i18n("Find Pre&vious"), "previous", 0, this,
                               SLOT(slotFindPrevious()), m_shortcuts, "find_previous");
  m_findPrevious->setEnabled( b_histEnabled );

  m_saveHistory = new KAction(i18n("S&ave History As..."), "filesaveas", 0, this,
                              SLOT(slotSaveHistory()), m_shortcuts, "save_history");
  m_saveHistory->setEnabled(b_histEnabled );

  m_clearHistory = new KAction(i18n("Clear &History"), "history_clear", 0, this,
                               SLOT(slotClearHistory()), m_shortcuts, "clear_history");
  m_clearHistory->setEnabled(b_histEnabled);

  m_clearAllSessionHistories = new KAction(i18n("Clear All H&istories"), "history_clear", 0,
    this, SLOT(slotClearAllSessionHistories()), m_shortcuts, "clear_all_histories");

  m_detachSession = new KAction(i18n("&Detach Session"), 0, this,
                                SLOT(detachSession()), m_shortcuts, "detach_session");
  m_detachSession->setEnabled(false);

  m_renameSession = new KAction(i18n("&Rename Session..."), Qt::CTRL+Qt::ALT+Qt::Key_S, this,
                                SLOT(slotRenameSession()), m_shortcuts, "rename_session");
  m_zmodemUpload = new KAction(i18n("&ZModem Upload..."), Qt::CTRL+Qt::ALT+Qt::Key_U, this,
                                SLOT(slotZModemUpload()), m_shortcuts, "zmodem_upload");
  monitorActivity = new KToggleAction ( i18n( "Monitor for &Activity" ), "idea", 0, this,
                                        SLOT( slotToggleMonitor() ), m_shortcuts, "monitor_activity" );
  monitorSilence = new KToggleAction ( i18n( "Monitor for &Silence" ), "ktip", 0, this,
                                       SLOT( slotToggleMonitor() ), m_shortcuts, "monitor_silence" );
  masterMode = new KToggleAction ( i18n( "Send &Input to All Sessions" ), "remote", 0, this,
                                   SLOT( slotToggleMasterMode() ), m_shortcuts, "send_input_to_all_sessions" );

  showMenubar = new KToggleAction ( i18n( "Show &Menubar" ), "showmenu", 0, this,
                                    SLOT( slotToggleMenubar() ), m_shortcuts, "show_menubar" );
  m_fullscreen = KStdAction::fullScreen(this, SLOT(slotToggleFullscreen()), m_shortcuts );
  m_fullscreen->setChecked(b_fullscreen);

  m_saveProfile = new KAction( i18n( "Save Sessions &Profile..." ), 0, this,
                         SLOT( slotSaveSessionsProfile() ), m_shortcuts, "save_sessions_profile" );

  //help menu
  if (m_help)
     m_help->setAccel(QKeySequence(),m_help->idAt(0));

  m_closeSession = new KAction(i18n("C&lose Session"), "fileclose", 0, this,
                               SLOT(closeCurrentSession()), m_shortcuts, "close_session");
  m_print = new KAction(i18n("&Print Screen..."), "fileprint", 0, this, SLOT( slotPrint() ), m_shortcuts, "file_print");
  m_quit = new KAction(i18n("&Quit"), "exit", 0, this, SLOT( close() ), m_shortcuts, "file_quit");

  KShortcut shortcut(Qt::CTRL+Qt::ALT+Qt::Key_N);
  shortcut.append(KShortcut(Qt::CTRL+Qt::SHIFT+Qt::Key_N));
  new KAction(i18n("New Session"), shortcut, this, SLOT(newSession()), m_shortcuts, "new_session");
  new KAction(i18n("Activate Menu"), Qt::CTRL+Qt::ALT+Qt::Key_M, this, SLOT(activateMenu()), m_shortcuts, "activate_menu");
  new KAction(i18n("List Sessions"), 0, this, SLOT(listSessions()), m_shortcuts, "list_sessions");

  m_shortcuts->readShortcutSettings();
}

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
   if(skip_exit_query)
     // saving session - do not even think about doing any kind of cleanup here
       return true;

   while (detached.count()) {
     KonsoleChild* child=detached.first();
     delete child;
     detached.remove(child);
   }

   if (sessions.count() == 0)
       return true;

   if ( b_warnQuit)
   {
        if( (sessions.count()>1) &&
            ( KMessageBox::warningYesNo( this,
                                         i18n( "You have open sessions (besides the current one). "
                                               "These will be killed if you continue.\n"
                                               "Are you sure you want to quit?" ),
					 i18n("Really Quit?"),
					 i18n("&Quit"), i18n("&Cancel") )

              == KMessageBox::No )
            ) {
            return false;
        }
    }

    // WABA: Don't close if there are any sessions left.
    // Tell them to go away.
    sessions.first();
    while(sessions.current())
    {
      sessions.current()->closeSession();
      sessions.next();
    }

    m_closeTimeout.start(1500, true);
    return false;
}

void Konsole::slotCouldNotClose()
{
    int result = KMessageBox::warningContinueCancel(this,
             i18n("The application running in Konsole does not respond to the close request. "
                  "Do you want Konsole to close anyway?"),
             i18n("Application Does Not Respond"),
             i18n("Close"));
    if (result == KMessageBox::Continue)
    {
        while(sessions.first())
        {
            doneSession(sessions.current());
        }
    }
}

/**
    sets application window to a size based on columns X lines of the te
    guest widget. Call with (0,0) for setting default size.
*/

void Konsole::setColLin(int columns, int lines)
{
  if ((columns==0) || (lines==0))
  {
    if (b_fixedSize || defaultSize.isEmpty())
    {
      // not in config file : set default value
      columns = 80;
      lines = 24;
    }
  }

  if ((columns==0) || (lines==0))
  {
    resize(defaultSize);
  }
  else
  {
    if (b_fixedSize)
       te->setFixedSize(columns, lines);
    else
       te->setSize(columns, lines);
    adjustSize();
    if (b_fixedSize)
      setFixedSize(sizeHint());
    notifySize(lines,columns); // set menu items (strange arg order !)
  }
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void Konsole::configureRequest(TEWidget* _te, int state, int x, int y)
{
//printf("Konsole::configureRequest(_,%d,%d)\n",x,y);
   if (!m_menuCreated)
      makeGUI();
  KPopupMenu *menu = (state & ControlButton) ? m_session : m_rightButton;
  if (menu)
     menu->popup(_te->mapToGlobal(QPoint(x,y)));
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* Configuration                                                             */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void Konsole::slotSaveSessionsProfile()
{
  bool ok;

  QString prof = KInputDialog::getText( i18n( "Save Sessions Profile" ),
      i18n( "Enter name under which the profile should be saved:" ),
      QString::null, &ok, this );
  if ( ok ) {
    QString path = locateLocal( "data",
        QString::fromLatin1( "konsole/profiles/" ) + prof,
        KGlobal::instance() );

    if ( QFile::exists( path ) )
      QFile::remove( path );

    KSimpleConfig cfg( path );
    savePropertiesInternal(&cfg,1);
    saveMainWindowSettings(&cfg);
  }
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
        config->writeEntry(key, colors->find( sessions.current()->schemaNo() )->relPath());
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

        QString cwd=sessions.current()->getCwd();
        if (cwd.isEmpty())
          cwd=sessions.current()->getInitial_cwd();
        key = QString("Cwd%1").arg(counter);
        config->writePathEntry(key, cwd);

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
  config->writeEntry("tabbar",n_tabbar);
  config->writeEntry("bellmode",n_bell);
  config->writeEntry("keytab",KeyTrans::find(n_defaultKeytab)->id());
  config->writeEntry("ActiveSession", active);
  config->writeEntry("DefaultSession", m_defaultSessionFilename);

  if (se) {
    config->writeEntry("history", se->history().getSize());
    config->writeEntry("historyenabled", b_histEnabled);
  }

  config->writeEntry("class",name());
  if (config != KGlobal::config())
  {
      saveMainWindowSettings(config);
  }
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
     b_warnQuit=config->readBoolEntry( "WarnQuit", true );
     b_allowResize=config->readBoolEntry( "AllowResize", false);
     b_bidiEnabled = config->readBoolEntry("EnableBidi",false);
     s_word_seps= config->readEntry("wordseps",":@-./_~");
     b_framevis = config->readBoolEntry("has frame",true);
     QPtrList<TEWidget> tes = activeTEs();
     for (TEWidget *_te = tes.first(); _te; _te = tes.next()) {
       _te->setWordCharacters(s_word_seps);
       _te->setTerminalSizeHint( config->readBoolEntry("TerminalSizeHint",false) );
       _te->setFrameStyle( b_framevis?(QFrame::WinPanel|QFrame::Sunken):QFrame::NoFrame );
       _te->setBlinkingCursor(config->readBoolEntry("BlinkingCursor",false));
       _te->setCtrlDrag(config->readBoolEntry("CtrlDrag",true));
       _te->setCutToBeginningOfLine(config->readBoolEntry("CutToBeginningOfLine",false));
       _te->setLineSpacing( config->readUnsignedNumEntry( "LineSpacing", 0 ) );
       _te->setBidiEnabled(b_bidiEnabled);
     }

     monitorSilenceSeconds=config->readUnsignedNumEntry("SilenceSeconds", 10);
     for (TESession *ses = sessions.first(); ses; ses = sessions.next())
       ses->setMonitorSilenceSeconds(monitorSilenceSeconds);

     b_xonXoff = config->readBoolEntry("XonXoff",false);
     config->setGroup("UTMP");
     b_addToUtmp = config->readBoolEntry("AddToUtmp",true);
     config->setDesktopGroup();
   }

   if (!globalConfigOnly)
   {
      n_defaultKeytab=KeyTrans::find(config->readEntry("keytab","default"))->numb(); // act. the keytab for this session
      b_fullscreen = config->readBoolEntry("Fullscreen",false);
      n_defaultFont = n_font = QMIN(config->readUnsignedNumEntry("font",3),TOPFONT);
      n_scroll   = QMIN(config->readUnsignedNumEntry("scrollbar",TEWidget::SCRRIGHT),2);
      n_tabbar   = QMIN(config->readUnsignedNumEntry("tabbar",TabBottom),2);
      n_bell = QMIN(config->readUnsignedNumEntry("bellmode",TEWidget::BELLSYSTEM),2);
      // Options that should be applied to all sessions /////////////

      // (1) set menu items and Konsole members
      QFont tmpFont("fixed");
      tmpFont.setFixedPitch(true);
      tmpFont.setStyleHint(QFont::TypeWriter);
      defaultFont = config->readFontEntry("defaultfont", &tmpFont);

      //set the schema
      s_kconfigSchema=config->readEntry("schema");
      ColorSchema* sch = colors->find(schema.isEmpty() ? s_kconfigSchema : schema);
      if (!sch)
      {
         kdWarning() << "Could not find schema named " <<s_kconfigSchema<< endl;
         sch=(ColorSchema*)colors->at(0);  //the default one
      }
      if (sch->hasSchemaFileChanged()) sch->rereadSchemaFile();
      s_schema = sch->relPath();
      curr_schema = sch->numb();
      pmPath = sch->imagePath();

      if (te) {
        if (sch->useTransparency())
        {
           //KONSOLEDEBUG << "Setting up transparency" << endl;
           if (!rootxpms[te])
             rootxpms.insert( te, new KRootPixmap(te) );
           rootxpms[te]->setFadeEffect(sch->tr_x(), QColor(sch->tr_r(), sch->tr_g(), sch->tr_b()));
           rootxpms[te]->start();
           rootxpms[te]->repaint(true);
        }
        else
        {
           //KONSOLEDEBUG << "Stopping transparency" << endl
           if (rootxpms[te]) {
             delete rootxpms[te];
             rootxpms.remove(te);
           }
           pixmap_menu_activated(sch->alignment());
        }

        setFont(QMIN(config->readUnsignedNumEntry("font",3),TOPFONT));
        te->setColorTable(sch->table()); //FIXME: set twice here to work around a bug
        te->setColorTable(sch->table());
        te->setScrollbarLocation(n_scroll);
        te->setBellMode(n_bell);
      }

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
   if (m_options)
   {
      setFont();
      notifySize(te->Lines(),te->Columns());
      selectTabbar->setCurrentItem(n_tabbar);
      showMenubar->setChecked(!menuBar()->isHidden());
      selectScrollbar->setCurrentItem(n_scroll);
      selectBell->setCurrentItem(n_bell);
      updateRMBMenu();
   }
   updateKeytabMenu();
}


/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void Konsole::bookmarks_menu_check()
{
  bool state = false;
  if ( se )
      state = !(se->getCwd().isEmpty());

  KAction *addBookmark = actionCollection()->action( "add_bookmark" );
  if ( !addBookmark )
  {
      return;
  }
  addBookmark->setEnabled( state );
}

void Konsole::pixmap_menu_activated(int item, TEWidget* tewidget)
{
  if (!tewidget)
    tewidget=te;
  if (item <= 1) pmPath = "";
  QPixmap pm(pmPath);
  if (pm.isNull()) {
    pmPath = "";
    item = 1;
    tewidget->setBackgroundColor(tewidget->getDefaultBackColor());
    return;
  }
  // FIXME: respect scrollbar (instead of te->size)
  n_render= item;
  switch (item)
  {
    case 1: // none
    case 2: // tile
            tewidget->setBackgroundPixmap(pm);
    break;
    case 3: // center
            { QPixmap bgPixmap;
              bgPixmap.resize(tewidget->size());
              bgPixmap.fill(tewidget->getDefaultBackColor());
              bitBlt( &bgPixmap, ( tewidget->size().width() - pm.width() ) / 2,
                                ( tewidget->size().height() - pm.height() ) / 2,
                      &pm, 0, 0,
                      pm.width(), pm.height() );

              tewidget->setBackgroundPixmap(bgPixmap);
            }
    break;
    case 4: // full
            {
              float sx = (float)tewidget->size().width() / pm.width();
              float sy = (float)tewidget->size().height() / pm.height();
              QWMatrix matrix;
              matrix.scale( sx, sy );
              tewidget->setBackgroundPixmap(pm.xForm( matrix ));
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

   QPtrList<TEWidget> tes = activeTEs();
   for (TEWidget *_te = tes.first(); _te; _te = tes.next())
     _te->setScrollbarLocation(n_scroll);
   activateSession(); // maybe helps in bg
}

void Konsole::slotSelectFont() {
  assert(se);
  int item = selectFont->currentItem();
  if( item > 9 ) // compensate for the two separators
      --item;
  if( item > 6 )
      --item;
  // KONSOLEDEBUG << "slotSelectFont " << item << endl;
  if (item == DEFAULTFONT)
  {
    if ( KFontDialog::getFont(defaultFont, true) == QDialog::Rejected )
    {
      setFont();
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
    QString title=s->title();
    m_schema->insertItem(title.replace('&',"&&"),s->numb(),0);
  }

  if (te && se)
  {
//        KONSOLEDEBUG << "Current session has schema "
//                << se->schemaNo()
//                << endl;
        m_schema->setItemChecked(se->schemaNo(),true);
  }

}

void Konsole::updateKeytabMenu()
{
  if (m_menuCreated)
  {
     m_keytab->setItemChecked(n_keytab,false);
     m_keytab->setItemChecked(se->keymapNo(),true);
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
  if (fontno == -1)
  {
    fontno = n_font;
  }
  else if (fontno == DEFAULTFONT)
  {
    te->setVTFont(defaultFont);
  }
  else if (fonts[fontno][0] == '-')
  {
    QFont f;
    f.setRawName( fonts[fontno] );
    f.setFixedPitch(true);
    f.setStyleHint(QFont::TypeWriter);
    if ( !f.exactMatch() && fontno != DEFAULTFONT)
    {
      // Ugly hack to prevent bug #20487
      fontNotFound_par=fonts[fontno];
      QTimer::singleShot(1,this,SLOT(fontNotFound()));
      return;
    }
    te->setVTFont(f);
  }
  else
  {
    QFont f;
    f.setFamily("fixed");
    f.setFixedPitch(true);
    f.setStyleHint(QFont::TypeWriter);
    f.setPixelSize(QString(fonts[fontno]).toInt());
    te->setVTFont(f);
  }

  if (se) se->setFontNo(fontno);

  if (selectFont)
  {
     QStringList items = selectFont->items();
     int i = fontno;
     int j = 0;
     for(;j < (int)items.count();j++)
     {
       if (!items[j].isEmpty())
          if (!i--)
             break;
     }
     selectFont->setCurrentItem(j);
  }

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
  if (b_fixedSize)
  {
     adjustSize();
     setFixedSize(sizeHint());
  }
  if (!showMenubar->isChecked()) {
    setCaption(i18n("Use the right mouse button to bring back the menu"));
    QTimer::singleShot(5000,this,SLOT(updateTitle()));
  }
  updateRMBMenu();
}

void Konsole::initTEWidget(TEWidget* new_te, TEWidget* default_te)
{
  new_te->setWordCharacters(default_te->wordCharacters());
  new_te->setTerminalSizeHint(default_te->isTerminalSizeHint());
  new_te->setTerminalSizeStartup(false);
  new_te->setFrameStyle(b_framevis?(QFrame::WinPanel|QFrame::Sunken):QFrame::NoFrame);
  new_te->setBlinkingCursor(default_te->blinkingCursor());
  new_te->setCtrlDrag(default_te->ctrlDrag());
  new_te->setCutToBeginningOfLine(default_te->cutToBeginningOfLine());
  new_te->setLineSpacing(default_te->lineSpacing());
  new_te->setBidiEnabled(b_bidiEnabled);

  new_te->setVTFont(default_te->font());
  new_te->setScrollbarLocation(n_scroll);
  new_te->setBellMode(default_te->bellMode());

  new_te->setMinimumSize(150,70);
}

void Konsole::switchToTabWidget()
{
  TEWidget* se_widget = se->widget();
  makeTabWidget();

  QPtrListIterator<TESession> ses_it(sessions);
  while(TESession* _se=ses_it.current()) {
    TEWidget* new_te=new TEWidget(tabwidget);

    connect( new_te, SIGNAL(configureRequest(TEWidget*, int, int, int)),
             this, SLOT(configureRequest(TEWidget*,int,int,int)) );
    initTEWidget(new_te, se_widget);

    tabwidget->insertTab(new_te,SmallIconSet(_se->IconName()),_se->Title());
    setSchema(_se->schemaNo(),new_te);

    new_te->calcGeometry();
    _se->changeWidget(new_te);

    ++ses_it;
  }

  if (rootxpms[se_widget]) {
    delete rootxpms[se_widget];
    rootxpms.remove(se_widget);
  }
  delete se_widget;
  setCentralWidget(tabwidget);
  tabwidget->showPage(se->widget());
  tabwidget->show();
}

void Konsole::switchToFlat()
{
  TEWidget* se_widget = se->widget();

  te=new TEWidget(this);

  connect( te, SIGNAL(configureRequest(TEWidget*, int, int, int)),
           this, SLOT(configureRequest(TEWidget*,int,int,int)) );

  initTEWidget(te, se_widget);
  te->setFocus();

  setCentralWidget(te);
  te->show();

  te->calcGeometry();

  sessions.first();
  while(sessions.current())
  {
    sessions.current()->changeWidget(te);
    sessions.next();
  }
  setSchema(se->schemaNo());

  for (int i=0;i<tabwidget->count();i++)
    if (rootxpms[tabwidget->page(i)]) {
      delete rootxpms[tabwidget->page(i)];
      rootxpms.remove(tabwidget->page(i));
    }
  delete tabwidget;
  tabwidget = 0L;
}

/**
    Toggle the Tabbar visibility
 */
void Konsole::slotSelectTabbar() {
   if (m_menuCreated)
      n_tabbar = selectTabbar->currentItem();

  if (n_tabbar!=TabNone) {
    if (!tabwidget)
      switchToTabWidget();
    if (n_tabbar==TabTop)
      tabwidget->setTabPosition(QTabWidget::Top);
    else
      tabwidget->setTabPosition(QTabWidget::Bottom);
    QPtrDictIterator<KRootPixmap> it(rootxpms);
    for (;it.current();++it)
      it.current()->repaint(true);
  }
  else if (tabwidget)
    switchToFlat();

  if (b_fixedSize)
  {
     adjustSize();
     setFixedSize(sizeHint());
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

void Konsole::slotConfigureNotifications()
{
   KNotifyDialog::configure(this, "Notification Configuration Dialog");
}

void Konsole::slotConfigureKeys()
{
   KKeyDialog::configure(m_shortcuts);
   m_shortcuts->writeShortcutSettings();
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
  buildSessionMenus();

  if (tabwidget) {
    for (TESession *_se = sessions.first(); _se; _se = sessions.next()) {
       ColorSchema* s = colors->find( _se->schemaNo() );
       if (s) {
         if (s->hasSchemaFileChanged())
           s->rereadSchemaFile();
         setSchema(s,_se->widget());
       }
    }
  }
  else
    setSchema(curr_schema);
  for (KonsoleChild *child = detached.first(); child; child = detached.next()) {
     int numb = child->session()->schemaNo();
     ColorSchema* s = colors->find(numb);
     if (s) {
       if (s->hasSchemaFileChanged())
         s->rereadSchemaFile();
       child->setSchema(s);
     }
  }
}

// --| color selection |-------------------------------------------------------

void Konsole::changeColumns(int columns)
{
  if (b_allowResize) {
    setColLin(columns,te->Lines());
    te->update();
  }
}

void Konsole::slotSelectSize() {
    int item = selectSize->currentItem();
    if (b_fullscreen)
       slotToggleFullscreen();

    switch (item) {
    case 0: setColLin(40,15); break;
    case 1: setColLin(80,24); break;
    case 2: setColLin(80,25); break;
    case 3: setColLin(80,40); break;
    case 4: setColLin(80,52); break;
    case 6: SizeDialog dlg(te->Columns(), te->Lines(), this);
            if (dlg.exec())
              setColLin(dlg.columns(),dlg.lines());
            break;
   }
}


void Konsole::notifySize(int lines, int columns)
{
  if (selectSize)
  {
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
  }

  if (n_render >= 3) pixmap_menu_activated(n_render);
}

void Konsole::updateTitle()
{
  setCaption( se->fullTitle() );
  setIconText( se->IconText() );
}

void Konsole::initSessionFont(int fontNo) {
  if (fontNo == -1) return; // Don't change
  setFont(fontNo);
}

void Konsole::initSessionKeyTab(const QString &keyTab) {
  se->setKeymap(keyTab);
  updateKeytabMenu();
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

void Konsole::slotToggleFullscreen()
{
  setFullScreen(!b_fullscreen);
  te->setFrameStyle( b_framevis && !b_fullscreen ?(QFrame::WinPanel|QFrame::Sunken):QFrame::NoFrame );
}

void Konsole::setFullScreen(bool on)
{
  if( on )
    showFullScreen(); // both calls will generate event triggering updateFullScreen()
  else {
    if( isFullScreen()) // showNormal() may also do unminimize, unmaximize etc. :(
        showNormal();
  }
}

void Konsole::updateFullScreen()
{
  if( isFullScreen() == b_fullscreen )
      return;
  b_fullscreen = isFullScreen();
  if (b_fullscreen) {
    te->setFrameStyle( QFrame::NoFrame );
  }
  else {
    te->setFrameStyle( b_framevis?(QFrame::WinPanel|QFrame::Sunken):QFrame::NoFrame );
    updateTitle(); // restore caption of window
  }
  if (m_fullscreen)
    m_fullscreen->setChecked(b_fullscreen);
    
  updateRMBMenu();
}

bool Konsole::event( QEvent* e )
{
    if( e->type() == QEvent::ShowFullScreen || e->type() == QEvent::ShowNormal )
        updateFullScreen();
    return KMainWindow::event( e );
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
      ses->setListenToKeyPress(true);
  else
    se->setListenToKeyPress(true);
}

void Konsole::feedAllSessions(const QString &text)
{
  for (TESession *ses = sessions.first(); ses; ses = sessions.next())
    ses->setListenToKeyPress(true);
  if (te)
    te->emitText(text);
  if(!se->isMasterMode()) {
    for (TESession *ses = sessions.first(); ses; ses = sessions.next())
      ses->setListenToKeyPress(false);
    se->setListenToKeyPress(true);
  }
}

void Konsole::sendAllSessions(const QString &text)
{
  QString newtext=text;
  newtext.append("\r");
  feedAllSessions(newtext);
}

KURL Konsole::baseURL() const
{
   KURL url;
   url.setPath(se->getCwd()+"/");
   return url;
}

void Konsole::enterURL(const QString& URL, const QString&)
{
  QString path, login, host, newtext;
  int i;

  if (se->isMasterMode()) {
    clearAllListenToKeyPress();
    se->setListenToKeyPress(true);
  }
  if (URL.startsWith("file:")) {
    KURL uglyurl(URL);
    newtext=uglyurl.prettyURL().mid(5);
    KRun::shellQuote(newtext);
    te->emitText("cd "+newtext+"\r");
  }
  else if (URL.contains("://", true)) {
    i = URL.find("://", 0);
    newtext = URL.left(i);
    path = URL.mid(i + 3);
    /*
     * Is it protocol://user@host, or protocol://host ?
     */
    if (path.contains("@", true)) {
      i = path.find("@", 0);
      login = path.left(i);
      host = path.mid(i + 1);
      if (!login.isEmpty()) {
	newtext = newtext + " -l " + login;
      }
    } else {
      host = path;
    }

    /*
     * If we have a host, connect.
     */
    if (!host.isEmpty()) {
      newtext = newtext + " " + host;
      se->setUserTitle(31,"");           // we don't know remote cwd
      te->emitText(newtext + "\r");
    }
  }
  else
    te->emitText(URL);
  restoreAllListenToKeyPress();
}

void Konsole::slotClearTerminal()
{
  if (se) {
    se->getEmulation()->clearEntireScreen();
    se->getEmulation()->clearSelection();
  }
}

void Konsole::slotResetClearTerminal()
{
  if (se) {
    se->getEmulation()->reset();
    se->getEmulation()->clearSelection();
  }
}

void Konsole::sendSignal(int sn)
{
  if (se) se->sendSignal(sn);
}

void Konsole::runSession(TESession* s)
{
    KRadioAction *ra = session2action.find(s);
    ra->setChecked(true);
    activateSession(s);

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
     for (KonsoleChild *child = detached.first(); child; child = detached.next())
     {
        if (newTitle == child->session()->Title())
        {
           nameOk = false;
           break;
        }
     }
     if (!nameOk)
     {
       count++;
       newTitle = i18n("abbreviation of number","%1 No. %2").arg(s->Title()).arg(count);
     }
  }
  while (!nameOk);

  s->setTitle(newTitle);

  // create an action for the session
  KRadioAction *ra = new KRadioAction(newTitle.replace('&',"&&"),
                                      s->IconName(),
                                      0,
                                      this,
                                      SLOT(activateSession()),
                                      this);
  ra->setExclusiveGroup("sessions");
  ra->setChecked(true);

  action2session.insert(ra, s);
  session2action.insert(s,ra);
  sessions.append(s);
  if (sessions.count()>1) {
     if (!m_menuCreated)
        makeGUI();
     m_detachSession->setEnabled(true);
  }

  if (m_menuCreated)
     ra->plug(m_view);

  if (tabwidget) {
  //KONSOLEDEBUG<<"Konsole ctor() after new TEWidget() "<<time.elapsed()<<" msecs elapsed"<<endl;
    tabwidget->insertTab(te,SmallIconSet(s->IconName()),newTitle);
    setSchema(s->schemaNo());
    tabwidget->setCurrentPage(tabwidget->count()-1);
  }
}

QString Konsole::currentSession()
{
  return se->SessionId();
}

QString Konsole::sessionId(const int position)
{
  if (position<=0 || position>(int)sessions.count())
    return "";

  return sessions.at(position-1)->SessionId();
}

void Konsole::listSessions()
{
  int counter=0;
  KPopupMenu* m_sessionList = new KPopupMenu(this);
  m_sessionList->insertTitle(i18n("Session List"));
#if KDE_VERSION >=306
  m_sessionList->setKeyboardShortcutsEnabled(true);
#endif
  for (TESession *ses = sessions.first(); ses; ses = sessions.next()) {
    QString title=ses->Title();
    m_sessionList->insertItem(SmallIcon(ses->IconName()),title.replace('&',"&&"),counter++);
  }
  connect(m_sessionList, SIGNAL(activated(int)), SLOT(activateSession(int)));
  m_sessionList->adjustSize();
  m_sessionList->popup(mapToGlobal(QPoint((width()/2)-(m_sessionList->width()/2),(height()/2)-(m_sessionList->height()/2))));
}

void Konsole::activateSession(int position)
{
  if (position<0 || position>=(int)sessions.count())
    return;
  activateSession( sessions.at(position) );
}

void Konsole::activateSession(QWidget* w)
{
  activateSession(tabwidget->indexOf(w));
  w->setFocus();
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
   Activates a session from the menu
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
     se->setConnect(false);
     if(se->isMasterMode())
       for (TESession *_se = sessions.first(); _se; _se = sessions.next())
         _se->setListenToKeyPress(false);

     notifySessionState(se,NOTIFYNORMAL);
     // Delete the session if isn't in the session list any longer.
     if (sessions.find(se) == -1)
        delete se;
  }
  se_previous = se;
  se = s;
  session2action.find(se)->setChecked(true);
  QTimer::singleShot(1,this,SLOT(allowPrevNext())); // hack, hack, hack

  if (tabwidget) {
    tabwidget->showPage( se->widget() );
    te = se->widget();
    if (m_menuCreated) {
      selectBell->setCurrentItem(te->bellMode());
      selectFont->setCurrentItem(se->fontNo());
      updateSchemaMenu();
    }
  }
  else {
    if (s->schemaNo()!=curr_schema)
    {
       // the current schema has changed
       setSchema(s->schemaNo());
    }

    if (s->fontNo() != n_font)
    {
        setFont(s->fontNo());
    }
  }

  notifySize(te->Lines(), te->Columns());  // set menu items (strange arg order !)
  s->setConnect(true);
  if(se->isMasterMode())
    for (TESession *_se = sessions.first(); _se; _se = sessions.next())
      _se->setListenToKeyPress(true);
  updateTitle();
  if (!m_menuCreated)
     return;

  updateKeytabMenu(); // act. the keytab for this session
  m_clearHistory->setEnabled( se->history().isOn() );
  m_findHistory->setEnabled( se->history().isOn() );
  m_findNext->setEnabled( se->history().isOn() );
  m_findPrevious->setEnabled( se->history().isOn() );
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
  if (!se) return;
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

void Konsole::newSession(const QString &pgm, const QStrList &args, const QString &term, const QString &icon, const QString &cwd)
{
  KSimpleConfig *co = defaultSession();
  newSession(co, pgm, args, term, icon, QString::null, cwd);
}

QString Konsole::newSession()
{
  KSimpleConfig *co = defaultSession();
  return newSession(co, QString::null, QStrList());
}

void Konsole::newSession(int i)
{
  KSimpleConfig* co = no2command.find(i);
  if (co) {
    newSession(co);
    resetScreenSessions();
  }
}

void Konsole::newSessionTabbar(int i)
{
  KSimpleConfig* co = no2command.find(i);
  if (co) {
    newSession(co);
    resetScreenSessions();
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

QString Konsole::newSession(KSimpleConfig *co, QString program, const QStrList &args, const QString &_term,const QString &_icon,const QString &_title, const QString &_cwd)
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

  if (!_cwd.isEmpty())
     cwd = _cwd;

  if (!program.isEmpty()) {
     cmdArgs = args;
  }
  else {
     program = QFile::decodeName(konsole_shell(cmdArgs));

     if (co) {
        co->setDesktopGroup();
        QString cmd = co->readPathEntry("Exec");

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

  if (tabwidget) {
    TEWidget* te_old = te;
    te=new TEWidget(tabwidget);

    connect( te, SIGNAL(configureRequest(TEWidget*, int, int, int)),
             this, SLOT(configureRequest(TEWidget*,int,int,int)) );
    if (te_old) {
      initTEWidget(te, te_old);
    }
    else {
      readProperties(KGlobal::config(), "", true);
      setFont(QMIN(fno, TOPFONT));
      te->setScrollbarLocation(n_scroll);
      te->setBellMode(n_bell);
    }

    te->setMinimumSize(150,70);
  }

  QString sessionId="session-"+QString::number(++sessionIdCounter);
  TESession* s = new TESession(te, QFile::encodeName(program),cmdArgs,emu,winId(),sessionId,cwd);
  s->setMonitorSilenceSeconds(monitorSilenceSeconds);
  s->enableFullScripting(b_fullScripting);
  // If you add any new signal-slot connection below, think about doing it in konsolePart too
  connect( s,SIGNAL(done(TESession*)),
           this,SLOT(doneSession(TESession*)) );
  connect( s, SIGNAL( updateTitle() ),
           this, SLOT( updateTitle() ) );
  connect( s, SIGNAL( notifySessionState(TESession*, int) ),
           this, SLOT( notifySessionState(TESession*, int)) );
  connect( s, SIGNAL(clearAllListenToKeyPress()),
           this, SLOT(clearAllListenToKeyPress()) );
  connect( s, SIGNAL(restoreAllListenToKeyPress()),
           this, SLOT(restoreAllListenToKeyPress()) );
  connect( s, SIGNAL(renameSession(TESession*,const QString&)),
           this, SLOT(slotRenameSession(TESession*, const QString&)) );
  connect( s->getEmulation(), SIGNAL(changeColumns(int)),
           this, SLOT(changeColumns(int)) );
  connect( s->getEmulation(), SIGNAL(ImageSizeChanged(int,int)),
           this, SLOT(notifySize(int,int)));
  connect( s, SIGNAL(zmodemDetected(TESession*)),
           this, SLOT(slotZModemDetected(TESession*)));

  s->setFontNo(QMIN(fno, TOPFONT));
  s->setSchemaNo(schmno);
  if (key.isEmpty())
    s->setKeymapNo(n_defaultKeytab);
  else
    s->setKeymap(key);
  s->setTitle(txt);
  s->setIconName(icon);
  s->setAddToUtmp(b_addToUtmp);
  s->setXonXoff(b_xonXoff);

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

/*
 * Starts a new session based on URL.
 */
void Konsole::newSession(const QString& sURL, const QString& title)
{
   QStrList args;
   QString protocol, path, login, host;

   KURL url = KURL(sURL);
   if ((url.protocol() == "file") && (url.hasPath())) {
     KSimpleConfig *co = defaultSession();
     path = url.path();
     newSession(co, QString::null, QStrList(), QString::null, QString::null,
                title.isEmpty() ? path : title, path);
     return;
   }
   else if ((!url.protocol().isEmpty()) && (url.hasHost())) {
     protocol = url.protocol();
     args.append( protocol.latin1() ); /* argv[0] == command to run. */
     host = url.host();
     if (url.hasUser()) {
       login = url.user();
       args.append("-l");
       args.append(login.latin1());
     }
     args.append(host.latin1());
     newSession( NULL, protocol.latin1() /* protocol */, args /* arguments */,
                 QString::null /*term*/, QString::null /*icon*/,
 	        title.isEmpty() ? path : title /*title*/, QString::null /*cwd*/);
     return;
   }
   /*
    * We can't create a session without a protocol.
    * We should ideally popup a warning.
    */
}

void Konsole::closeCurrentSession()
{
  se->closeSession();
}

void Konsole::doneChild(KonsoleChild* child, TESession* session)
{
  if (session)
    attachSession(session);
  detached.remove(child);
}

//FIXME: If a child dies during session swap,
//       this routine might be called before
//       session swap is completed.

void Konsole::doneSession(TESession* s)
{
// qWarning("Konsole::doneSession(): Exited:%d ExitStatus:%d\n", WIFEXITED(status),WEXITSTATUS(status));
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
  if (tabwidget) {
    tabwidget->removePage( s->widget() );
    if (rootxpms[s->widget()]) {
      delete rootxpms[s->widget()];
      rootxpms.remove(s->widget());
    }
    delete s->widget();
  }
  session2action.remove(s);
  action2session.remove(ra);
  int sessionIndex = sessions.findRef(s);
  sessions.remove(s);
  delete ra; // will the toolbar die?

  s->setConnect(false);
  if(s->isMasterMode())
    for (TESession *_se = sessions.first(); _se; _se = sessions.next())
      _se->setListenToKeyPress(false);

  delete s;
  if (s == se_previous)
    se_previous=NULL;
  if (s == se)
  { // pick a new session
    se = 0;
    if (sessions.count() || detached.count())
    {
      if (sessions.count()) {
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
      else {
        KonsoleChild* child=detached.first();
        delete child;
        detached.remove(child);
      }
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
  if (sessions.count()==1)
    m_detachSession->setEnabled(false);
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

void Konsole::slotMovedTab(int from, int to)
{
  //kdDebug() << "Konsole::slotMovedTab(" << from <<","<<to<<")"<<endl;

  TESession* _se = sessions.take(from);
  sessions.remove(_se);
  sessions.insert(to,_se);

  KRadioAction *ra = session2action.find(_se);
  ra->unplug(m_view);
  ra->plug(m_view,(m_view->count()-sessions.count()+1)+to);

  if (to==tabwidget->currentPageIndex()) {
    if (!m_menuCreated)
      makeGUI();
    m_moveSessionLeft->setEnabled(to>0);
    m_moveSessionRight->setEnabled(to<(int)sessions.count()-1);
  }
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

  if (tabwidget) {
    tabwidget->blockSignals(true);
    tabwidget->removePage(se->widget());
    tabwidget->blockSignals(false);
    tabwidget->insertTab(se->widget(), SmallIconSet( se->isMasterMode()?"remote":se->IconName() ), se->Title(), position-1 );
    tabwidget->showPage(se->widget());
  }

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

  if (tabwidget) {
    tabwidget->blockSignals(true);
    tabwidget->removePage(se->widget());
    tabwidget->blockSignals(false);
    tabwidget->insertTab(se->widget(), SmallIconSet( se->isMasterMode()?"remote":se->IconName() ), se->Title(), position+1 );
    tabwidget->showPage(se->widget());
  }

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
      ses->setListenToKeyPress(true);
  else {
    for (TESession *ses = sessions.first(); ses; ses = sessions.next())
      ses->setListenToKeyPress(false);
    se->setListenToKeyPress(true);
  }
  notifySessionState(se,NOTIFYNORMAL);
}

void Konsole::notifySessionState(TESession* session, int state)
{
  if (!tabwidget) {
    session->testAndSetStateIconName("noneset");
    return;
  }

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
    tabwidget->setTabIconSet(session->widget(), SmallIconSet(state_iconname));
}

// --| Session support |-------------------------------------------------------

void Konsole::buildSessionMenus()
{
   m_session->clear();
   if (m_tabbarSessionsCommands)
      m_tabbarSessionsCommands->clear();

   no2command.clear();
   no2tempFile.clear();
   no2filename.clear();

   cmd_serial = 0;
   cmd_first_screen = -1;

   loadSessionCommands();
   loadScreenSessions();

   if (kapp->authorizeKAction("file_print"))
   {
      m_session->insertSeparator();
      m_print->plug(m_session);
   }

   m_session->insertSeparator();
   m_closeSession->plug(m_session);

   m_session->insertSeparator();
   m_quit->plug(m_session);
}

static void insertItemSorted(KPopupMenu *menu, const QIconSet &iconSet, const QString &txt, int id)
{
  const int defaultId = 1; // The id of the 'new' item.
  int index = menu->indexOf(defaultId);
  int count = menu->count();
  if (index >= 0)
  {
     index++; // Skip separator
     while(true)
     {
        index++;
        if (index >= count)
        {
           index = -1; // Insert at end
           break;
        }
        if (menu->text(menu->idAt(index)) > txt)
           break; // Insert before this item
     }
  }
  menu->insertItem(iconSet, txt, id, index);
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
    co = new KSimpleConfig(path,true);
  co->setDesktopGroup();
  QString typ = co->readEntry("Type");
  QString txt = co->readEntry("Name");

  // try to locate the binary
  QString exec= co->readPathEntry("Exec");
  if (exec.startsWith("su -c \'")) {
     exec = exec.mid(7,exec.length()-8);
  }

  exec = KRun::binaryName(exec, false);
  QString pexec = KGlobal::dirs()->findExe(exec);
  if (typ.isEmpty() || txt.isEmpty() || typ != "KonsoleApplication"
      || ( !exec.isEmpty() && pexec.isEmpty() ) )
  {
    if (!path.isEmpty())
       delete co;
    return; // ignore

  }

  QString icon = co->readEntry("Icon", "openterm");
  insertItemSorted(m_tabbarSessionsCommands, SmallIconSet( icon ), txt.replace('&',"&&"), ++cmd_serial );
  QString comment = co->readEntry("Comment");
  if (comment.isEmpty())
    comment=txt.prepend(i18n("New "));
  insertItemSorted( m_session, SmallIconSet( icon ), comment.replace('&',"&&"), cmd_serial );
  no2command.insert(cmd_serial,co);

  int j = filename.findRev('/');
  if (j > -1)
    filename = filename.mid(j+1);
  no2filename.insert(cmd_serial,new QString(filename));
}


void Konsole::loadSessionCommands()
{
  if (!kapp->authorize("shell_access"))
     return;
  addSessionCommand(QString::null);
  m_session->insertSeparator();
  m_tabbarSessionsCommands->insertSeparator();

  QStringList lst = KGlobal::dirs()->findAllResources("appdata", "*.desktop", false, true);

  for(QStringList::Iterator it = lst.begin(); it != lst.end(); ++it )
    if (!(*it).endsWith("/shell.desktop"))
       addSessionCommand(*it);

  if (m_bookmarksSession)
  {
    m_session->insertSeparator();
    m_session->insertItem(SmallIconSet("keditbookmarks"),
                          i18n("New Shell at Bookmark"), m_bookmarksSession);
    m_tabbarSessionsCommands->insertSeparator();
    m_tabbarSessionsCommands->insertItem(SmallIconSet("keditbookmarks"),
                          i18n("Shell at Bookmark"), m_bookmarksSession);
  }
}

void Konsole::addScreenSession(const QString &path, const QString &socket)
{
  KTempFile *tmpFile = new KTempFile();
  tmpFile->setAutoDelete(true);
  KSimpleConfig *co = new KSimpleConfig(tmpFile->name());
  co->setDesktopGroup();
  co->writeEntry("Name", socket);
  QString txt = i18n("Screen is a program controlling screens!", "Screen at %1").arg(socket);
  co->writeEntry("Comment", txt);
  co->writePathEntry("Exec", QString::fromLatin1("SCREENDIR=%1 screen -r %2")
    .arg(path).arg(socket));
  QString icon = "openterm"; // FIXME use another icon (malte)
  cmd_serial++;
  m_session->insertItem( SmallIconSet( icon ), txt, cmd_serial, cmd_serial - 1 );
  m_tabbarSessionsCommands->insertItem( SmallIconSet( icon ), txt, cmd_serial );
  no2command.insert(cmd_serial,co);
  no2tempFile.insert(cmd_serial,tmpFile);
  no2filename.insert(cmd_serial,new QString(""));
}

void Konsole::loadScreenSessions()
{
  if (!kapp->authorize("shell_access"))
     return;
  QCString screenDir = getenv("SCREENDIR");
  if (screenDir.isEmpty())
    screenDir = QFile::encodeName(QDir::homeDirPath()) + "/.screen/";
  // Some distributions add a shell function called screen that sets
  // $SCREENDIR to ~/tmp. In this case the variable won't be set here.
  if (!QFile::exists(screenDir))
    screenDir = QFile::encodeName(QDir::homeDirPath()) + "/tmp/";
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
  resetScreenSessions();
  for (QStringList::ConstIterator it = sessions.begin(); it != sessions.end(); ++it)
    addScreenSession(screenDir, *it);
}

void Konsole::resetScreenSessions()
{
  if (cmd_first_screen == -1)
    cmd_first_screen = cmd_serial + 1;
  else
  {
    for (int i = cmd_first_screen; i <= cmd_serial; ++i)
    {
      m_session->removeItem(i);
      if (m_tabbarSessionsCommands)
         m_tabbarSessionsCommands->removeItem(i);
      no2command.remove(i);
      no2tempFile.remove(i);
      no2filename.remove(i);
    }
    cmd_serial = cmd_first_screen - 1;
  }
}

// --| Schema support |-------------------------------------------------------

void Konsole::setSchema(int numb, TEWidget* tewidget)
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
	  if (s) setSchema(s, tewidget);
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

void Konsole::setSchema(ColorSchema* s, TEWidget* tewidget)
{
  if (!s) return;
  if (!tewidget) tewidget=te;

//        KONSOLEDEBUG << "Checking menu items" << endl;
  if (tewidget==te) {
    if (m_schema)
    {
      m_schema->setItemChecked(curr_schema,false);
      m_schema->setItemChecked(s->numb(),true);
    }
//        KONSOLEDEBUG << "Remembering schema data" << endl;

    s_schema = s->relPath();
    curr_schema = s->numb();
    pmPath = s->imagePath();
  }
  tewidget->setColorTable(s->table()); //FIXME: set twice here to work around a bug

  if (s->useTransparency()) {
//        KONSOLEDEBUG << "Setting up transparency" << endl;
    if (!rootxpms[tewidget])
      rootxpms.insert( tewidget, new KRootPixmap(tewidget) );
    rootxpms[tewidget]->setFadeEffect(s->tr_x(), QColor(s->tr_r(), s->tr_g(), s->tr_b()));
    rootxpms[tewidget]->start();
    rootxpms[tewidget]->repaint(true);
  } else {
//        KONSOLEDEBUG << "Stopping transparency" << endl;
      if (rootxpms[tewidget]) {
        delete rootxpms[tewidget];
        rootxpms.remove(tewidget);
      }
       pixmap_menu_activated(s->alignment(), tewidget);
  }

  tewidget->setColorTable(s->table());
  if (tabwidget) {
    QPtrListIterator<TESession> ses_it(sessions);
    for (; ses_it.current(); ++ses_it)
      if (tewidget==ses_it.current()->widget()) {
        ses_it.current()->setSchemaNo(s->numb());
        break;
      }
  }
  else if (se)
    se->setSchemaNo(s->numb());
}

void Konsole::detachSession() {
  KRadioAction *ra = session2action.find(se);
  ra->unplug(m_view);
  TEWidget* se_widget = se->widget();
  session2action.remove(se);
  action2session.remove(ra);
  int sessionIndex = sessions.findRef(se);
  sessions.remove(se);
  delete ra;

  disconnect( se,SIGNAL(done(TESession*)),
              this,SLOT(doneSession(TESession*)) );

  disconnect( se->getEmulation(),SIGNAL(ImageSizeChanged(int,int)), this,SLOT(notifySize(int,int)));
  disconnect( se->getEmulation(),SIGNAL(changeColumns(int)), this,SLOT(changeColumns(int)) );

  disconnect( se,SIGNAL(updateTitle()), this,SLOT(updateTitle()) );
  disconnect( se,SIGNAL(notifySessionState(TESession*,int)), this,SLOT(notifySessionState(TESession*,int)) );
  disconnect( se,SIGNAL(clearAllListenToKeyPress()), this,SLOT(clearAllListenToKeyPress()) );
  disconnect( se,SIGNAL(restoreAllListenToKeyPress()), this,SLOT(restoreAllListenToKeyPress()) );
  disconnect( se,SIGNAL(renameSession(TESession*,const QString&)), this,SLOT(slotRenameSession(TESession*,const QString&)) );

  ColorSchema* schema = colors->find(se->schemaNo());
  KonsoleChild* konsolechild = new KonsoleChild(se,te->Columns(),te->Lines(),n_scroll,
                                                b_framevis?(QFrame::WinPanel|QFrame::Sunken):QFrame::NoFrame,
                                                schema,te->font(),te->bellMode(),te->wordCharacters(),
                                                te->blinkingCursor(),te->ctrlDrag(),te->isTerminalSizeHint(),
                                                te->lineSpacing(),te->cutToBeginningOfLine(),b_allowResize, b_fixedSize);
  detached.append(konsolechild);
  konsolechild->show();
  konsolechild->run();

  connect( konsolechild,SIGNAL(doneChild(KonsoleChild*, TESession*)),
           this,SLOT(doneChild(KonsoleChild*, TESession*)) );

  if (se == se_previous)
    se_previous=NULL;

  // pick a new session
  if (se_previous)
    se = se_previous;
  else
    se = sessions.at(sessionIndex ? sessionIndex - 1 : 0);
  session2action.find(se)->setChecked(true);
  QTimer::singleShot(1,this,SLOT(activateSession()));
  if (sessions.count()==1)
    m_detachSession->setEnabled(false);

  if (tabwidget) {
    tabwidget->removePage( se_widget );
    if (rootxpms[se_widget]) {
      delete rootxpms[se_widget];
      rootxpms.remove(se_widget);
    }
    delete se_widget;
  }
}

void Konsole::attachSession(TESession* session)
{
  TEWidget* se_widget = se->widget();
  if (tabwidget) {
    te=new TEWidget(tabwidget);

    connect( te, SIGNAL(configureRequest(TEWidget*, int, int, int)),
             this, SLOT(configureRequest(TEWidget*,int,int,int)) );

    initTEWidget(te, se_widget);
    session->changeWidget(te);
    tabwidget->insertTab(te,SmallIconSet(session->IconName()),session->Title());
    setSchema(session->schemaNo());
  }
  else
    session->changeWidget(te);

  QString title=session->Title();
  KRadioAction *ra = new KRadioAction(title.replace('&',"&&"), session->IconName(),
                                      0, this, SLOT(activateSession()), this);

  ra->setExclusiveGroup("sessions");
  ra->setChecked(true);

  action2session.insert(ra, session);
  session2action.insert(session,ra);
  sessions.append(session);
  if (sessions.count()>1)
    m_detachSession->setEnabled(true);

  if (m_menuCreated)
    ra->plug(m_view);

  connect( session,SIGNAL(done(TESession*)),
           this,SLOT(doneSession(TESession*)) );

  connect( session,SIGNAL(updateTitle()), this,SLOT(updateTitle()) );
  connect( session,SIGNAL(notifySessionState(TESession*,int)), this,SLOT(notifySessionState(TESession*,int)) );

  connect( session,SIGNAL(clearAllListenToKeyPress()), this,SLOT(clearAllListenToKeyPress()) );
  connect( session,SIGNAL(restoreAllListenToKeyPress()), this,SLOT(restoreAllListenToKeyPress()) );
  connect( session,SIGNAL(renameSession(TESession*,const QString&)), this,SLOT(slotRenameSession(TESession*,const QString&)) );
  connect( session->getEmulation(),SIGNAL(ImageSizeChanged(int,int)), this,SLOT(notifySize(int,int)));
  connect( session->getEmulation(),SIGNAL(changeColumns(int)), this,SLOT(changeColumns(int)) );

  activateSession(session);
}

void Konsole::slotRenameSession() {
//  KONSOLEDEBUG << "slotRenameSession\n";
  QString name = se->Title();
  bool ok;

  name = KInputDialog::getText( i18n( "Rename Session" ),
      i18n( "Session name:" ), name, &ok, this );

  if (ok)
    initSessionTitle(name);
}

void Konsole::slotRenameSession(TESession* ses, const QString &name)
{
  KRadioAction *ra = session2action.find(ses);
  QString title=name;
  title=title.replace('&',"&&");
  ra->setText(title);
  ra->setIcon( ses->IconName() ); // I don't know why it is needed here
  if (tabwidget)
    tabwidget->changeTab( ses->widget(), title );
  updateTitle();
}

void Konsole::initSessionTitle(const QString &_title) {
  se->setTitle(_title);
  slotRenameSession(se,_title);
}

void Konsole::slotClearAllSessionHistories() {
  for (TESession *_se = sessions.first(); _se; _se = sessions.next())
    _se->clearHistory();
}

//////////////////////////////////////////////////////////////////////

HistoryTypeDialog::HistoryTypeDialog(const HistoryType& histType,
                                     unsigned int histSize,
                                     QWidget *parent)
  : KDialogBase(Plain, i18n("History Configuration"),
                Help | Default | Ok | Cancel, Ok,
                parent, 0, true, true)
{
  QFrame *mainFrame = plainPage();

  QHBoxLayout *hb = new QHBoxLayout(mainFrame);

  m_btnEnable    = new QCheckBox(i18n("&Enable"), mainFrame);
  connect(m_btnEnable, SIGNAL(toggled(bool)), SLOT(slotHistEnable(bool)));

  m_label = new QLabel(i18n("&Number of lines: "), mainFrame);

  m_size = new QSpinBox(0, 10 * 1000 * 1000, 100, mainFrame);
  m_size->setValue(histSize);
  m_size->setSpecialValueText(i18n("Unlimited (number of lines)", "Unlimited"));

  m_label->setBuddy( m_size );

  m_setUnlimited = new QPushButton(i18n("&Set Unlimited"), mainFrame);
  connect( m_setUnlimited,SIGNAL(clicked()), this,SLOT(slotSetUnlimited()) );

  hb->addWidget(m_btnEnable);
  hb->addSpacing(10);
  hb->addWidget(m_label);
  hb->addWidget(m_size);
  hb->addSpacing(10);
  hb->addWidget(m_setUnlimited);

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
  m_label->setEnabled(b);
  m_size->setEnabled(b);
  m_setUnlimited->setEnabled(b);
  if (b) m_size->setFocus();
}

void HistoryTypeDialog::slotSetUnlimited()
{
  m_size->setValue(0);
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
    m_findNext->setEnabled( dlg.isOn() );
    m_findPrevious->setEnabled( dlg.isOn() );
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
  se->clearHistory();
}

void Konsole::slotFindHistory()
{
  if( !m_finddialog ) {
    m_finddialog = new KonsoleFind( this, "konsolefind", false);
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

void Konsole::slotFindNext()
{
  if( !m_finddialog ) {
    slotFindHistory();
    return;
  }

  QString string;
  string = m_finddialog->getText();
  m_finddialog->setText(string.isEmpty() ? m_find_pattern : string);

  slotFind();
}

void Konsole::slotFindPrevious()
{
  if( !m_finddialog ) {
    slotFindHistory();
    return;
  }

  QString string;
  string = m_finddialog->getText();
  m_finddialog->setText(string.isEmpty() ? m_find_pattern : string);

  m_finddialog->setDirection( !m_finddialog->get_direction() );
  slotFind();
  m_finddialog->setDirection( !m_finddialog->get_direction() );
}

void Konsole::slotFind()
{
  if (m_find_first) {
    se->getEmulation()->findTextBegin();
    m_find_first = false;
  }

  bool forward = !m_finddialog->get_direction();
  m_find_pattern = m_finddialog->getText();

  if (se->getEmulation()->findTextNext(m_find_pattern,forward,
                          m_finddialog->case_sensitive(),m_finddialog->reg_exp()))
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
  KURL url = KFileDialog::getSaveURL(QString::null, QString::null, 0L, i18n("Save History"));
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

void Konsole::slotZModemUpload()
{
  if (se->zmodemIsBusy())
  {
    KMessageBox::sorry(this,
         i18n("<p>The current session already has a ZModem file transfer in progress."));
    return;
  }
  QString zmodem = KGlobal::dirs()->findExe("sz");
  if (zmodem.isEmpty())
    zmodem = KGlobal::dirs()->findExe("lsz");
  if (zmodem.isEmpty())
  {
    KMessageBox::sorry(this,
                   i18n("<p>No suitable ZModem software was found on "
                        "the system.\n"
                        "<p>You may wish to install the 'rzsz' or 'lrzsz' package.\n"));
    return;
  }

  QStringList files = KFileDialog::getOpenFileNames(QString::null, QString::null, this,
  	i18n("Select Files to Upload"));
  if (files.isEmpty())
    return;

  se->startZModem(zmodem, QString::null, files);
}

void Konsole::slotZModemDetected(TESession *session)
{
  if(se != session)
    activateSession(session);

  QString zmodem = KGlobal::dirs()->findExe("rz");
  if (zmodem.isEmpty())
    zmodem = KGlobal::dirs()->findExe("lrz");
  if (zmodem.isEmpty())
  {
    KMessageBox::information(this,
                   i18n("<p>A ZModem file transfer attempt has been detected, "
                        "but no suitable ZModem software was found on "
                        "the system.\n"
                        "<p>You may wish to install the 'rzsz' or 'lrzsz' package.\n"));
    return;
  }
  KURLRequesterDlg dlg(KGlobalSettings::documentPath(),
                   i18n("A ZModem file transfer attempt has been detected.\n"
                        "Please specify the directory you want to store the file(s):"),
                   this, "zmodem_dlg");
  dlg.setButtonOKText( i18n("&Download"),
                       i18n("Start downloading file to specified directory."),
                       i18n("Start downloading file to specified directory."));
  if (!dlg.exec())
  {
     session->cancelZModem();
  }
  else
  {
     const KURL &url = dlg.selectedURL();
     session->startZModem(zmodem, url.path(), QStringList());
  }
}

void Konsole::slotPrint()
{
  KPrinter printer;
  printer.addDialogPage(new PrintSettings());
  if (printer.setup(this, i18n("Print %1").arg(se->Title())))
  {
    printer.setFullPage(false);
    printer.setCreator("Konsole");
    QPainter paint;
    paint.begin(&printer);
    se->print(paint, printer.option("app-konsole-printfriendly") == "true",
                     printer.option("app-konsole-printexact") == "true");
    paint.end();
  }
}

void Konsole::toggleBidi()
{
  b_bidiEnabled=!b_bidiEnabled;
  QPtrList<TEWidget> tes = activeTEs();
  for (TEWidget *_te = tes.first(); _te; _te = tes.next()) {
    _te->setBidiEnabled(b_bidiEnabled);
    _te->repaint();
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

  hb->addWidget(new QLabel(i18n("Number of columns:"), mainFrame));
  hb->addWidget(m_columns);
  hb->addSpacing(10);
  hb->addWidget(new QLabel(i18n("Number of lines:"), mainFrame));
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

KonsoleFind::KonsoleFind( QWidget *parent, const char *name, bool /*modal*/ )
  : KEdFind( parent, name, false ), m_editorDialog(0), m_editRegExp(0)
{
  QHBox* row = new QHBox( (QWidget*)group );
  m_asRegExp = new QCheckBox( i18n("As &regular expression"), row, "asRegexp" );

  if (!KTrader::self()->query("KRegExpEditor/KRegExpEditor").isEmpty()) {
    m_editRegExp = new QPushButton( i18n("&Edit..."), row, "editRegExp" );
    connect( m_asRegExp, SIGNAL( toggled(bool) ), m_editRegExp, SLOT( setEnabled(bool) ) );
    connect( m_editRegExp, SIGNAL( clicked() ), this, SLOT( slotEditRegExp() ) );
    m_editRegExp->setEnabled( false );
  }
}

void KonsoleFind::slotEditRegExp()
{
  if ( m_editorDialog == 0 )
    m_editorDialog = KParts::ComponentFactory::createInstanceFromQuery<QDialog>( "KRegExpEditor/KRegExpEditor", QString::null, this );

  assert( m_editorDialog );

  KRegExpEditorInterface *iface = dynamic_cast<KRegExpEditorInterface *>( m_editorDialog );
  assert( iface );

  iface->setRegExp( getText() );
  bool ret = m_editorDialog->exec();
  if ( ret == QDialog::Accepted)
    setText( iface->regExp() );
}

bool KonsoleFind::reg_exp() const
{
  return m_asRegExp->isChecked();
}

//////////////////////////////////////////////////////////////////////

void Konsole::slotBackgroundChanged(int desk)
{
  ColorSchema* s = colors->find(curr_schema);
  if (s==0) return;

  // Only update rootxpm if window is visible on current desktop
  NETWinInfo info( qt_xdisplay(), winId(), qt_xrootwin(), NET::WMDesktop );

  if (s->useTransparency() && info.desktop()==desk && (0 != rootxpms[te])) {
    //KONSOLEDEBUG << "Wallpaper changed on my desktop, " << desk << ", repainting..." << endl;
    //Check to see if we are on the current desktop. If not, delay the repaint
    //by setting wallpaperSource to 0. Next time our desktop is selected, we will
    //automatically update because we are saying "I don't have the current wallpaper"
    NETRootInfo rootInfo( qt_xdisplay(), NET::CurrentDesktop );
    rootInfo.activate();
    if( rootInfo.currentDesktop() == info.desktop() && rootxpms[te]) {
       //We are on the current desktop, go ahead and update
       //KONSOLEDEBUG << "My desktop is current, updating..." << endl;
       wallpaperSource = desk;
       rootxpms[te]->repaint(true);
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
   if( bNeedUpdate && s->useTransparency() && rootxpms[te]) {
      wallpaperSource = desk;
      rootxpms[te]->repaint(true);
   }
}

///////////////////////////////////////////////////////////

void Konsole::biggerFont(void) {
    assert(se);
    if (defaultFont.pixelSize() == -1)
        defaultFont.setPointSize( defaultFont.pointSize() + 1);
    else
        defaultFont.setPixelSize( defaultFont.pixelSize() + 2 );
    setFont( DEFAULTFONT );
    activateSession();
}

void Konsole::smallerFont(void) {
    assert(se);
    if (defaultFont.pixelSize() == -1)
        defaultFont.setPointSize( defaultFont.pointSize() - 1);
    else
        defaultFont.setPixelSize( defaultFont.pixelSize() - 2 );
    setFont( DEFAULTFONT );
    activateSession();
}


bool Konsole::processDynamic(const QCString &fun, const QByteArray &data, QCString& replyType, QByteArray &replyData)
{
    if (b_fullScripting)
    {
      if (fun == "feedAllSessions(QString)")
      {
        QString arg0;
        QDataStream arg( data, IO_ReadOnly );
        arg >> arg0;
        feedAllSessions(arg0);
        replyType = "void";
        return true;
      }
      else if (fun == "sendAllSessions(QString)")
      {
        QString arg0;
        QDataStream arg( data, IO_ReadOnly );
        arg >> arg0;
        sendAllSessions(arg0);
        replyType = "void";
        return true;
      }
    }
    return KonsoleIface::processDynamic(fun, data, replyType, replyData);
}

QCStringList Konsole::functionsDynamic()
{
    QCStringList funcs = KonsoleIface::functionsDynamic();
    if (b_fullScripting)
    {
      funcs << "void feedAllSessions(QString text)";
      funcs << "void sendAllSessions(QString text)";
    }
    return funcs;
}

void Konsole::enableFullScripting(bool b)
{
    b_fullScripting = b;
    for (TESession *_se = sessions.first(); _se; _se = sessions.next())
       _se->enableFullScripting(b);
}

void Konsole::enableFixedSize(bool b)
{
    b_fixedSize = b;
    if (b_fixedSize)
    {
      delete m_fullscreen;
      m_fullscreen = 0;
    }
}

QPtrList<TEWidget> Konsole::activeTEs()
{
   QPtrList<TEWidget> ret;
   if (tabwidget) {
     if (sessions.count()>0)
       for (TESession *_se = sessions.first(); _se; _se = sessions.next())
          ret.append(_se->widget());
     else if (te)  // check for startup initalization case in newSession()
       ret.append(te);
   }
   else
     if (te) {
       ret.append(te);
       for (KonsoleChild *_child = detached.first(); _child; _child = detached.next())
         ret.append(_child->session()->widget());
     }
   return ret;
}

#include "konsole.moc"

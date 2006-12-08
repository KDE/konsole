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
/* The material contained in here more or less directly orginates from    */
/* kvt, which is copyright (c) 1996 by Matthias Ettrich <ettrich@kde.org> */
/*                                                                        */

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
#include <qimage.h>
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
#include <kaccelmanager.h>

#include <kaction.h>
#include <kshell.h>
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
#include <kcharsets.h>
#include <kcolordialog.h>
#include <kio/netaccess.h>

#include "konsole.h"
#include <netwm.h>
#include "printsettings.h"

#define KONSOLEDEBUG    kdDebug(1211)

#define POPUP_NEW_SESSION_ID 121
#define POPUP_SETTINGS_ID 212

#define SESSION_NEW_WINDOW_ID 1
#define SESSION_NEW_SHELL_ID 100

extern bool argb_visual; // declared in main.cpp and konsole_part.cpp

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

#define DEFAULT_HISTORY_SIZE 1000

Konsole::Konsole(const char* name, int histon, bool menubaron, bool tabbaron, bool frameon, bool scrollbaron,
                 QCString type, bool b_inRestore, const int wanted_tabbar, const QString &workdir )
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
,selectSetEncoding(0)
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
,m_tabViewMode(ShowIconAndText)
,b_dynamicTabHide(false)
,b_autoResizeTabs(false)
,b_installBitmapFonts(false)
,b_framevis(true)
,b_fullscreen(false)
,m_menuCreated(false)
,b_warnQuit(false)
,b_allowResize(true) // Whether application may resize
,b_fixedSize(false) // Whether user may resize
,b_addToUtmp(true)
,b_xonXoff(false)
,b_bidiEnabled(false)
,b_fullScripting(false)
,b_showstartuptip(true)
,b_sessionShortcutsEnabled(false)
,b_sessionShortcutsMapped(false)
,b_matchTabWinTitle(false)
,m_histSize(DEFAULT_HISTORY_SIZE)
,m_separator_id(-1)
,m_newSessionButton(0)
,m_removeSessionButton(0)
,sessionNumberMapper(0)
,sl_sessionShortCuts(0)
,s_workDir(workdir)
{
  isRestored = b_inRestore;
  connect( &m_closeTimeout, SIGNAL(timeout()), this, SLOT(slotCouldNotClose()));

  no2command.setAutoDelete(true);
  menubar = menuBar();

  KAcceleratorManager::setNoAccel( menubar );

  sessionNumberMapper = new QSignalMapper( this );
  connect( sessionNumberMapper, SIGNAL( mapped( int ) ),
          this, SLOT( newSessionTabbar( int ) ) );

  colors = new ColorSchemaList();
  colors->checkSchemas();
  colors->sort();

  KeyTrans::loadAll();

  // create applications /////////////////////////////////////////////////////
  // read and apply default values ///////////////////////////////////////////
  resize(321, 321); // Dummy.
  QSize currentSize = size();
  KConfig * config = KGlobal::config();
  config->setDesktopGroup();
  applyMainWindowSettings(config);
  if (currentSize != size())
     defaultSize = size();

  KSimpleConfig *co;
  if (!type.isEmpty())
    setDefaultSession(type+".desktop");
  co = defaultSession();

  co->setDesktopGroup();
  QString schema = co->readEntry("Schema");
  readProperties(config, schema, false);

  makeBasicGUI();

  if (isRestored) {
    n_tabbar = wanted_tabbar;
    KConfig *c = KApplication::kApplication()->sessionConfig();
//    c->setDesktopGroup();      // Reads from wrong group
    b_dynamicTabHide = c->readBoolEntry("DynamicTabHide", false);
  }

  if (!tabbaron)
    n_tabbar = TabNone;

  makeTabWidget();
  setCentralWidget(tabwidget);

  if (b_dynamicTabHide || n_tabbar==TabNone)
    tabwidget->setTabBarHidden(true);

  if (!histon)
    b_histEnabled=false;

  if (!menubaron)
    menubar->hide();
  if (!frameon) {
    b_framevis=false;
    if (te) te->setFrameStyle( QFrame::NoFrame );
  }
  if (!scrollbaron) {
    n_scroll = TEWidget::SCRNONE;
    if (te) te->setScrollbarLocation(TEWidget::SCRNONE);
  }

//  connect(kapp, SIGNAL(kdisplayFontChanged()), this, SLOT(slotFontChanged()));

  kapp->dcopClient()->setDefaultObject( "konsole" );
}


Konsole::~Konsole()
{
    sessions.first();
    while(sessions.current())
    {
      sessions.current()->closeSession();
      sessions.next();
    }

    // Wait a bit for all childs to clean themselves up.
    while(sessions.count() && KProcessController::theKProcessController->waitForProcessExit(1))
        ;

    sessions.setAutoDelete(true);

    resetScreenSessions();
    if (no2command.isEmpty())
       delete m_defaultSession;

    delete colors;
    colors=0;

    delete kWinModule;
    kWinModule = 0;
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
   if (b_showstartuptip)
      KTipDialog::showTip(this);
}

/* ------------------------------------------------------------------------- */
/*  Make menu                                                                */
/* ------------------------------------------------------------------------- */

void Konsole::updateRMBMenu()
{
   if (!m_rightButton) return;
   int index = 0;
   if (!showMenubar->isChecked() && m_options)
   {
      // Only show when menubar is hidden
      if (!showMenubar->isPlugged( m_rightButton ))
      {
         showMenubar->plug ( m_rightButton, index );
         m_rightButton->insertSeparator( index+1 );
      }
      index = 2;
      m_rightButton->setItemVisible(POPUP_NEW_SESSION_ID,true);
      if (m_separator_id != -1)
         m_rightButton->setItemVisible(m_separator_id,true);
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
      m_rightButton->setItemVisible(m_separator_id,false);
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
      KAcceleratorManager::manage( m_signals );
   }

   // Edit Menu ----------------------------------------------------------------
   m_copyClipboard->plug(m_edit);
   m_pasteClipboard->plug(m_edit);

   m_edit->setCheckable(true);
   if (m_signals)
      m_edit->insertItem( i18n("&Send Signal"), m_signals );

   if ( m_zmodemUpload ) {
      m_edit->insertSeparator();
      m_zmodemUpload->plug( m_edit );
   }

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
   m_moveSessionLeft->setEnabled( false );
   m_moveSessionLeft->plug(m_view);

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
   KAcceleratorManager::manage( m_schema );
   connect(m_schema, SIGNAL(activated(int)), SLOT(schema_menu_activated(int)));
   connect(m_schema, SIGNAL(aboutToShow()), SLOT(schema_menu_check()));

   // Keyboard Options Menu ---------------------------------------------------
   m_keytab = new KPopupMenu(this);
   m_keytab->setCheckable(true);
   KAcceleratorManager::manage( m_keytab );
   connect(m_keytab, SIGNAL(activated(int)), SLOT(keytab_menu_activated(int)));

   //options menu
   if (m_options)
   {
      // Menubar on/off
      showMenubar->plug ( m_options );

      // Tabbar
      selectTabbar = new KSelectAction(i18n("&Tab Bar"), 0, this,
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
                << i18n("&Visible Bell")
                << i18n("N&one");
      selectBell->setItems(bellitems);
      selectBell->plug(m_options);

      checkBitmapFonts();
      KActionMenu* m_fontsizes = new KActionMenu( i18n( "Font" ),
                                                  SmallIconSet( "text" ),
                                                  actions, 0L );
      m_fontsizes->insert( new KAction( i18n( "&Enlarge Font" ),
                           SmallIconSet( "fontsizeup" ), 0, this,
                           SLOT( biggerFont() ), actions,
                           "enlarge_font" ) );
      m_fontsizes->insert( new KAction( i18n( "&Shrink Font" ),
                           SmallIconSet( "fontsizedown" ), 0, this,
                           SLOT( smallerFont() ), actions,
                           "shrink_font" ) );
      m_fontsizes->insert( new KAction( i18n( "Se&lect..." ),
                           SmallIconSet( "font" ), 0, this,
                           SLOT( slotSelectFont() ), actions,
                           "select_font" ) );
      if ( b_installBitmapFonts )
      {
         m_fontsizes->insert( new KAction( i18n( "&Install Bitmap..." ),
                              SmallIconSet( "font" ), 0, this,
                              SLOT( slotInstallBitmapFonts() ), actions,
                              "install_fonts" ) );
      }
      m_fontsizes->plug(m_options);

      // encoding menu, start with default checked !
      selectSetEncoding = new KSelectAction( i18n( "&Encoding" ), SmallIconSet( "charset" ), 0, this, SLOT(slotSetEncoding()), actions, "set_encoding" );
      QStringList list = KGlobal::charsets()->descriptiveEncodingNames();
      list.prepend( i18n( "Default" ) );
      selectSetEncoding->setItems(list);
      selectSetEncoding->setCurrentItem (0);
      selectSetEncoding->plug(m_options);

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
         m_separator_id=m_rightButton->insertSeparator();
         m_rightButton->insertItem(i18n("S&ettings"), m_options, POPUP_SETTINGS_ID);
      }
      m_rightButton->insertSeparator();
      m_closeSession->plug(m_rightButton );
      if (KGlobalSettings::insertTearOffHandle())
         m_rightButton->insertTearOffHandle();
   }

   delete colors;
   colors = new ColorSchemaList();
   colors->checkSchemas();
   colors->sort();
   updateSchemaMenu();
   ColorSchema *sch=colors->find(s_schema);
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
   // This sorting seems a bit cumbersome; but it is not called often.
   QStringList kt_titles;
   typedef QMap<QString,KeyTrans*> QStringKeyTransMap;
   QStringKeyTransMap kt_map;

   for (int i = 0; i < KeyTrans::count(); i++)
   {
      KeyTrans* ktr = KeyTrans::find(i);
      assert( ktr );
      QString title=ktr->hdr().lower();
      kt_titles << title;
      kt_map[title] = ktr;
   }
   kt_titles.sort();
   for ( QStringList::Iterator it = kt_titles.begin(); it != kt_titles.end(); ++it ) {
      KeyTrans* ktr = kt_map[*it];
      assert( ktr );
      QString title=ktr->hdr();
      m_keytab->insertItem(title.replace('&',"&&"),ktr->numb());
   }

   applySettingsToGUI();
   isRestored = false;


   // Fill tab context menu
   m_tabPopupMenu = new KPopupMenu( this );
   KAcceleratorManager::manage( m_tabPopupMenu );

   m_tabDetachSession= new KAction( i18n("&Detach Session"), SmallIconSet("tab_breakoff"), 0, this, SLOT(slotTabDetachSession()), this );
   m_tabDetachSession->plug(m_tabPopupMenu);

   m_tabPopupMenu->insertItem( i18n("&Rename Session..."), this,
                         SLOT(slotTabRenameSession()) );
   m_tabPopupMenu->insertSeparator();

  m_tabMonitorActivity = new KToggleAction ( i18n( "Monitor for &Activity" ),
      SmallIconSet("activity"), 0, this, SLOT( slotTabToggleMonitor() ), this );
  m_tabMonitorActivity->setCheckedState( KGuiItem( i18n( "Stop Monitoring for &Activity" ) ) );
   m_tabMonitorActivity->plug(m_tabPopupMenu);

  m_tabMonitorSilence = new KToggleAction ( i18n( "Monitor for &Silence" ),
      SmallIconSet("silence"), 0, this, SLOT( slotTabToggleMonitor() ), this );
  m_tabMonitorSilence->setCheckedState( KGuiItem( i18n( "Stop Monitoring for &Silence" ) ) );
   m_tabMonitorSilence->plug(m_tabPopupMenu);

   m_tabMasterMode = new KToggleAction ( i18n( "Send &Input to All Sessions" ), "remote", 0, this,
                                    SLOT( slotTabToggleMasterMode() ), this);
   m_tabMasterMode->plug(m_tabPopupMenu);

   m_tabPopupMenu->insertSeparator();
   m_tabPopupMenu->insertItem( SmallIconSet("colors"), i18n("Select &Tab Color..."), this, SLOT(slotTabSelectColor()) );

   m_tabPopupMenu->insertSeparator();
   m_tabPopupTabsMenu = new KPopupMenu( m_tabPopupMenu );
   m_tabPopupMenu->insertItem( i18n("Switch to Tab" ), m_tabPopupTabsMenu );
   connect( m_tabPopupTabsMenu, SIGNAL( activated ( int ) ),
            SLOT( activateSession( int ) ) );

   m_tabPopupMenu->insertSeparator();
   m_tabPopupMenu->insertItem( SmallIcon("fileclose"), i18n("C&lose Session"), this,
                          SLOT(slotTabCloseSession()) );

   if (m_options) {
      // Fill tab bar context menu
      m_tabbarPopupMenu = new KPopupMenu( this );
      KAcceleratorManager::manage( m_tabbarPopupMenu );
      selectTabbar->plug(m_tabbarPopupMenu);

      KSelectAction *viewOptions = new KSelectAction(this);
      viewOptions->setText(i18n("Tab &Options"));
      QStringList options;
      options << i18n("&Text && Icons") << i18n("Text &Only") << i18n("&Icons Only");
      viewOptions->setItems(options);
      viewOptions->setCurrentItem(m_tabViewMode);
      viewOptions->plug(m_tabbarPopupMenu);
      connect(viewOptions, SIGNAL(activated(int)), this, SLOT(slotTabSetViewOptions(int)));
      slotTabSetViewOptions(m_tabViewMode);

      KToggleAction *dynamicTabHideOption = new KToggleAction ( i18n( "&Dynamic Hide" ), 0, this,
                                       SLOT( slotTabbarToggleDynamicHide() ), this);
      dynamicTabHideOption->setChecked(b_dynamicTabHide);
      dynamicTabHideOption->plug(m_tabbarPopupMenu);

      KToggleAction *m_autoResizeTabs = new KToggleAction( i18n("&Auto Resize Tabs"),
                 0, this, SLOT( slotToggleAutoResizeTabs() ), this);
      m_autoResizeTabs->setChecked(b_autoResizeTabs);
      m_autoResizeTabs->plug(m_tabbarPopupMenu);
    }
}

// Called via menu
void Konsole::slotSetEncoding()
{
  if (!se) return;

  QTextCodec * qtc;
  if (selectSetEncoding->currentItem() == 0)
  {
    qtc = QTextCodec::codecForLocale();
  }
  else
  {
    bool found;
    QString enc = KGlobal::charsets()->encodingForName(selectSetEncoding->currentText());
    qtc = KGlobal::charsets()->codecForName(enc, found);

    // BR114535 : Remove jis7 due to infinite loop.
    if ( enc == "jis7" ) {
      kdWarning()<<"Encoding Japanese (jis7) currently does not work!  BR114535"<<endl;
      qtc = QTextCodec::codecForLocale();
      selectSetEncoding->setCurrentItem( 0 );
    }

    if(!found)
    {
      kdWarning() << "Codec " << selectSetEncoding->currentText() << " not found!  Using default..." << endl;
      qtc = QTextCodec::codecForLocale();
      selectSetEncoding->setCurrentItem( 0 );
    }
  }

  se->setEncodingNo(selectSetEncoding->currentItem());
  se->getEmulation()->setCodec(qtc);
}

void Konsole::makeTabWidget()
{
  tabwidget = new KTabWidget(this);
  tabwidget->setTabReorderingEnabled(true);
  tabwidget->setAutomaticResizeTabs( b_autoResizeTabs );
  tabwidget->setTabCloseActivatePrevious( true );

  if (n_tabbar==TabTop)
    tabwidget->setTabPosition(QTabWidget::Top);
  else
    tabwidget->setTabPosition(QTabWidget::Bottom);

  KAcceleratorManager::setNoAccel( tabwidget );

  connect(tabwidget, SIGNAL(movedTab(int,int)), SLOT(slotMovedTab(int,int)));
  connect(tabwidget, SIGNAL(mouseDoubleClick(QWidget*)), SLOT(slotRenameSession()));
  connect(tabwidget, SIGNAL(currentChanged(QWidget*)), SLOT(activateSession(QWidget*)));
  connect( tabwidget, SIGNAL(contextMenu(QWidget*, const QPoint &)),
           SLOT(slotTabContextMenu(QWidget*, const QPoint &)));
  connect( tabwidget, SIGNAL(contextMenu(const QPoint &)),
           SLOT(slotTabbarContextMenu(const QPoint &)));

  if (kapp->authorize("shell_access")) {
    connect(tabwidget, SIGNAL(mouseDoubleClick()), SLOT(newSession()));

    m_newSessionButton = new QToolButton( tabwidget );
    QToolTip::add(m_newSessionButton,i18n("Click for new standard session\nClick and hold for session menu"));
    m_newSessionButton->setIconSet( SmallIcon( "tab_new" ) );
    m_newSessionButton->adjustSize();
    m_newSessionButton->setPopup( m_tabbarSessionsCommands );
    connect(m_newSessionButton, SIGNAL(clicked()), SLOT(newSession()));
    tabwidget->setCornerWidget( m_newSessionButton, BottomLeft );
    m_newSessionButton->installEventFilter(this);

    m_removeSessionButton = new QToolButton( tabwidget );
    QToolTip::add(m_removeSessionButton,i18n("Close the current session"));
    m_removeSessionButton->setIconSet( SmallIconSet( "tab_remove" ) );
    m_removeSessionButton->adjustSize();
    m_removeSessionButton->setEnabled(false);
    connect(m_removeSessionButton, SIGNAL(clicked()), SLOT(confirmCloseCurrentSession()));
    tabwidget->setCornerWidget( m_removeSessionButton, BottomRight );

  }
}

bool Konsole::eventFilter( QObject *o, QEvent *ev )
{
  if (o == m_newSessionButton)
  {
    // Popup the menu when the left mousebutton is pressed and the mouse
    // is moved by a small distance.
    if (ev->type() == QEvent::MouseButtonPress)
    {
      QMouseEvent* mev = static_cast<QMouseEvent*>(ev);
      m_newSessionButtonMousePressPos = mev->pos();
    }
    else if (ev->type() == QEvent::MouseMove)
    {
      QMouseEvent* mev = static_cast<QMouseEvent*>(ev);
      if ((mev->pos() - m_newSessionButtonMousePressPos).manhattanLength()
            > KGlobalSettings::dndEventDelay())
      {
        m_newSessionButton->openPopup();
        return true;
      }
    }
    else if (ev->type() == QEvent::ContextMenu)
    {
      QMouseEvent* mev = static_cast<QMouseEvent*>(ev);
      slotTabbarContextMenu(mev->globalPos());
      return true;
    }
  }
  return KMainWindow::eventFilter(o, ev);
}

void Konsole::makeBasicGUI()
{
  if (kapp->authorize("shell_access")) {
    m_tabbarSessionsCommands = new KPopupMenu( this );
    KAcceleratorManager::manage( m_tabbarSessionsCommands );
    connect(m_tabbarSessionsCommands, SIGNAL(activated(int)), SLOT(newSessionTabbar(int)));
  }

  m_session = new KPopupMenu(this);
  KAcceleratorManager::manage( m_session );
  m_edit = new KPopupMenu(this);
  KAcceleratorManager::manage( m_edit );
  m_view = new KPopupMenu(this);
  KAcceleratorManager::manage( m_view );
  if (kapp->authorizeKAction("bookmarks"))
  {
    bookmarkHandler = new KonsoleBookmarkHandler( this, true );
    m_bookmarks = bookmarkHandler->menu();
    // call manually to disable accelerator c-b for add-bookmark initially.
    bookmarks_menu_check();
  }

  if (kapp->authorizeKAction("settings")) {
     m_options = new KPopupMenu(this);
     KAcceleratorManager::manage( m_options );
  }

  if (kapp->authorizeKAction("help"))
     m_help = helpMenu(0, false);

  if (kapp->authorizeKAction("konsole_rmb")) {
     m_rightButton = new KPopupMenu(this);
     KAcceleratorManager::manage( m_rightButton );
  }

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

  m_detachSession = new KAction(i18n("&Detach Session"), SmallIconSet("tab_breakoff"), 0, this,
                                SLOT(slotDetachSession()), m_shortcuts, "detach_session");
  m_detachSession->setEnabled(false);

  m_renameSession = new KAction(i18n("&Rename Session..."), Qt::CTRL+Qt::ALT+Qt::Key_S, this,
                                SLOT(slotRenameSession()), m_shortcuts, "rename_session");

  if (kapp->authorizeKAction("zmodem_upload"))
    m_zmodemUpload = new KAction( i18n( "&ZModem Upload..." ),
                                  Qt::CTRL+Qt::ALT+Qt::Key_U, this,
                                  SLOT( slotZModemUpload() ),
                                  m_shortcuts, "zmodem_upload" );

  monitorActivity = new KToggleAction ( i18n( "Monitor for &Activity" ),
      SmallIconSet("activity"), 0, this,
      SLOT( slotToggleMonitor() ), m_shortcuts, "monitor_activity" );
  monitorActivity->setCheckedState( KGuiItem( i18n( "Stop Monitoring for &Activity" ) ) );

  monitorSilence = new KToggleAction ( i18n( "Monitor for &Silence" ),
      SmallIconSet("silence"), 0, this,
      SLOT( slotToggleMonitor() ), m_shortcuts, "monitor_silence" );
  monitorSilence->setCheckedState( KGuiItem( i18n( "Stop Monitoring for &Silence" ) ) );

  masterMode = new KToggleAction ( i18n( "Send &Input to All Sessions" ), "remote", 0, this,
                                   SLOT( slotToggleMasterMode() ), m_shortcuts, "send_input_to_all_sessions" );

  showMenubar = new KToggleAction ( i18n( "Show &Menubar" ), "showmenu", 0, this,
                                    SLOT( slotToggleMenubar() ), m_shortcuts, "show_menubar" );
  showMenubar->setCheckedState( KGuiItem( i18n("Hide &Menubar"), "showmenu", QString::null, QString::null ) );

  m_fullscreen = KStdAction::fullScreen(0, 0, m_shortcuts, this );
  connect( m_fullscreen,SIGNAL(toggled(bool)), this,SLOT(updateFullScreen(bool)));
  m_fullscreen->setChecked(b_fullscreen);

  m_saveProfile = new KAction( i18n( "Save Sessions &Profile..." ), SmallIconSet("filesaveas"), 0, this,
                         SLOT( slotSaveSessionsProfile() ), m_shortcuts, "save_sessions_profile" );

  //help menu
  if (m_help)
     m_help->setAccel(QKeySequence(),m_help->idAt(0));
     // Don't steal F1 (handbook) accel (esp. since it not visible in
     // "Configure Shortcuts").

  m_closeSession = new KAction(i18n("C&lose Session"), "fileclose", 0, this,
                               SLOT(confirmCloseCurrentSession()), m_shortcuts, "close_session");
  m_print = new KAction(i18n("&Print Screen..."), "fileprint", 0, this, SLOT( slotPrint() ), m_shortcuts, "file_print");
  m_quit = new KAction(i18n("&Quit"), "exit", 0, this, SLOT( close() ), m_shortcuts, "file_quit");

  KShortcut shortcut(Qt::CTRL+Qt::ALT+Qt::Key_N);
  shortcut.append(KShortcut(Qt::CTRL+Qt::SHIFT+Qt::Key_N));
  new KAction(i18n("New Session"), shortcut, this, SLOT(newSession()), m_shortcuts, "new_session");
  new KAction(i18n("Activate Menu"), Qt::CTRL+Qt::ALT+Qt::Key_M, this, SLOT(activateMenu()), m_shortcuts, "activate_menu");
  new KAction(i18n("List Sessions"), 0, this, SLOT(listSessions()), m_shortcuts, "list_sessions");

  m_moveSessionLeft = new KAction(i18n("&Move Session Left"), QApplication::reverseLayout() ? "forward" : "back",
                                        QApplication::reverseLayout() ? Qt::CTRL+Qt::SHIFT+Qt::Key_Right : Qt::CTRL+Qt::SHIFT+Qt::Key_Left, this,
                                        SLOT(moveSessionLeft()), m_shortcuts, "move_session_left");
  m_moveSessionRight = new KAction(i18n("M&ove Session Right"), QApplication::reverseLayout() ? "back" : "forward",
                                        QApplication::reverseLayout() ? Qt::CTRL+Qt::SHIFT+Qt::Key_Left : Qt::CTRL+Qt::SHIFT+Qt::Key_Right, this,
                                        SLOT(moveSessionRight()), m_shortcuts, "move_session_right");

  new KAction(i18n("Go to Previous Session"), QApplication::reverseLayout() ? Qt::SHIFT+Qt::Key_Right : Qt::SHIFT+Qt::Key_Left,
              this, SLOT(prevSession()), m_shortcuts, "previous_session");
  new KAction(i18n("Go to Next Session"), QApplication::reverseLayout() ? Qt::SHIFT+Qt::Key_Left : Qt::SHIFT+Qt::Key_Right,
              this, SLOT(nextSession()), m_shortcuts, "next_session");

  for (int i=1;i<13;i++) { // Due to 12 function keys?
     new KAction(i18n("Switch to Session %1").arg(i), 0, this, SLOT(switchToSession()), m_shortcuts, QString().sprintf("switch_to_session_%02d", i).latin1());
  }

  new KAction(i18n("Enlarge Font"), 0, this, SLOT(biggerFont()), m_shortcuts, "bigger_font");
  new KAction(i18n("Shrink Font"), 0, this, SLOT(smallerFont()), m_shortcuts, "smaller_font");

  new KAction(i18n("Toggle Bidi"), Qt::CTRL+Qt::ALT+Qt::Key_B, this, SLOT(toggleBidi()), m_shortcuts, "toggle_bidi");

  // Should we load all *.desktop files now?  Required for Session shortcuts.
  if ( KConfigGroup(KGlobal::config(), "General").readBoolEntry("SessionShortcutsEnabled", false) ) {
    b_sessionShortcutsEnabled = true;
    loadSessionCommands();
    loadScreenSessions();
  }
  m_shortcuts->readShortcutSettings();

  m_sessionList = new KPopupMenu(this);
  KAcceleratorManager::manage( m_sessionList );
  connect(m_sessionList, SIGNAL(activated(int)), SLOT(activateSession(int)));
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
   if(kapp->sessionSaving())
     // saving session - do not even think about doing any kind of cleanup here
       return true;

   if (sessions.count() == 0)
       return true;

   if ( b_warnQuit)
   {
        if(sessions.count()>1) {
	    switch (
		KMessageBox::warningYesNoCancel(
	    	    this,
            	    i18n( "You have open sessions (besides the current one). "
                	  "These will be killed if you continue.\n"
                	  "Are you sure you want to quit?" ),
	    	    i18n("Really Quit?"),
	    	    KStdGuiItem::quit(),
	    	    KGuiItem(i18n("C&lose Session"),"fileclose")
		)
	    ) {
		case KMessageBox::Yes :
		break;
		case KMessageBox::No :
		    { closeCurrentSession();
		        return false;
		    }
		break;
		case KMessageBox::Cancel : return false;
	    }
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
             KStdGuiItem::close());
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
    notifySize(columns, lines);  // set menu items
  }
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void Konsole::configureRequest(TEWidget* _te, int state, int x, int y)
{
   if (!m_menuCreated)
      makeGUI();
  KPopupMenu *menu = (state & ControlButton) ? m_session : m_rightButton;
  if (menu)
     menu->popup(_te->mapToGlobal(QPoint(x,y)));
}

void Konsole::slotTabContextMenu(QWidget* _te, const QPoint & pos)
{
   if (!m_menuCreated)
      makeGUI();

  m_contextMenuSession = sessions.at( tabwidget->indexOf( _te ) );

  m_tabDetachSession->setEnabled( tabwidget->count()>1 );

  m_tabMonitorActivity->setChecked( m_contextMenuSession->isMonitorActivity() );
  m_tabMonitorSilence->setChecked( m_contextMenuSession->isMonitorSilence() );
  m_tabMasterMode->setChecked( m_contextMenuSession->isMasterMode() );

  m_tabPopupTabsMenu->clear();
  int counter=0;
  for (TESession *ses = sessions.first(); ses; ses = sessions.next()) {
    QString title=ses->Title();
    m_tabPopupTabsMenu->insertItem(SmallIcon(ses->IconName()),title.replace('&',"&&"),counter++);
  }

  m_tabPopupMenu->popup( pos );
}

void Konsole::slotTabDetachSession() {
  detachSession( m_contextMenuSession );
}

void Konsole::slotTabRenameSession() {
  renameSession(m_contextMenuSession);
}

void Konsole::slotTabSelectColor()
{
  QColor color = tabwidget->tabColor( m_contextMenuSession->widget() );
  int result = KColorDialog::getColor( color );

  if ( result == KColorDialog::Accepted )
    tabwidget->setTabColor(m_contextMenuSession->widget(), color);
}

void Konsole::slotTabToggleMonitor()
{
  m_contextMenuSession->setMonitorActivity( m_tabMonitorActivity->isChecked() );
  m_contextMenuSession->setMonitorSilence( m_tabMonitorSilence->isChecked() );
  notifySessionState( m_contextMenuSession, NOTIFYNORMAL );
  if (m_contextMenuSession==se) {
    monitorActivity->setChecked( m_tabMonitorActivity->isChecked() );
    monitorSilence->setChecked( m_tabMonitorSilence->isChecked() );
  }
}

void Konsole::slotTabToggleMasterMode()
{
  setMasterMode( m_tabMasterMode->isChecked(), m_contextMenuSession );
}

void Konsole::slotTabCloseSession()
{
  confirmCloseCurrentSession(m_contextMenuSession);
}

void Konsole::slotTabbarContextMenu(const QPoint & pos)
{
   if (!m_menuCreated)
      makeGUI();

  if ( m_tabbarPopupMenu ) m_tabbarPopupMenu->popup( pos );
}

void Konsole::slotTabSetViewOptions(int mode)
{
  m_tabViewMode = TabViewModes(mode);

  for(int i = 0; i < tabwidget->count(); i++) {

    QWidget *page = tabwidget->page(i);
    QIconSet icon = iconSetForSession(sessions.at(i));
    QString title;
    if (b_matchTabWinTitle)
      title = sessions.at(i)->fullTitle();
    else
      title = sessions.at(i)->Title();

    title=title.replace('&',"&&");
    switch(mode) {
      case ShowIconAndText:
        tabwidget->changeTab(page, icon, title);
        break;
      case ShowTextOnly:
        tabwidget->changeTab(page, QIconSet(), title);
        break;
      case ShowIconOnly:
        tabwidget->changeTab(page, icon, QString::null);
        break;
    }
  }
}

void Konsole::slotToggleAutoResizeTabs()
{
  b_autoResizeTabs = !b_autoResizeTabs;

  tabwidget->setAutomaticResizeTabs( b_autoResizeTabs );
}

void Konsole::slotTabbarToggleDynamicHide()
{
  b_dynamicTabHide=!b_dynamicTabHide;
  if (b_dynamicTabHide && tabwidget->count()==1)
    tabwidget->setTabBarHidden(true);
  else
    tabwidget->setTabBarHidden(false);
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

  if (config != KGlobal::config())
  {
     // called by the session manager
     config->writeEntry("numSes",sessions.count());
     sessions.first();
     while(counter < sessions.count())
     {
        key = QString("Title%1").arg(counter);

        config->writeEntry(key, sessions.current()->Title());
        key = QString("Schema%1").arg(counter);
        config->writeEntry(key, colors->find( sessions.current()->schemaNo() )->relPath());
	key = QString("Encoding%1").arg(counter);
	config->writeEntry(key, sessions.current()->encodingNo());
        key = QString("Args%1").arg(counter);
        config->writeEntry(key, sessions.current()->getArgs());
        key = QString("Pgm%1").arg(counter);
        config->writeEntry(key, sessions.current()->getPgm());
        key = QString("SessionFont%1").arg(counter);
        config->writeEntry(key, (sessions.current()->widget())->getVTFont());
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
        key = QString("TabColor%1").arg(counter);
        config->writeEntry(key, tabwidget->tabColor((sessions.current())->widget()));
        key = QString("History%1").arg(counter);
        config->writeEntry(key, sessions.current()->history().getSize());
        key = QString("HistoryEnabled%1").arg(counter);
        config->writeEntry(key, sessions.current()->history().isOn());

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
  else
  {
     config->setDesktopGroup();
     config->writeEntry("TabColor", tabwidget->tabColor(se->widget()));
  }
  config->writeEntry("Fullscreen",b_fullscreen);
  config->writeEntry("defaultfont", (se->widget())->getVTFont());
  s_kconfigSchema = colors->find( se->schemaNo() )->relPath();
  config->writeEntry("schema",s_kconfigSchema);
  config->writeEntry("scrollbar",n_scroll);
  config->writeEntry("tabbar",n_tabbar);
  config->writeEntry("bellmode",n_bell);
  config->writeEntry("keytab",KeyTrans::find(n_defaultKeytab)->id());
  config->writeEntry("ActiveSession", active);
  config->writeEntry("DefaultSession", m_defaultSessionFilename);
  config->writeEntry("TabViewMode", int(m_tabViewMode));
  config->writeEntry("DynamicTabHide", b_dynamicTabHide);
  config->writeEntry("AutoResizeTabs", b_autoResizeTabs);

  if (selectSetEncoding)
  {
    QString encoding = KGlobal::charsets()->encodingForName(selectSetEncoding->currentText());
    config->writeEntry("EncodingName", encoding);
  } else {    // This will not always work (ie 'winsami' saves as 'ws2')
    if (se) config->writeEntry("EncodingName", se->encoding());
  }

  if (se) {
    config->writeEntry("history", se->history().getSize());
    config->writeEntry("historyenabled", b_histEnabled);
  }

  config->writeEntry("class",name());
  if (config != KGlobal::config())
  {
      saveMainWindowSettings(config);
  }

  if (!s_workDir.isEmpty())
    config->writePathEntry("workdir", s_workDir);

  // Set the new default font
  defaultFont = se->widget()->getVTFont();
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

   if (config==KGlobal::config())
   {
     config->setDesktopGroup();
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
     b_matchTabWinTitle = config->readBoolEntry("MatchTabWinTitle",false);
     config->setGroup("UTMP");
     b_addToUtmp = config->readBoolEntry("AddToUtmp",true);
     config->setDesktopGroup();

     // Do not set a default value; this allows the System-wide Scheme
     // to set the tab text color.
     m_tabColor = config->readColorEntry("TabColor");
   }

   if (!globalConfigOnly)
   {
      n_defaultKeytab=KeyTrans::find(config->readEntry("keytab","default"))->numb(); // act. the keytab for this session
      b_fullscreen = config->readBoolEntry("Fullscreen",false);
      n_scroll   = QMIN(config->readUnsignedNumEntry("scrollbar",TEWidget::SCRRIGHT),2);
      n_tabbar   = QMIN(config->readUnsignedNumEntry("tabbar",TabBottom),2);
      n_bell = QMIN(config->readUnsignedNumEntry("bellmode",TEWidget::BELLSYSTEM),3);

      // Options that should be applied to all sessions /////////////

      // (1) set menu items and Konsole members

      QFont tmpFont = KGlobalSettings::fixedFont();
      defaultFont = config->readFontEntry("defaultfont", &tmpFont);

      //set the schema
      s_kconfigSchema=config->readEntry("schema");
      ColorSchema* sch = colors->find(schema.isEmpty() ? s_kconfigSchema : schema);
      if (!sch)
      {
         sch = (ColorSchema*)colors->at(0);  //the default one
         kdWarning() << "Could not find schema named " <<s_kconfigSchema<<"; using "<<sch->relPath()<<endl;
         s_kconfigSchema = sch->relPath();
      }
      if (sch->hasSchemaFileChanged()) sch->rereadSchemaFile();
      s_schema = sch->relPath();
      curr_schema = sch->numb();
      pmPath = sch->imagePath();

      if (te) {
        if (sch->useTransparency())
        {
           if (!rootxpms[te])
             rootxpms.insert( te, new KRootPixmap(te) );
           rootxpms[te]->setFadeEffect(sch->tr_x(), QColor(sch->tr_r(), sch->tr_g(), sch->tr_b()));
        }
        else
        {
           if (rootxpms[te]) {
             delete rootxpms[te];
             rootxpms.remove(te);
           }
           pixmap_menu_activated(sch->alignment());
        }

        te->setColorTable(sch->table()); //FIXME: set twice here to work around a bug
        te->setColorTable(sch->table());
        te->setScrollbarLocation(n_scroll);
        te->setBellMode(n_bell);
      }

      // History
      m_histSize = config->readNumEntry("history",DEFAULT_HISTORY_SIZE);
      b_histEnabled = config->readBoolEntry("historyenabled",true);

      // Tab View Mode
      m_tabViewMode = TabViewModes(config->readNumEntry("TabViewMode", ShowIconAndText));
      b_dynamicTabHide = config->readBoolEntry("DynamicTabHide", false);
      b_autoResizeTabs = config->readBoolEntry("AutoResizeTabs", false);

      s_encodingName = config->readEntry( "EncodingName", "" ).lower();

      // The scrollbar location only needs to be changed when the given
      // profile scrollbar entry differs from the konsolerc scrollbar entry.
      QPtrList<TEWidget> tes = activeTEs();
      for (TEWidget *_te = tes.first(); _te; _te = tes.next()) {
        if (_te->getScrollbarLocation() != n_scroll) 
           _te->setScrollbarLocation(n_scroll);
      }
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
//      setFont();
      notifySize(te->Columns(), te->Lines());
      selectTabbar->setCurrentItem(n_tabbar);
      showMenubar->setChecked(!menuBar()->isHidden());
      selectScrollbar->setCurrentItem(n_scroll);
      selectBell->setCurrentItem(n_bell);
      selectSetEncoding->setCurrentItem( se->encodingNo() );
      updateRMBMenu();
   }
   updateKeytabMenu();
   tabwidget->setAutomaticResizeTabs( b_autoResizeTabs );
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

void Konsole::checkBitmapFonts()
{
    {
        QFont f;
        f.setRawName("-misc-console-medium-r-normal--16-160-72-72-c-80-iso10646-1");
        QFontInfo fi( f );
        if ( !fi.exactMatch() )
            b_installBitmapFonts = true;
    }
    {
        QFont f;
        f.setRawName("-misc-fixed-medium-r-normal--15-140-75-75-c-90-iso10646-1");
        QFontInfo fi( f );
        if ( !fi.exactMatch() )
            b_installBitmapFonts = true;
    }
}

// In KDE 3.5, Konsole only allows the user to pick a font via
// KFontDialog.  This causes problems with the bdf/pcf files
// distributed with Konsole (console8x16 and 9x15).
void Konsole::slotInstallBitmapFonts()
{
    if ( !b_installBitmapFonts )
        return;

    QStringList sl_installFonts;
    {
        QFont f;
        f.setRawName("-misc-console-medium-r-normal--16-160-72-72-c-80-iso10646-1");
        QFontInfo fi( f );
        if ( !fi.exactMatch() )
            sl_installFonts << "console8x16.pcf.gz";
    }
    {
        QFont f;
        f.setRawName("-misc-fixed-medium-r-normal--15-140-75-75-c-90-iso10646-1");
        QFontInfo fi( f );
        if ( !fi.exactMatch() )
            sl_installFonts << "9x15.pcf.gz";
    }

    if ( !sl_installFonts.isEmpty() )
    {
        if ( KMessageBox::questionYesNoList( this,
            i18n( "If you want to use the bitmap fonts distributed with Konsole, they must be installed.  After installation, you must restart Konsole to use them.  Do you want to install the fonts listed below into fonts:/Personal?" ),
            sl_installFonts,
            i18n( "Install Bitmap Fonts?" ),
            KGuiItem( i18n("&Install" ) ),
            i18n("Do Not Install") ) == KMessageBox::Yes )
        {
            for ( QStringList::iterator it = sl_installFonts.begin();
                  it != sl_installFonts.end(); ++it )
            {
                QString sf = "fonts/" + *it;
                if ( KIO::NetAccess::copy( locate( "appdata", sf ),
                                            "fonts:/Personal/", 0 ) )
                {
                    b_installBitmapFonts = false;
                    // TODO: Remove the Install from the Fonts sub-menu.
                } else {
                    KMessageBox::error( this, i18n( "Could not install %1 into fonts:/Personal/" ).arg( *it ), i18n( "Error" ) );
                }
            }
        }
    }

}

void Konsole::slotSelectFont() {
   if ( !se ) return;

   QFont font = se->widget()->getVTFont();
   if ( KFontDialog::getFont( font, true ) != QDialog::Accepted )
      return;

   se->widget()->setVTFont( font );
//  activateSession(); // activates the current
}

void Konsole::schema_menu_activated(int item)
{
  if (!se) return;
  setSchema(item);
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

void Konsole::createSessionTab(TEWidget *widget, const QIconSet &iconSet,
                               const QString &text, int index)
{
  switch(m_tabViewMode) {
  case ShowIconAndText:
    tabwidget->insertTab(widget, iconSet, text, index);
    break;
  case ShowTextOnly:
    tabwidget->insertTab(widget, QIconSet(), text, index);
    break;
  case ShowIconOnly:
    tabwidget->insertTab(widget, iconSet, QString::null, index);
    break;
  }
  if ( m_tabColor.isValid() )
    tabwidget->setTabColor(widget, m_tabColor);
}

QIconSet Konsole::iconSetForSession(TESession *session) const
{
  if (m_tabViewMode == ShowTextOnly)
    return QIconSet();
  return SmallIconSet(session->isMasterMode() ? "remote" : session->IconName());
}


/**
    Toggle the Tabbar visibility
 */
void Konsole::slotSelectTabbar() {
   if (m_menuCreated)
      n_tabbar = selectTabbar->currentItem();

   if ( n_tabbar == TabNone ) {     // Hide tabbar
      tabwidget->setTabBarHidden( true );
   } else {
      if ( tabwidget->isTabBarHidden() )
         tabwidget->setTabBarHidden( false );
      if ( n_tabbar == TabTop )
         tabwidget->setTabPosition( QTabWidget::Top );
      else
         tabwidget->setTabPosition( QTabWidget::Bottom );
   }

/* FIXME: Still necessary ? */
      QPtrDictIterator<KRootPixmap> it(rootxpms);
      for (;it.current();++it)
        it.current()->repaint(true);

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

  QStringList ctrlKeys;

  for ( uint i = 0; i < m_shortcuts->count(); i++ )
  {
    KShortcut shortcut = (m_shortcuts->action( i ))->shortcut();
    for( uint j = 0; j < shortcut.count(); j++)
    {
      const KKey &key = shortcut.seq(j).key(0); // First Key of KeySequence
      if (key.modFlags() == KKey::CTRL)
        ctrlKeys += key.toString();
    }

    // Are there any shortcuts for Session Menu entries?
    if ( !b_sessionShortcutsEnabled &&
         m_shortcuts->action( i )->shortcut().count() &&
         QString(m_shortcuts->action( i )->name()).startsWith("SSC_") ) {
      b_sessionShortcutsEnabled = true;
      KConfigGroup group(KGlobal::config(), "General");
      group.writeEntry("SessionShortcutsEnabled", true);
    }
  }

  if (!ctrlKeys.isEmpty())
  {
    ctrlKeys.sort();
    KMessageBox::informationList( this, i18n( "You have chosen one or more Ctrl+<key> combinations to be used as shortcuts. "
                                               "As a result these key combinations will no longer be passed to the command shell "
                                               "or to applications that run inside Konsole. "
                                               "This can have the unintended consequence that functionality that would otherwise be "
                                               "bound to these key combinations is no longer accessible."
                                               "\n\n"
                                               "You may wish to reconsider your choice of keys and use Alt+Ctrl+<key> or Ctrl+Shift+<key> instead."
                                               "\n\n"
                                               "You are currently using the following Ctrl+<key> combinations:" ),
                                               ctrlKeys,
                                               i18n( "Choice of Shortcut Keys" ), 0);
  }
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

  // The .desktop files may have been changed by user...
  b_sessionShortcutsMapped = false;

  // Mappings may have to be changed...get a fresh mapper.
  disconnect( sessionNumberMapper, SIGNAL( mapped( int ) ),
          this, SLOT( newSessionTabbar( int ) ) );
  delete sessionNumberMapper;
  sessionNumberMapper = new QSignalMapper( this );
  connect( sessionNumberMapper, SIGNAL( mapped( int ) ),
          this, SLOT( newSessionTabbar( int ) ) );

  sl_sessionShortCuts.clear();
  buildSessionMenus();

  // FIXME: Should be a better way to traverse KActionCollection
  uint count = m_shortcuts->count();
  for ( uint i = 0; i < count; i++ )
  {
    KAction* action = m_shortcuts->action( i );
    bool b_foundSession = false;
    if ( QString(action->name()).startsWith("SSC_") ) {
      QString name = QString(action->name());

      // Check to see if shortcut's session has been loaded.
      for ( QStringList::Iterator it = sl_sessionShortCuts.begin(); it != sl_sessionShortCuts.end(); ++it ) {
        if ( QString::compare( *it, name ) == 0 ) {
          b_foundSession = true;
          break;
        }
      }
      if ( ! b_foundSession ) {
        action->setShortcut( KShortcut() );   // Clear shortcut
        m_shortcuts->writeShortcutSettings();
        delete action;           // Remove Action and Accel
        if ( i == 0 ) i = 0;     // Reset index
        else i--;
        count--;                 // = m_shortcuts->count();
      }
    }
  }

  m_shortcuts->readShortcutSettings();

  // User may have changed Schema->Set as default schema
  s_kconfigSchema = KGlobal::config()->readEntry("schema");
  ColorSchema* sch = colors->find(s_kconfigSchema);
  if (!sch)
  {
     sch = (ColorSchema*)colors->at(0);  //the default one
     kdWarning() << "Could not find schema named " <<s_kconfigSchema<<"; using "<<sch->relPath()<<endl;
     s_kconfigSchema = sch->relPath();
  }
  if (sch->hasSchemaFileChanged()) sch->rereadSchemaFile();
  s_schema = sch->relPath();
  curr_schema = sch->numb();
  pmPath = sch->imagePath();

  for (TESession *_se = sessions.first(); _se; _se = sessions.next()) {
     ColorSchema* s = colors->find( _se->schemaNo() );
     if (s) {
       if (s->hasSchemaFileChanged())
         s->rereadSchemaFile();
       setSchema(s,_se->widget());
     }
  }
}

// Called via emulation via session
void Konsole::changeTabTextColor( TESession* ses, int rgb )
{
    if ( !ses ) return;
    QColor color;
    color.setRgb( rgb );
    if ( !color.isValid() ) {
        kdWarning()<<" Invalid RGB color "<<rgb<<endl;
        return;
    }
    tabwidget->setTabColor( ses->widget(), color );
}

// Called from emulation
void Konsole::changeColLin(int columns, int lines)
{
  if (b_allowResize && !b_fixedSize) {
    setColLin(columns, lines);
    te->update();
  }
}

// Called from emulation
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
       setFullScreen( false );

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

void Konsole::notifySize(int columns, int lines)
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

void Konsole::updateTitle(TESession* _se)
{
  if ( !_se )
    _se = se;

  if (_se == se)
  {
    setCaption( _se->fullTitle() );
    setIconText( _se->IconText() );
  }
  tabwidget->setTabIconSet(_se->widget(), iconSetForSession(_se));
  QString icon = _se->IconName();
  KRadioAction *ra = session2action.find(_se);
  if (ra && (ra->icon() != icon))
    ra->setIcon(icon);
  if (m_tabViewMode == ShowIconOnly) 
    tabwidget->changeTab( _se->widget(), QString::null );
  else if (b_matchTabWinTitle)
    tabwidget->setTabLabel( _se->widget(), _se->fullTitle().replace('&',"&&"));
}

void Konsole::initSessionFont(QFont font) {
  te->setVTFont( font );
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

void Konsole::toggleFullScreen()
{
  setFullScreen(!b_fullscreen);
}

bool Konsole::fullScreen()
{
  return b_fullscreen;
}

void Konsole::setFullScreen(bool on)
{
  if( on )
      showFullScreen();
  else {
      if( isFullScreen()) // showNormal() may also do unminimize, unmaximize etc. :(
          showNormal();
  }
}

// don't call this directly
void Konsole::updateFullScreen( bool on )
{
  b_fullscreen = on;
  if( on )
    showFullScreen();
  else {
    if( isFullScreen()) // showNormal() may also do unminimize, unmaximize etc. :(
        showNormal();
    updateTitle(); // restore caption of window
  }
  updateRMBMenu();
  te->setFrameStyle( b_framevis && !b_fullscreen ?(QFrame::WinPanel|QFrame::Sunken):QFrame::NoFrame );
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

void Konsole::disableMasterModeConnections()
{
  QPtrListIterator<TESession> from_it(sessions);
  for (; from_it.current(); ++from_it) {
    TESession *from = from_it.current();
    if (from->isMasterMode()) {
      QPtrListIterator<TESession> to_it(sessions);
      for (; to_it.current(); ++to_it) {
        TESession *to = to_it.current();
        if (to!=from)
          disconnect(from->widget(),SIGNAL(keyPressedSignal(QKeyEvent*)),
                     to->getEmulation(),SLOT(onKeyPress(QKeyEvent*)));
      }
    }
  }
}

void Konsole::enableMasterModeConnections()
{
  QPtrListIterator<TESession> from_it(sessions);
  for (; from_it.current(); ++from_it) {
    TESession *from = from_it.current();
    if (from->isMasterMode()) {
      QPtrListIterator<TESession> to_it(sessions);
      for (; to_it.current(); ++to_it) {
        TESession *to = to_it.current();
        if (to!=from) {
          connect(from->widget(),SIGNAL(keyPressedSignal(QKeyEvent*)),
                  to->getEmulation(),SLOT(onKeyPress(QKeyEvent*)));
        }
      }
    }
    from->setListenToKeyPress(true);
  }
}

void Konsole::feedAllSessions(const QString &text)
{
  if (!te) return;
  bool oldMasterMode = se->isMasterMode();
  setMasterMode(true);
  te->emitText(text);
  if (!oldMasterMode)
    setMasterMode(false);
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

  if (URL.startsWith("file:")) {
    KURL uglyurl(URL);
    newtext=uglyurl.path();
    KRun::shellQuote(newtext);
    te->emitText("cd "+newtext+"\r");
  }
  else if (URL.contains("://", true)) {
    KURL u(URL);
    newtext = u.protocol();
    bool isSSH = (newtext == "ssh");
    if (u.port() && isSSH)
       newtext += " -p " + QString().setNum(u.port());
    if (u.hasUser())
       newtext += " -l " + u.user();

    /*
     * If we have a host, connect.
     */
    if (u.hasHost()) {
      newtext = newtext + " " + u.host();
      if (u.port() && !isSSH)
         newtext += QString(" %1").arg(u.port());
      se->setUserTitle(31,"");           // we don't know remote cwd
      te->emitText(newtext + "\r");
    }
  }
  else
    te->emitText(URL);
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
                                      m_shortcuts);
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

  createSessionTab(te, SmallIconSet(s->IconName()), newTitle);
  setSchema(s->schemaNo());
  tabwidget->setCurrentPage(tabwidget->count()-1);
  disableMasterModeConnections(); // no duplicate connections, remove old
  enableMasterModeConnections();
  if( m_removeSessionButton )
    m_removeSessionButton->setEnabled(tabwidget->count()>1);
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
  m_sessionList->clear();
  m_sessionList->insertTitle(i18n("Session List"));
  m_sessionList->setKeyboardShortcutsEnabled(true);
  for (TESession *ses = sessions.first(); ses; ses = sessions.next()) {
    QString title=ses->Title();
    m_sessionList->insertItem(SmallIcon(ses->IconName()),title.replace('&',"&&"),counter++);
  }
  m_sessionList->adjustSize();
  m_sessionList->popup(mapToGlobal(QPoint((width()/2)-(m_sessionList->width()/2),(height()/2)-(m_sessionList->height()/2))));
}

void Konsole::switchToSession()
{
  activateSession( QString( sender()->name() ).right( 2 ).toInt() -1 );
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
     se->setListenToKeyPress(true);
     notifySessionState(se,NOTIFYNORMAL);
     // Delete the session if isn't in the session list any longer.
     if (sessions.find(se) == -1)
        delete se;
  }
  if (se != s)
     se_previous = se;
  se = s;

  // Set the required schema variables for the current session
  ColorSchema* cs = colors->find( se->schemaNo() );
  if (!cs)
      cs = (ColorSchema*)colors->at(0);  //the default one
  s_schema = cs->relPath();
  curr_schema = cs->numb();
  pmPath = cs->imagePath();
  n_render = cs->alignment();

// BR 106464 temporary fix... 
//  only 2 sessions opened, 2nd session viewable, right-click on 1st tab and 
//  select 'Detach', close original Konsole window... crash
//  s is not set properly on original Konsole window
  KRadioAction *ra = session2action.find(se);
  if (!ra) {
    se=sessions.first();        // Get new/correct TESession
    ra = session2action.find(se);
  }
  ra->setChecked(true);

  QTimer::singleShot(1,this,SLOT(allowPrevNext())); // hack, hack, hack

  tabwidget->showPage( se->widget() );
  te = se->widget();
  if (m_menuCreated) {
    if (selectBell) selectBell->setCurrentItem(te->bellMode());
    updateSchemaMenu();
  }

  if (rootxpms[te])
    rootxpms[te]->start();
  notifySize(te->Columns(), te->Lines()); // set menu items
  se->setConnect(true);
  updateTitle();
  if (!m_menuCreated)
     return;

  if (selectSetEncoding) selectSetEncoding->setCurrentItem(se->encodingNo());
  updateKeytabMenu(); // act. the keytab for this session
  if (m_clearHistory) m_clearHistory->setEnabled( se->history().isOn() );
  if (m_findHistory) m_findHistory->setEnabled( se->history().isOn() );
  if (m_findNext) m_findNext->setEnabled( se->history().isOn() );
  if (m_findPrevious) m_findPrevious->setEnabled( se->history().isOn() );
  se->getEmulation()->findTextBegin();
  if (m_saveHistory) m_saveHistory->setEnabled( se->history().isOn() );
  if (monitorActivity) monitorActivity->setChecked( se->isMonitorActivity() );
  if (monitorSilence) monitorSilence->setChecked( se->isMonitorSilence() );
  masterMode->setChecked( se->isMasterMode() );
  sessions.find(se);
  uint position=sessions.at();
  if (m_moveSessionLeft) m_moveSessionLeft->setEnabled(position>0);
  if (m_moveSessionRight) m_moveSessionRight->setEnabled(position<sessions.count()-1);
}

void Konsole::slotUpdateSessionConfig(TESession *session)
{
  if (session == se)
     activateSession(se);
}

void Konsole::slotResizeSession(TESession *session, QSize size)
{
  TESession *oldSession = se;
  if (se != session)
     activateSession(session);
  setColLin(size.width(), size.height());
  activateSession(oldSession);
}

// Called by newSession and DCOP function below
void Konsole::setSessionEncoding( const QString &encoding, TESession *session )
{
    if ( encoding.isEmpty() )
        return;

    if ( !session )
        session = se;

    bool found = false;
    QString enc = KGlobal::charsets()->encodingForName(encoding);
    QTextCodec * qtc = KGlobal::charsets()->codecForName(enc, found);
    if ( !found || !qtc )
        return;

    // Encoding was found; now try to figure out which Encoding menu item
    // it corresponds to.
    int i = 0;
    bool found_encoding = false;
    QStringList encodingNames = KGlobal::charsets()->descriptiveEncodingNames();
    QStringList::ConstIterator it = encodingNames.begin();
    QString t_encoding = encoding.lower();

    while ( it != encodingNames.end() && !found_encoding )
    {
      if ( QString::compare( KGlobal::charsets()->encodingForName(*it), 
                             t_encoding ) == 0 ) {
         found_encoding = true;
      }
      i++; it++;
    }

    // BR114535 : Remove jis7 due to infinite loop.
    if ( enc == "jis7" ) {
      kdWarning()<<"Encoding Japanese (jis7) currently does not work!  BR114535"<<endl;
      return;
    }

    if ( found_encoding )
    {
      session->setEncodingNo( i );
      session->getEmulation()->setCodec(qtc);
      if (se == session)
        activateSession(se);
    }
}

// Called via DCOP only
void Konsole::slotSetSessionEncoding(TESession *session, const QString &encoding)
{
   setSessionEncoding( encoding, session );
}

void Konsole::slotGetSessionSchema(TESession *session, QString &schema)
{
  int no = session->schemaNo();
  ColorSchema* s = colors->find( no );
  schema = s->relPath();
}

void Konsole::slotSetSessionSchema(TESession *session, const QString &schema)
{
  ColorSchema* s = colors->find( schema );
  setSchema(s, session->widget());
}

void Konsole::allowPrevNext()
{
  if (!se) return;
  notifySessionState(se,NOTIFYNORMAL);
}

KSimpleConfig *Konsole::defaultSession()
{
  if (!m_defaultSession) {
    KConfig * config = KGlobal::config();
    config->setDesktopGroup();
    setDefaultSession(config->readEntry("DefaultSession","shell.desktop"));
  }
  return m_defaultSession;
}

void Konsole::setDefaultSession(const QString &filename)
{
  delete m_defaultSession;
  m_defaultSession = new KSimpleConfig(locate("appdata", filename), true /* read only */);
  m_defaultSession->setDesktopGroup();
  b_showstartuptip = m_defaultSession->readBoolEntry("Tips", true);

  m_defaultSessionFilename=filename;
}

void Konsole::newSession(const QString &pgm, const QStrList &args, const QString &term, const QString &icon, const QString &title, const QString &cwd)
{
  KSimpleConfig *co = defaultSession();
  newSession(co, pgm, args, term, icon, title, cwd);
}

QString Konsole::newSession()
{
  KSimpleConfig *co = defaultSession();
  return newSession(co, QString::null, QStrList());
}

void Konsole::newSession(int i)
{
  if (i == SESSION_NEW_WINDOW_ID)
  {
    // TODO: "type" isn't passed properly
    Konsole* konsole = new Konsole(name(), b_histEnabled, !menubar->isHidden(), n_tabbar != TabNone, b_framevis,
                                   n_scroll != TEWidget::SCRNONE, 0, false, 0);
    konsole->newSession();
    konsole->enableFullScripting(b_fullScripting);
    konsole->enableFixedSize(b_fixedSize);
    konsole->setColLin(0,0); // Use defaults
    konsole->initFullScreen();
    konsole->show();
    return;
  }

  KSimpleConfig* co = no2command.find(i);
  if (co) {
    newSession(co);
    resetScreenSessions();
  }
}

void Konsole::newSessionTabbar(int i)
{
  if (i == SESSION_NEW_WINDOW_ID)
  {
    // TODO: "type" isn't passed properly
    Konsole* konsole = new Konsole(name(), b_histEnabled, !menubar->isHidden(), n_tabbar != TabNone, b_framevis,
                                   n_scroll != TEWidget::SCRNONE, 0, false, 0);
    konsole->newSession();
    konsole->enableFullScripting(b_fullScripting);
    konsole->enableFixedSize(b_fixedSize);
    konsole->setColLin(0,0); // Use defaults
    konsole->initFullScreen();
    konsole->show();
    return;
  }

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

QString Konsole::newSession(KSimpleConfig *co, QString program, const QStrList &args,
                            const QString &_term,const QString &_icon,
                            const QString &_title, const QString &_cwd)
{
  QString emu = "xterm";
  QString icon = "konsole";
  QString key;
  QString sch = s_kconfigSchema;
  QString txt;
  QString cwd;
  QFont font = defaultFont;
  QStrList cmdArgs;

  if (co) {
     co->setDesktopGroup();
     emu = co->readEntry("Term", emu);
     key = co->readEntry("KeyTab", key);
     sch = co->readEntry("Schema", sch);
     txt = co->readEntry("Name");
     font = co->readFontEntry("SessionFont", &font);
     icon = co->readEntry("Icon", icon);
     cwd = co->readPathEntry("Cwd");
  }

  if (!_term.isEmpty())
     emu = _term;

  if (!_icon.isEmpty())
     icon = _icon;

  if (!_title.isEmpty())
     txt = _title;

  // apply workdir only when the session config does not have a directory
  if (cwd.isEmpty())
     cwd = s_workDir;
  // bookmarks take precedence over workdir
  // however, --workdir option has precedence in the very first session
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

  if (sessions.count()==1 && n_tabbar!=TabNone)
    tabwidget->setTabBarHidden( false );

  TEWidget* te_old = te;
  te=new TEWidget(tabwidget);

  connect( te, SIGNAL(configureRequest(TEWidget*, int, int, int)),
           this, SLOT(configureRequest(TEWidget*,int,int,int)) );
  if (te_old) {
    initTEWidget(te, te_old);
  }
  else {
    readProperties(KGlobal::config(), "", true);
    te->setVTFont(font);
    te->setScrollbarLocation(n_scroll);
    te->setBellMode(n_bell);
  }

  te->setMinimumSize(150,70);

  QString sessionId="session-"+QString::number(++sessionIdCounter);
  TESession* s = new TESession(te, emu,winId(),sessionId,cwd);
  s->setProgram(QFile::encodeName(program),cmdArgs);
  s->setMonitorSilenceSeconds(monitorSilenceSeconds);
  s->enableFullScripting(b_fullScripting);
  // If you add any new signal-slot connection below, think about doing it in konsolePart too
  connect( s,SIGNAL(done(TESession*)),
           this,SLOT(doneSession(TESession*)) );
  connect( s, SIGNAL( updateTitle(TESession*) ),
           this, SLOT( updateTitle(TESession*) ) );
  connect( s, SIGNAL( notifySessionState(TESession*, int) ),
           this, SLOT( notifySessionState(TESession*, int)) );
  connect( s, SIGNAL(disableMasterModeConnections()),
           this, SLOT(disableMasterModeConnections()) );
  connect( s, SIGNAL(enableMasterModeConnections()),
           this, SLOT(enableMasterModeConnections()) );
  connect( s, SIGNAL(renameSession(TESession*,const QString&)),
           this, SLOT(slotRenameSession(TESession*, const QString&)) );
  connect( s->getEmulation(), SIGNAL(changeColumns(int)),
           this, SLOT(changeColumns(int)) );
  connect( s->getEmulation(), SIGNAL(changeColLin(int,int)),
           this, SLOT(changeColLin(int,int)) );
  connect( s->getEmulation(), SIGNAL(ImageSizeChanged(int,int)),
           this, SLOT(notifySize(int,int)));
  connect( s, SIGNAL(zmodemDetected(TESession*)),
           this, SLOT(slotZModemDetected(TESession*)));
  connect( s, SIGNAL(updateSessionConfig(TESession*)),
           this, SLOT(slotUpdateSessionConfig(TESession*)));
  connect( s, SIGNAL(resizeSession(TESession*, QSize)),
           this, SLOT(slotResizeSession(TESession*, QSize)));
  connect( s, SIGNAL(setSessionEncoding(TESession*, const QString &)),
           this, SLOT(slotSetSessionEncoding(TESession*, const QString &)));
  connect( s, SIGNAL(getSessionSchema(TESession*, QString &)),
           this, SLOT(slotGetSessionSchema(TESession*, QString &)));
  connect( s, SIGNAL(setSessionSchema(TESession*, const QString &)),
           this, SLOT(slotSetSessionSchema(TESession*, const QString &)));
  connect( s, SIGNAL(changeTabTextColor(TESession*, int)),
           this,SLOT(changeTabTextColor(TESession*, int)) );

  s->widget()->setVTFont(defaultFont);// Hack to set font again after newSession
  s->setSchemaNo(schmno);
  if (key.isEmpty())
    s->setKeymapNo(n_defaultKeytab);
  else {
    // TODO: Fixes BR77018, see BR83000.
    if (key.endsWith(".keytab"))
      key.remove(".keytab");
    s->setKeymap(key);
  }

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

  setSessionEncoding( s_encodingName, s );

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
     bool isSSH = (protocol == "ssh");
     args.append( protocol.latin1() ); /* argv[0] == command to run. */
     host = url.host();
     if (url.port() && isSSH) {
       args.append("-p");
       args.append(QCString().setNum(url.port()));
     }
     if (url.hasUser()) {
       login = url.user();
       args.append("-l");
       args.append(login.latin1());
     }
     args.append(host.latin1());
     if (url.port() && !isSSH)
       args.append(QCString().setNum(url.port()));
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

void Konsole::confirmCloseCurrentSession( TESession* _se )
{
   if ( !_se )
      _se = se;
   if (KMessageBox::warningContinueCancel(this,
     i18n("Are you sure that you want to close the current session?"),
     i18n("Close Confirmation"), KGuiItem(i18n("C&lose Session"),"tab_remove"),
     "ConfirmCloseSession")==KMessageBox::Continue)
       _se->closeSession();
}

void Konsole::closeCurrentSession()
{
  se->closeSession();
}

//FIXME: If a child dies during session swap,
//       this routine might be called before
//       session swap is completed.

void Konsole::doneSession(TESession* s)
{

  if (s == se_previous)
    se_previous = 0;

  if (se_previous)
    activateSession(se_previous);

  KRadioAction *ra = session2action.find(s);
  ra->unplug(m_view);
  tabwidget->removePage( s->widget() );
  if (rootxpms[s->widget()]) {
    delete rootxpms[s->widget()];
    rootxpms.remove(s->widget());
  }
  delete s->widget();
  if(m_removeSessionButton )
      m_removeSessionButton->setEnabled(tabwidget->count()>1);
  session2action.remove(s);
  action2session.remove(ra);
  int sessionIndex = sessions.findRef(s);
  sessions.remove(s);
  delete ra; // will the toolbar die?

  s->setConnect(false);
  delete s;

  if (s == se_previous)
    se_previous = 0;

  if (s == se)
  { // pick a new session
    se = 0;
    if (sessions.count())
    {
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
  if (sessions.count()==1) {
    m_detachSession->setEnabled(false);
    if (b_dynamicTabHide && !tabwidget->isTabBarHidden())
       tabwidget->setTabBarHidden(true);
  }
}

/*! Cycle to previous session (if any) */

void Konsole::prevSession()
{
  sessions.find(se); sessions.prev();
  if (!sessions.current()) sessions.last();
  if (sessions.current() && sessions.count() > 1)
    activateSession(sessions.current());
}

/*! Cycle to next session (if any) */

void Konsole::nextSession()
{
  sessions.find(se); sessions.next();
  if (!sessions.current()) sessions.first();
  if (sessions.current() && sessions.count() > 1)
    activateSession(sessions.current());
}

void Konsole::slotMovedTab(int from, int to)
{

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

  QColor oldcolor = tabwidget->tabColor(se->widget());
  
  tabwidget->blockSignals(true);
  tabwidget->removePage(se->widget());
  tabwidget->blockSignals(false);
  QString title = se->Title();
  createSessionTab(se->widget(), iconSetForSession(se), 
                   title.replace('&', "&&"), position-1);
  tabwidget->showPage(se->widget());
  tabwidget->setTabColor(se->widget(),oldcolor);
  
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

  QColor oldcolor = tabwidget->tabColor(se->widget());
  
  tabwidget->blockSignals(true);
  tabwidget->removePage(se->widget());
  tabwidget->blockSignals(false);
  QString title = se->Title();
  createSessionTab(se->widget(), iconSetForSession(se), 
                   title.replace('&', "&&"), position+1);
  tabwidget->showPage(se->widget());
  tabwidget->setTabColor(se->widget(),oldcolor);
  
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

void Konsole::initTabColor(QColor color)
{
  if ( color.isValid() )
    tabwidget->setTabColor( se->widget(), color );
}

void Konsole::initHistory(int lines, bool enable)
{
   // If no History#= is given in the profile, use the history
   // parameter saved in konsolerc.
   if ( lines < 0 ) lines = m_histSize;

   if ( enable && lines > 0 )
      se->setHistory( HistoryTypeBuffer( lines ) );
   else if ( enable )  // Unlimited buffer
      se->setHistory(HistoryTypeFile());
   else
      se->setHistory( HistoryTypeNone() );
}

void Konsole::slotToggleMasterMode()
{
  setMasterMode( masterMode->isChecked() );
}

void Konsole::setMasterMode(bool _state, TESession* _se)
{
  if (!_se)
    _se = se;
  if (_se->isMasterMode() == _state)
    return;

  if (_se==se)
    masterMode->setChecked( _state );

  disableMasterModeConnections();

  _se->setMasterMode( _state );

  if (_state)
    enableMasterModeConnections();

  notifySessionState(_se,NOTIFYNORMAL);
}

void Konsole::notifySessionState(TESession* session, int state)
{
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
    case NOTIFYACTIVITY: state_iconname = "activity";
                         break;
    case NOTIFYSILENCE : state_iconname = "silence";
			 break;
  }
  if (!state_iconname.isEmpty()
      && session->testAndSetStateIconName(state_iconname)
      && m_tabViewMode != ShowTextOnly) {

    QPixmap normal = KGlobal::instance()->iconLoader()->loadIcon(state_iconname,
                       KIcon::Small, 0, KIcon::DefaultState, 0L, true);
    QPixmap active = KGlobal::instance()->iconLoader()->loadIcon(state_iconname,
                       KIcon::Small, 0, KIcon::ActiveState, 0L, true);

    // make sure they are not larger than 16x16
    if (normal.width() > 16 || normal.height() > 16)
      normal.convertFromImage(normal.convertToImage().smoothScale(16,16));
    if (active.width() > 16 || active.height() > 16)
      active.convertFromImage(active.convertToImage().smoothScale(16,16));

    QIconSet iconset;
    iconset.setPixmap(normal, QIconSet::Small, QIconSet::Normal);
    iconset.setPixmap(active, QIconSet::Small, QIconSet::Active);

    tabwidget->setTabIconSet(session->widget(), iconset);
  }
}

// --| Session support |-------------------------------------------------------

void Konsole::buildSessionMenus()
{
   m_session->clear();
   if (m_tabbarSessionsCommands)
      m_tabbarSessionsCommands->clear();

   loadSessionCommands();
   loadScreenSessions();

   createSessionMenus();

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
  const int defaultId = SESSION_NEW_SHELL_ID; // The id of the 'new' item.
  int index = menu->indexOf(defaultId);
  int count = menu->count();
  if (index >= 0)
  {
     index++; // Skip New Window
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
  if (path.isEmpty())
    co = new KSimpleConfig(locate("appdata", "shell.desktop"), true /* read only */);
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
  exec = KShell::tildeExpand(exec);
  QString pexec = KGlobal::dirs()->findExe(exec);

  if (typ.isEmpty() || txt.isEmpty() || typ != "KonsoleApplication"
      || ( !exec.isEmpty() && pexec.isEmpty() ) )
  {
    if (!path.isEmpty())
       delete co;
    kdWarning()<<"Unable to use "<<path.latin1()<<endl;
    return; // ignore
  }

  no2command.insert(++cmd_serial,co);

  // Add shortcuts only once and not for 'New Shell'.
  if ( ( b_sessionShortcutsMapped == true ) || ( cmd_serial == SESSION_NEW_SHELL_ID ) ) return;

  // Add an empty shortcut for each Session.
  QString comment = co->readEntry("Comment");
  if (comment.isEmpty())
    comment=txt.prepend(i18n("New "));

  QString name = comment;
  name.prepend("SSC_");  // Allows easy searching for Session ShortCuts
  name.replace(" ", "_");
  sl_sessionShortCuts << name;

  // Is there already this shortcut?
  KAction* sessionAction;
  if ( m_shortcuts->action( name.latin1() ) ) {
    sessionAction = m_shortcuts->action( name.latin1() );
  } else {
    sessionAction = new KAction( comment, 0, this, 0, m_shortcuts, name.latin1() );
  }
  connect( sessionAction, SIGNAL( activated() ), sessionNumberMapper, SLOT( map() ) );
  sessionNumberMapper->setMapping( sessionAction, cmd_serial );

}

void Konsole::loadSessionCommands()
{
  no2command.clear();

  cmd_serial = 99;
  cmd_first_screen = -1;

  if (!kapp->authorize("shell_access"))
     return;

  addSessionCommand(QString::null);

  QStringList lst = KGlobal::dirs()->findAllResources("appdata", "*.desktop", false, true);

  for(QStringList::Iterator it = lst.begin(); it != lst.end(); ++it )
    if (!(*it).endsWith("/shell.desktop"))
       addSessionCommand(*it);

  b_sessionShortcutsMapped = true;
}

void Konsole::createSessionMenus()
{
  if (no2command.isEmpty()) { // All sessions have been deleted
    m_session->insertItem(SmallIconSet("window_new"),
                          i18n("New &Window"), SESSION_NEW_WINDOW_ID);
    m_tabbarSessionsCommands->insertItem(SmallIconSet("window_new"),
                          i18n("New &Window"), SESSION_NEW_WINDOW_ID);
    return;
  }

  KSimpleConfig *cfg = no2command[SESSION_NEW_SHELL_ID];
  QString txt = cfg->readEntry("Name");
  QString icon = cfg->readEntry("Icon", "konsole");
  insertItemSorted(m_tabbarSessionsCommands, SmallIconSet(icon),
                   txt.replace('&',"&&"), SESSION_NEW_SHELL_ID );

  QString comment = cfg->readEntry("Comment");
  if (comment.isEmpty())
    comment=txt.prepend(i18n("New "));
  insertItemSorted(m_session, SmallIconSet(icon),
                   comment.replace('&',"&&"), SESSION_NEW_SHELL_ID);
  m_session->insertItem(SmallIconSet("window_new"),
                        i18n("New &Window"), SESSION_NEW_WINDOW_ID);
  m_tabbarSessionsCommands->insertItem(SmallIconSet("window_new"),
                        i18n("New &Window"), SESSION_NEW_WINDOW_ID);
  m_session->insertSeparator();
  m_tabbarSessionsCommands->insertSeparator();

  QIntDictIterator<KSimpleConfig> it( no2command );
  for ( ; it.current(); ++it ) {
    if ( it.currentKey() == SESSION_NEW_SHELL_ID )
      continue;

    QString txt = (*it).readEntry("Name");
    QString icon = (*it).readEntry("Icon", "konsole");
    insertItemSorted(m_tabbarSessionsCommands, SmallIconSet(icon),
                     txt.replace('&',"&&"), it.currentKey() );
    QString comment = (*it).readEntry("Comment");
    if (comment.isEmpty())
      comment=txt.prepend(i18n("New "));
    insertItemSorted(m_session, SmallIconSet(icon),
                     comment.replace('&',"&&"), it.currentKey());
  }

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
  QString icon = "konsole";
  cmd_serial++;
  m_session->insertItem( SmallIconSet( icon ), txt, cmd_serial, cmd_serial - 1 );
  m_tabbarSessionsCommands->insertItem( SmallIconSet( icon ), txt, cmd_serial );
  no2command.insert(cmd_serial,co);
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
    s = (ColorSchema*)colors->at(0);
    kdWarning() << "No schema with serial #"<<numb<<", using "<<s->relPath()<<" (#"<<s->numb()<<")." << endl;
    s_kconfigSchema = s->relPath();
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
     s = (ColorSchema*)colors->at(0);  //the default one
     kdWarning() << "No schema with the name " <<path<<", using "<<s->relPath()<<endl;
     s_kconfigSchema = s->relPath();
  }
  if (s->hasSchemaFileChanged())
  {
        const_cast<ColorSchema *>(s)->rereadSchemaFile();
  }
  if (s) setSchema(s);
}

// Called via main.cpp for session manager.
void Konsole::setEncoding(int index)
{
  if ( selectSetEncoding ) {
    selectSetEncoding->setCurrentItem(index);
    slotSetEncoding();
  }
}

void Konsole::setSchema(ColorSchema* s, TEWidget* tewidget)
{
  if (!s) return;
  if (!tewidget) tewidget=te;

  if (tewidget==te) {
    if (m_schema)
    {
      m_schema->setItemChecked(curr_schema,false);
      m_schema->setItemChecked(s->numb(),true);
    }

    s_schema = s->relPath();
    curr_schema = s->numb();
    pmPath = s->imagePath();
  }
  tewidget->setColorTable(s->table()); //FIXME: set twice here to work around a bug

  if (s->useTransparency()) {
    if (!argb_visual) {
      if (!rootxpms[tewidget])
        rootxpms.insert( tewidget, new KRootPixmap(tewidget) );
      rootxpms[tewidget]->setFadeEffect(s->tr_x(), QColor(s->tr_r(), s->tr_g(), s->tr_b()));
    } else {
      tewidget->setBlendColor(qRgba(s->tr_r(), s->tr_g(), s->tr_b(), int(s->tr_x() * 255)));
      tewidget->setErasePixmap( QPixmap() ); // make sure any background pixmap is unset
    }
  } else {
      if (rootxpms[tewidget]) {
        delete rootxpms[tewidget];
        rootxpms.remove(tewidget);
      }
       pixmap_menu_activated(s->alignment(), tewidget);
       tewidget->setBlendColor(qRgba(0, 0, 0, 0xff));
  }

  tewidget->setColorTable(s->table());
  QPtrListIterator<TESession> ses_it(sessions);
  for (; ses_it.current(); ++ses_it)
    if (tewidget==ses_it.current()->widget()) {
      ses_it.current()->setSchemaNo(s->numb());
      break;
    }
}

void Konsole::slotDetachSession()
{
  detachSession();
}

void Konsole::detachSession(TESession* _se) {
  if (!_se) _se=se;

  KRadioAction *ra = session2action.find(_se);
  ra->unplug(m_view);
  TEWidget* se_widget = _se->widget();
  session2action.remove(_se);
  action2session.remove(ra);
  int sessionIndex = sessions.findRef(_se);
  sessions.remove(_se);
  delete ra;

  if ( _se->isMasterMode() ) {
    // Disable master mode when detaching master
    setMasterMode(false);
  } else {
    QPtrListIterator<TESession> from_it(sessions);
    for(; from_it.current(); ++from_it) {
      TESession *from = from_it.current();
      if(from->isMasterMode())
        disconnect(from->widget(), SIGNAL(keyPressedSignal(QKeyEvent*)),
	           _se->getEmulation(), SLOT(onKeyPress(QKeyEvent*)));
    }
  }

  QColor se_tabtextcolor = tabwidget->tabColor( _se->widget() );

  disconnect( _se,SIGNAL(done(TESession*)),
              this,SLOT(doneSession(TESession*)) );

  disconnect( _se->getEmulation(),SIGNAL(ImageSizeChanged(int,int)), this,SLOT(notifySize(int,int)));
  disconnect( _se->getEmulation(),SIGNAL(changeColLin(int, int)), this,SLOT(changeColLin(int,int)) );
  disconnect( _se->getEmulation(),SIGNAL(changeColumns(int)), this,SLOT(changeColumns(int)) );
  disconnect( _se, SIGNAL(changeTabTextColor(TESession*, int)), this, SLOT(changeTabTextColor(TESession*, int)) );

  disconnect( _se,SIGNAL(updateTitle(TESession*)), this,SLOT(updateTitle(TESession*)) );
  disconnect( _se,SIGNAL(notifySessionState(TESession*,int)), this,SLOT(notifySessionState(TESession*,int)) );
  disconnect( _se,SIGNAL(disableMasterModeConnections()), this,SLOT(disableMasterModeConnections()) );
  disconnect( _se,SIGNAL(enableMasterModeConnections()), this,SLOT(enableMasterModeConnections()) );
  disconnect( _se,SIGNAL(renameSession(TESession*,const QString&)), this,SLOT(slotRenameSession(TESession*,const QString&)) );

  // TODO: "type" isn't passed properly
  Konsole* konsole = new Konsole(name(), b_histEnabled, !menubar->isHidden(), n_tabbar != TabNone, b_framevis,
                                 n_scroll != TEWidget::SCRNONE, 0, false, 0);
  konsole->enableFullScripting(b_fullScripting);
  // TODO; Make this work: konsole->enableFixedSize(b_fixedSize);
  konsole->resize(size());
  konsole->show();
  konsole->attachSession(_se);
  konsole->activateSession(_se);
  konsole->changeTabTextColor( _se, se_tabtextcolor.rgb() );//restore prev color
  konsole->slotTabSetViewOptions(m_tabViewMode);

  if (_se==se) {
    if (se == se_previous)
      se_previous=NULL;

    // pick a new session
    if (se_previous)
      se = se_previous;
    else
      se = sessions.at(sessionIndex ? sessionIndex - 1 : 0);
    session2action.find(se)->setChecked(true);
    QTimer::singleShot(1,this,SLOT(activateSession()));
  }

  if (sessions.count()==1)
    m_detachSession->setEnabled(false);

  tabwidget->removePage( se_widget );
  if (rootxpms[se_widget]) {
    delete rootxpms[se_widget];
    rootxpms.remove(se_widget);
  }
  delete se_widget;
  if (b_dynamicTabHide && tabwidget->count()==1)
    tabwidget->setTabBarHidden(true);

  if( m_removeSessionButton )
    m_removeSessionButton->setEnabled(tabwidget->count()>1);
}

void Konsole::attachSession(TESession* session)
{
  if (b_dynamicTabHide && sessions.count()==1 && n_tabbar!=TabNone)
    tabwidget->setTabBarHidden(false);

  TEWidget* se_widget = session->widget();

  te=new TEWidget(tabwidget);

  connect( te, SIGNAL(configureRequest(TEWidget*, int, int, int)),
           this, SLOT(configureRequest(TEWidget*,int,int,int)) );

  te->resize(se_widget->size());
  te->setSize(se_widget->Columns(), se_widget->Lines());
  initTEWidget(te, se_widget);
  session->changeWidget(te);
  te->setFocus();
  createSessionTab(te, SmallIconSet(session->IconName()), session->Title());
  setSchema(session->schemaNo());
  if (session->isMasterMode()) {
    disableMasterModeConnections(); // no duplicate connections, remove old
    enableMasterModeConnections();
  }

  QString title=session->Title();
  KRadioAction *ra = new KRadioAction(title.replace('&',"&&"), session->IconName(),
                                      0, this, SLOT(activateSession()), m_shortcuts);

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

  connect( session,SIGNAL(updateTitle(TESession*)), this,SLOT(updateTitle(TESession*)) );
  connect( session,SIGNAL(notifySessionState(TESession*,int)), this,SLOT(notifySessionState(TESession*,int)) );

  connect( session,SIGNAL(disableMasterModeConnections()), this,SLOT(disableMasterModeConnections()) );
  connect( session,SIGNAL(enableMasterModeConnections()), this,SLOT(enableMasterModeConnections()) );
  connect( session,SIGNAL(renameSession(TESession*,const QString&)), this,SLOT(slotRenameSession(TESession*,const QString&)) );
  connect( session->getEmulation(),SIGNAL(ImageSizeChanged(int,int)), this,SLOT(notifySize(int,int)));
  connect( session->getEmulation(),SIGNAL(changeColumns(int)), this,SLOT(changeColumns(int)) );
  connect( session->getEmulation(),SIGNAL(changeColLin(int, int)), this,SLOT(changeColLin(int,int)) );

  connect( session, SIGNAL(changeTabTextColor(TESession*, int)), this, SLOT(changeTabTextColor(TESession*, int)) );

  activateSession(session);
}

void Konsole::setSessionTitle( QString& title, TESession* ses )
{
   if ( !ses )
      ses = se;
   ses->setTitle( title );
   slotRenameSession( ses, title );
}

void Konsole::renameSession(TESession* ses) {
  QString title = ses->Title();
  bool ok;

  title = KInputDialog::getText( i18n( "Rename Session" ),
      i18n( "Session name:" ), title, &ok, this );

  if (!ok) return;

  ses->setTitle(title);
  slotRenameSession(ses,title);
}

void Konsole::slotRenameSession() {
  renameSession(se);
}

void Konsole::slotRenameSession(TESession* ses, const QString &name)
{
  KRadioAction *ra = session2action.find(ses);
  QString title=name;
  title=title.replace('&',"&&");
  ra->setText(title);
  ra->setIcon( ses->IconName() ); // I don't know why it is needed here
  if (m_tabViewMode!=ShowIconOnly)
    tabwidget->setTabLabel( ses->widget(), title );
  updateTitle();
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
        if ( KMessageBox::questionYesNo( m_finddialog,
             i18n("End of history reached.\n" "Continue from the beginning?"),
  	     i18n("Find"), KStdGuiItem::cont(), KStdGuiItem::cancel() ) == KMessageBox::Yes ) {
          m_find_first = true;
    	  slotFind();
        }
      }
      else {
        if ( KMessageBox::questionYesNo( m_finddialog,
             i18n("Beginning of history reached.\n" "Continue from the end?"),
  	     i18n("Find"), KStdGuiItem::cont(), KStdGuiItem::cancel() ) == KMessageBox::Yes ) {
          m_find_first = true;
   	  slotFind();
        }
      }
    }
  else
    KMessageBox::information( m_finddialog,
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
  // FIXME - mostLocalURL can't handle non-existing files yet, so this
  //         code doesn't work.
  KURL s_url = KFileDialog::getSaveURL(QString::null, QString::null, 0L, i18n("Save History"));
  if( s_url.isEmpty())
      return;
  KURL url = KIO::NetAccess::mostLocalURL( s_url, 0 );

  if( !url.isLocalFile() ) {
    KMessageBox::sorry(this, i18n("This is not a local file.\n"));
    return;
  }

  int query = KMessageBox::Continue;
  QFileInfo info;
  QString name( url.path() );
  info.setFile( name );
  if( info.exists() )
    query = KMessageBox::warningContinueCancel( this,
      i18n( "A file with this name already exists.\nDo you want to overwrite it?" ), i18n("File Exists"), i18n("Overwrite") );

  if (query==KMessageBox::Continue) {
    QFile file(url.path());
    if(!file.open(IO_WriteOnly)) {
      KMessageBox::sorry(this, i18n("Unable to write to file."));
      return;
    }

    QTextStream textStream(&file);
    assert( se && se->getEmulation() );
    se->getEmulation()->streamHistory( &textStream );

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
  if (!kapp->authorize("zmodem_download")) return;

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
                        "Please specify the folder you want to store the file(s):"),
                   this, "zmodem_dlg");
  dlg.setButtonOK(KGuiItem( i18n("&Download"),
                       i18n("Start downloading file to specified folder."),
                       i18n("Start downloading file to specified folder.")));
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

///////////////////////////////////////////////////////////
// This was to apply changes made to KControl fixed font to all TEs...
//  kvh - 03/10/2005 - We don't do this anymore...
void Konsole::slotFontChanged()
{
  TEWidget *oldTe = te;
  QPtrList<TEWidget> tes = activeTEs();
  for (TEWidget *_te = tes.first(); _te; _te = tes.next()) {
    te = _te;
//    setFont(n_font);
  }
  te = oldTe;
}

void Konsole::biggerFont(void) {
    if ( !se ) return;

    QFont f = te->getVTFont();
    f.setPointSize( f.pointSize() + 1 );
    te->setVTFont( f );
    activateSession();
}

void Konsole::smallerFont(void) {
    if ( !se ) return;

    QFont f = te->getVTFont();
    if ( f.pointSize() < 6 ) return;      // A minimum size
    f.setPointSize( f.pointSize() - 1 );
    te->setVTFont( f );
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
   if (sessions.count()>0)
     for (TESession *_se = sessions.first(); _se; _se = sessions.next())
        ret.append(_se->widget());
   else if (te)  // check for startup initalization case in newSession()
     ret.append(te);
   return ret;
}

#include "konsole.moc"

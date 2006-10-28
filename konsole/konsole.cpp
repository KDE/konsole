/*
    This file is part of the Konsole Terminal.
    
    Copyright (C) 2006 Robert Knight <robertknight@gmail.com>
    Copyright (C) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>
    Copyright (C) 1996 by Matthias Ettrich <ettrich@kde.org>

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

/*! \class Konsole

    \brief The Konsole main window which hosts the terminal emulator displays.

    This class is also responsible for setting up Konsole's menus, managing
    terminal sessions and applying settings.
*/

// System
#include <assert.h>
#include <config.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>

// Qt
#include <Q3PtrList>
#include <QCheckBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSignalMapper>
#include <QSpinBox>
#include <QStringList>
#include <QTextStream>
#include <QTime>
#include <QTime>
#include <QToolButton>
#include <QToolTip>
#include <QtDBus/QtDBus>

// KDE
#include <kacceleratormanager.h>
#include <kaction.h>
#include <kactioncollection.h>
#include <kactionmenu.h>
#include <kauthorized.h>
#include <kcharsets.h>
#include <kcolordialog.h>
#include <kdebug.h>
#include <kfiledialog.h>
#include <kfind.h>
#include <kfinddialog.h>
#include <kfontdialog.h>
#include <kglobalsettings.h>
#include <kicon.h>
#include <kiconloader.h>
#include <kinputdialog.h>
#include <kio/copyjob.h>
#include <kio/netaccess.h>
#include <kkeydialog.h>
#include <klocale.h>
#include <kmenu.h>
#include <kmenubar.h>
#include <kmessagebox.h>
#include <knotifydialog.h>
#include <kpalette.h>
#include <kparts/componentfactory.h>
#include <kprinter.h>
#include <kprocctrl.h>
#include <kregexpeditorinterface.h>
#include <krun.h>
#include <kselectaction.h>
#include <kservicetypetrader.h>
#include <kshell.h>
#include <kstandarddirs.h>
#include <kstdaction.h>
#include <kstringhandler.h>
//#include <ktabwidget.h>
#include <ktemporaryfile.h>
#include <ktip.h>
#include <ktoggleaction.h>
#include <ktogglefullscreenaction.h>
#include <ktoolinvocation.h>
#include <kurlrequesterdlg.h>
#include <netwm.h>
#include <knotifyconfigwidget.h>
#include <kwinmodule.h>

// Konsole
#include "SessionManager.h"
#include "TerminalCharacterDecoder.h"
#include "konsoleadaptor.h"
#include "konsolescriptingadaptor.h"
#include "printsettings.h"
#include "ViewSplitter.h"
#include "ViewContainer.h"
#include "NavigationItem.h"

#include "konsole.h"

#define KONSOLEDEBUG    kDebug(1211)

#define POPUP_NEW_SESSION_ID 121
#define POPUP_SETTINGS_ID 212

#define SESSION_NEW_WINDOW_ID 1
#define SESSION_NEW_SHELL_ID 100

extern bool true_transparency; // declared in main.cpp and konsole_part.cpp

// KonsoleFontSelectAction is now also used for selectSize!
class KonsoleFontSelectAction : public KSelectAction {
public:
    KonsoleFontSelectAction(const QString &text, KActionCollection* parent, const QString &name  = QString::null )
        : KSelectAction(text, parent, name) {}

protected:
    virtual void actionTriggered(QAction* action);
};

void KonsoleFontSelectAction::actionTriggered(QAction* action) {
    // emit even if it's already activated
    if (currentAction() == action) {
        trigger();
    } else {
        KSelectAction::actionTriggered(action);
    }
}

template class Q3PtrDict<TESession>;
template class Q3IntDict<KSimpleConfig>;
template class Q3PtrDict<KToggleAction>;

#define DEFAULT_HISTORY_SIZE 1000

Konsole::Konsole(const char* name, int histon, bool menubaron, bool tabbaron, bool frameon, bool scrollbaron,
                 const QString &type, bool b_inRestore, const int wanted_tabbar, const QString &workdir )
:KMainWindow(0)
,m_defaultSession(0)
,m_defaultSessionFilename("")
//,tabwidget(0)
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
,m_sessionList(0)

//Session Tabs Context Menu
    ,m_tabPopupMenu(0)
    ,m_tabPopupTabsMenu(0)
    ,m_tabbarPopupMenu(0)
    ,m_tabMonitorActivity(0)
    ,m_tabMonitorSilence(0)
    ,m_tabMasterMode(0)
//--

,m_zmodemUpload(0)
,monitorActivity(0)
,monitorSilence(0)
,masterMode(0)
,moveSessionLeftAction(0)
,moveSessionRightAction(0)
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
,bookmarkHandler(0)
,bookmarkHandlerSession(0)
,m_finddialog(0)
,saveHistoryDialog(0)
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
,_closing(false)
,m_tabViewMode(ShowIconAndText)
,b_dynamicTabHide(false)
,b_autoResizeTabs(false)
,b_framevis(true)
,b_fullscreen(false)
,m_menuCreated(false)
,b_warnQuit(false)
,b_allowResize(true) // Whether application may resize
,b_fixedSize(false) // Whether user may resize
,b_addToUtmp(true)
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
,_sessionManager(0)
{
  setObjectName( name );

  (void)new KonsoleAdaptor(this);
  QDBusConnection::sessionBus().registerObject(QLatin1String("/Konsole"), this);
  m_sessionGroup = new QActionGroup(this);

  isRestored = b_inRestore;

  menubar = menuBar();

  KAcceleratorManager::setNoAccel( menubar );

  kDebug() << "Warning: sessionMapper thingy not done yet " << endl;
  sessionNumberMapper = new QSignalMapper( this );
  /*connect( sessionNumberMapper, SIGNAL( mapped( int ) ),
          this, SLOT( newSessionTabbar( int ) ) );*/

  colors = new ColorSchemaList();
  colors->checkSchemas();

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
    c->setDesktopGroup();
    b_dynamicTabHide = c->readEntry("DynamicTabHide", QVariant(false)).toBool();
  }

  if (!tabbaron)
    n_tabbar = TabNone;

  makeTabWidget();

  _view = new ViewSplitter();
  _view->addContainer( new TabbedViewContainer() , Qt::Horizontal );

  setCentralWidget(_view);

//  setCentralWidget(tabwidget);

// SPLIT-VIEW Disabled
//  if (b_dynamicTabHide || n_tabbar==TabNone)
//    tabwidget->setTabBarHidden(true);

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

  connect(KGlobalSettings::self(), SIGNAL(kdisplayFontChanged()), this, SLOT(slotFontChanged()));
}


Konsole::~Konsole()
{
    QListIterator<TESession*> sessionIter( sessionManager()->sessions() );

    while ( sessionIter.hasNext() )
    {
        sessionIter.next()->closeSession();
    }
    
    // Wait a bit for all children to clean themselves up.
    while(sessions.count() && KProcessController::theKProcessController->waitForProcessExit(1))
        ;

    resetScreenSessions();
       
    delete m_defaultSession;

    // the tempfiles have autodelete=true, so the actual files are removed here too
    while (!tempfiles.isEmpty())
        delete tempfiles.takeFirst();

    delete colors;
    colors=0;

    delete kWinModule;
    kWinModule = 0;

	//tidy up dialogs
	delete saveHistoryDialog;
}

void Konsole::setAutoClose(bool on)
{
    if (sessions.first())
       sessions.first()->setAutoClose(on);
}

void Konsole::showTip()
{
   KTipDialog::showTip(this,QString(),true);
}

void Konsole::showTipOnStart()
{
   if (b_showstartuptip)
      KTipDialog::showTip(this);
}

/* ------------------------------------------------------------------------- */
/*  Make menu                                                                */
/* ------------------------------------------------------------------------- */

// Note about Konsole::makeGUI() - originally this was called to load the menus "on demand" (when
// the user moused over them for the first time).  This is not viable for the future.
// because it causes bugs:
// Keyboard accelerators don't work until  the user opens one of the menus.
// It also prevents the menus from being painted properly the first time they are opened.
//
// Theoretically the reason for loading "on demand" was for performance reasons
// makeGUI() takes about 150ms with a warm cache, and triggers IO that results in slowdown
// with a cold cache.
// Callgrind & wall-clock analysis suggests the expensive parts of this function are:
//
//      - Loading the icons for sessions via SmallIconSet is expensive at the time of
//        writing because KIconLoader's on-demand loading of icons
//        hasn't yet been ported for KDE4/Qt4.
//      - Searching all of the system paths for the executable needed
//        for each session, can be a problem if PATH environment variable contains many directories.
//        This can be made both more efficient and deferred until menus which list the various
//        types of schema are opened.
//      - IO related to colour schema files
//        adding some debug statements to the colour schema reading code, it seems that they are 
//        parsed multiple times unnecessarily when starting Konsole.
//
//        The only colour schema that needs to be parsed on startup is the one for the first
//        session which is created.  There appears to be some code which is supposed to prevent
//        repeat parsing of a color schema file if it hasn't changed - but that isn't working properly
//        (not looked at in-depth yet).  
//        When revealing the schema menu, only the schema titles
//        need to be extracted.  Only when a schema is then chosen (either for previewing or for
//        actual use in the terminal) does it need to be parsed fully.
//
//
//-- Robert Knight.

void Konsole::makeGUI()
{
   if (m_menuCreated) return;

   //timer for basic wall-clock profiling of this function
   QTime makeGUITimer;
   makeGUITimer.start();

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

   kDebug() << __FUNCTION__ << ": disconnect done - time = " << makeGUITimer.elapsed() << endl;

   // Remove the empty separator Qt inserts if the menu is empty on popup,
   // not sure if this will be "fixed" in Qt, for now use this hack (malte)
   if(!(isRestored)) {
     if (sender() && sender()->inherits("QPopupMenu") &&
       static_cast<const QMenu *>(sender())->actions().count() == 1)
       const_cast<QMenu *>(static_cast<const QMenu *>(sender()))->removeItemAt(0);
       }

   KActionCollection* actions = actionCollection();

   // Send Signal Menu
   if ( KAuthorized::authorizeKAction( "send_signal" ) )
   {
      m_signals = new KMenu( i18n( "&Send Signal" ), this );
      QAction* sigStop = m_signals->addAction( i18n( "&Suspend Task" ) + " (STOP)" );
      QAction* sigCont = m_signals->addAction( i18n( "&Continue Task" ) + " (CONT)" );
      QAction* sigHup = m_signals->addAction( i18n( "&Hangup" ) + " (HUP)" );
      QAction* sigInt = m_signals->addAction( i18n( "&Interrupt Task" ) + " (INT)" );
      QAction* sigTerm = m_signals->addAction( i18n( "&Terminate Task" ) + " (TERM)" );
      QAction* sigKill = m_signals->addAction( i18n( "&Kill Task" ) + " (KILL)" );
      QAction* sigUsr1 = m_signals->addAction( i18n( "User Signal &1" ) + " (USR1)" );
      QAction* sigUsr2 = m_signals->addAction( i18n( "User Signal &2" ) + " (USR2)" );
      sigStop->setData( QVariant( SIGSTOP ) );
      sigCont->setData( QVariant( SIGCONT ) );
      sigHup->setData( QVariant( SIGHUP ) );
      sigInt->setData( QVariant( SIGINT ) );
      sigTerm->setData( QVariant( SIGTERM ) );
      sigKill->setData( QVariant( SIGKILL ) );
      sigUsr1->setData( QVariant( SIGUSR1 ) );
      sigUsr2->setData( QVariant( SIGUSR2 ) );
      connect( m_signals, SIGNAL(triggered(QAction*)), SLOT(sendSignal(QAction*)));
      KAcceleratorManager::manage( m_signals );
   }

   kDebug() << __FUNCTION__ << ": signals done - time = " << makeGUITimer.elapsed() << endl;

   // Edit Menu
   m_edit->addAction( m_copyClipboard );
   m_edit->addAction( m_pasteClipboard );

   if ( m_signals )
      m_edit->addMenu( m_signals );

   if ( m_zmodemUpload )
   {
      m_edit->addSeparator();
      m_edit->addAction( m_zmodemUpload );
   }

   m_edit->addSeparator();
   m_edit->addAction( m_clearTerminal );
   m_edit->addAction( m_resetClearTerminal );

   m_edit->addSeparator();
   m_edit->addAction( m_findHistory );
   m_edit->addAction( m_findNext );
   m_edit->addAction( m_findPrevious );
   m_edit->addAction( m_saveHistory );

   m_edit->addSeparator();
   m_edit->addAction( m_clearHistory );
   m_edit->addAction( m_clearAllSessionHistories );

   // View Menu
   m_view->addAction( m_detachSession );
   m_view->addAction( m_renameSession );

   //Monitor for Activity / Silence
   m_view->addSeparator();
   m_view->addAction( monitorActivity );
   m_view->addAction( monitorSilence );
   //Send Input to All Sessions
   m_view->addAction( masterMode );

   m_view->addSeparator();
   KToggleAction *ra = session2action.find( se );
   if ( ra != 0 )
       m_view->addAction( ra );

   kDebug() << __FUNCTION__ << ": Edit and View done - time = " << makeGUITimer.elapsed() << endl;

   // Bookmarks menu
   if (bookmarkHandler)
      connect( bookmarkHandler, SIGNAL( openUrl( const QString&, const QString& )),
            SLOT( enterURL( const QString&, const QString& )));
   if (bookmarkHandlerSession)
      connect( bookmarkHandlerSession, SIGNAL( openUrl( const QString&, const QString& )),
            SLOT( newSession( const QString&, const QString& )));
   if (m_bookmarks)
      connect(m_bookmarks, SIGNAL(aboutToShow()), SLOT(bookmarks_menu_check()));
   if (m_bookmarksSession)
      connect(m_bookmarksSession, SIGNAL(aboutToShow()), SLOT(bookmarks_menu_check()));

   kDebug() << __FUNCTION__ << ": Bookmarks done - time = " << makeGUITimer.elapsed() << endl;

   // Schema Options Menu -----------------------------------------------------
   m_schema = new KMenu(i18n( "Sch&ema" ),this);
   m_schema->setIcon( SmallIconSet("colorize") );
   
   KAcceleratorManager::manage( m_schema );
   connect(m_schema, SIGNAL(activated(int)), SLOT(schema_menu_activated(int)));
   connect(m_schema, SIGNAL(aboutToShow()), SLOT(schema_menu_check()));


   // Keyboard Options Menu ---------------------------------------------------
   m_keytab = new KMenu( i18n("&Keyboard") , this);
   m_keytab->setIcon(SmallIconSet( "key_bindings" ));

   KAcceleratorManager::manage( m_keytab );
   connect(m_keytab, SIGNAL(activated(int)), SLOT(keytab_menu_activated(int)));

   // Options menu
   if ( m_options )
   {
      // Menubar on/off
      m_options->addAction( showMenubar );

      // Tabbar
      selectTabbar = new KSelectAction(i18n("&Tab Bar"), actions, "tabbar" );
      connect( selectTabbar, SIGNAL( triggered() ), this, SLOT( slotSelectTabbar() ) );
      QStringList tabbaritems;
      tabbaritems << i18n("&Hide") << i18n("&Top") << i18n("&Bottom");
      selectTabbar->setItems( tabbaritems );
      m_options->addAction( selectTabbar );

      // Scrollbar
      selectScrollbar = new KSelectAction(i18n("Sc&rollbar"), actions, "scrollbar" );
      connect( selectScrollbar, SIGNAL( triggered() ), this, SLOT(slotSelectScrollbar() ) );
      QStringList scrollitems;
      scrollitems << i18n("&Hide") << i18n("&Left") << i18n("&Right");
      selectScrollbar->setItems( scrollitems );
      m_options->addAction( selectScrollbar );

      // Fullscreen
      m_options->addSeparator();
      if ( m_fullscreen )
      {
        m_options->addAction( m_fullscreen );
        m_options->addSeparator();
      }

      // Select Bell
      selectBell = new KSelectAction(i18n("&Bell"), actions, "bell");
      selectBell->setIcon( KIcon( "bell") );
      connect( selectBell, SIGNAL( triggered() ), this, SLOT(slotSelectBell()) );
      QStringList bellitems;
      bellitems << i18n("System &Bell")
                << i18n("System &Notification")
                << i18n("&Visible Bell")
                << i18n("N&one");
      selectBell->setItems( bellitems );
      m_options->addAction( selectBell );

      KActionMenu* m_fontsizes = new KActionMenu( KIcon( "text" ),
                                                  i18n( "Font" ),
                                                  actions, 0L );
      KAction *action = new KAction( i18n( "&Enlarge Font" ), actions, "enlarge_font" );
      action->setIcon( KIcon( "fontsizeup" ) );
      connect( action, SIGNAL( triggered() ), this, SLOT( biggerFont() ) );
      m_fontsizes->addAction( action );

      action = new KAction( i18n( "&Shrink Font" ), actions, "shrink_font" );
      action->setIcon( KIcon( "fontsizedown" ) );
      connect( action, SIGNAL( triggered() ), this, SLOT( smallerFont() ) );
      m_fontsizes->addAction( action );

      action = new KAction( i18n( "Se&lect..." ), actions, "select_font" );
      action->setIcon( KIcon( "font" ) );
      connect( action, SIGNAL( triggered() ), this, SLOT( slotSelectFont() ) );
      m_fontsizes->addAction( action );

      m_options->addAction( m_fontsizes );

      // encoding menu, start with default checked !
      selectSetEncoding = new KSelectAction( i18n( "&Encoding" ), actions, "set_encoding" );
      selectSetEncoding->setIcon( KIcon( "charset" ) );
      connect( selectSetEncoding, SIGNAL( triggered() ), this, SLOT(slotSetEncoding()) );

      QStringList list = KGlobal::charsets()->descriptiveEncodingNames();
      list.prepend( i18n( "Default" ) );
      selectSetEncoding->setItems(list);
      selectSetEncoding->setCurrentItem (0);
      m_options->addAction( selectSetEncoding );

      if (KAuthorized::authorizeKAction("keyboard"))
        m_options->addMenu(m_keytab);
      
      // Schema
      if (KAuthorized::authorizeKAction("schema"))
        m_options->addMenu(m_schema);

      // Select size
      if (!b_fixedSize)
      {
         selectSize = new KonsoleFontSelectAction(i18n("S&ize"), actions, "size");
         connect(selectSize, SIGNAL(triggered(bool)), SLOT(slotSelectSize()));
         QStringList sizeitems;
         sizeitems << i18n("40x15 (&Small)")
            << i18n("80x24 (&VT100)")
            << i18n("80x25 (&IBM PC)")
            << i18n("80x40 (&XTerm)")
            << i18n("80x52 (IBM V&GA)")
            << ""
            << i18n("&Custom...");
         selectSize->setItems( sizeitems );
         m_options->addAction( selectSize );
      }

      KAction *historyType = new KAction(KIcon("history"), i18n("Hist&ory..."), actions, "history");
      connect(historyType, SIGNAL(triggered(bool) ), SLOT(slotHistoryType()));
      m_options->addAction( historyType );

      m_options->addSeparator();

      KAction *save_settings = new KAction(KIcon("filesave"), i18n("&Save as Default"), actions, "save_default");
      connect(save_settings, SIGNAL(triggered(bool) ), SLOT(slotSaveSettings()));
      m_options->addAction( save_settings );
      m_options->addSeparator();
      m_options->addAction( m_saveProfile );
      m_options->addSeparator();

      KAction *configureNotifications = KStdAction::configureNotifications( this, SLOT(slotConfigureNotifications()), actionCollection() );
      KAction *configureKeys = KStdAction::keyBindings( this, SLOT(slotConfigureKeys()), actionCollection() );
      KAction *configure = KStdAction::preferences( this, SLOT(slotConfigure()), actions );
      m_options->addAction( configureNotifications );
      m_options->addAction( configureKeys );
      m_options->addAction( configure );

      if (KGlobalSettings::insertTearOffHandle())
         m_options->setTearOffEnabled( true );
   }

   kDebug() << __FUNCTION__ << ": Options done - time = " << makeGUITimer.elapsed() << endl;

   // Help menu
   if ( m_help )
   {
      m_help->insertSeparator(1);
      m_help->insertItem(SmallIcon( "idea" ), i18n("&Tip of the Day"),
            this, SLOT(showTip()), 0, -1, 2);
   }

   // The different session menus
   buildSessionMenus();

   kDebug() << __FUNCTION__ << ": Session menus done - time = " << makeGUITimer.elapsed() << endl;

   connect(m_session, SIGNAL(triggered(QAction*)), SLOT(slotNewSessionAction(QAction*)));

   // Right mouse button menu
   if ( m_rightButton )
   {
      //Copy, Paste
      m_rightButton->addAction( m_copyClipboard );
      m_rightButton->addAction( m_pasteClipboard );

      KAction *selectionEnd = new KAction(i18n("Set Selection End"), actions, "selection_end");
      connect(selectionEnd, SIGNAL(triggered(bool) ), SLOT(slotSetSelectionEnd()));
      m_rightButton->addAction( selectionEnd );

      m_rightButton->addSeparator();

      //New Session menu
      if (m_tabbarSessionsCommands)
         m_rightButton->insertItem( i18n("New Sess&ion"), m_tabbarSessionsCommands, POPUP_NEW_SESSION_ID );

      //Detach Session, Rename Session
      m_rightButton->addAction( m_detachSession );
      m_rightButton->addAction( m_renameSession );

      m_rightButton->addSeparator();

      //Hide / Show Menu Bar
      m_rightButton->addAction( showMenubar );

      //Exit Fullscreen
      m_rightButton->addAction( m_fullscreen );

      //Close Session
      m_rightButton->addSeparator();
      m_rightButton->addAction( m_closeSession );
      if (KGlobalSettings::insertTearOffHandle())
         m_rightButton->setTearOffEnabled( true );
   }

   kDebug() << __FUNCTION__ << ": RMB menu done - time = " << makeGUITimer.elapsed() << endl;
 
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
   for (int i=0; i<m_schema->actions().count(); i++)
      m_schema->setItemChecked(i,false);


   kDebug() << __FUNCTION__ << ": Color schemas done - time = " << makeGUITimer.elapsed() << endl;
   m_schema->setItemChecked(curr_schema,true);

   Q_ASSERT( se != 0 );

   se->setSchemaNo(curr_schema);

   kDebug() << __FUNCTION__ << ": setSchemaNo done - time = " << makeGUITimer.elapsed() << endl;

   // insert keymaps into menu
   // This sorting seems a bit cumbersome; but it is not called often.
   QStringList kt_titles;
   typedef QMap<QString,KeyTrans*> QStringKeyTransMap;
   QStringKeyTransMap kt_map;

   for (int i = 0; i < KeyTrans::count(); i++)
   {
      KeyTrans* ktr = KeyTrans::find(i);
      assert( ktr );
      QString title=ktr->hdr().toLower();
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

   kDebug() << __FUNCTION__ << ": keytrans done - time = " << makeGUITimer.elapsed() << endl;


   applySettingsToGUI();
   isRestored = false;

   // Fill tab context menu
   setupTabContextMenu();

   if (m_options) {
      // Fill tab bar context menu
      m_tabbarPopupMenu = new KMenu( this );
      KAcceleratorManager::manage( m_tabbarPopupMenu );
      m_tabbarPopupMenu->addAction( selectTabbar );

      KSelectAction *viewOptions = new KSelectAction(actionCollection(), 0);
      viewOptions->setText(i18n("Tab &Options"));
      QStringList options;
      options << i18n("&Text && Icons") << i18n("Text &Only") << i18n("&Icons Only");
      viewOptions->setItems(options);
      viewOptions->setCurrentItem(m_tabViewMode);
      m_tabbarPopupMenu->addAction( viewOptions );
      connect(viewOptions, SIGNAL(activated(int)), this, SLOT(slotTabSetViewOptions(int)));
      slotTabSetViewOptions(m_tabViewMode);

      KToggleAction *dynamicTabHideOption = new KToggleAction( i18n( "&Dynamic Hide" ), actionCollection(), QString());
      connect(dynamicTabHideOption, SIGNAL(triggered(bool) ), SLOT( slotTabbarToggleDynamicHide() ));
      dynamicTabHideOption->setChecked(b_dynamicTabHide);
      m_tabbarPopupMenu->addAction( dynamicTabHideOption );

      KToggleAction *m_autoResizeTabs = new KToggleAction( i18n("&Auto Resize Tabs"), actionCollection(), QString());
      connect(m_autoResizeTabs, SIGNAL(triggered(bool) ), SLOT( slotToggleAutoResizeTabs() ));
      m_autoResizeTabs->setChecked(b_autoResizeTabs);
      m_tabbarPopupMenu->addAction( m_autoResizeTabs );
    }

   kDebug() << __FUNCTION__ << ": took " << makeGUITimer.elapsed() << endl;
}

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
    if(!found)
    {
      kWarning() << "Codec " << selectSetEncoding->currentText() << " not found!" << endl;
      qtc = QTextCodec::codecForLocale();
    }
  }

  se->setEncodingNo(selectSetEncoding->currentItem());
  se->getEmulation()->setCodec(qtc);
}

void Konsole::makeTabWidget()
{
 //SPLIT-VIEW Disabled
/*  //tabwidget = new SessionTabWidget(this);
  tabwidget = new KTabWidget(0);
 // tabwidget->show();
  tabwidget->setTabReorderingEnabled(true);
  tabwidget->setAutomaticResizeTabs( b_autoResizeTabs );
  tabwidget->setTabCloseActivatePrevious( true );
  tabwidget->setHoverCloseButton( true );
  connect( tabwidget, SIGNAL(closeRequest(QWidget*)), this,
                          SLOT(slotTabCloseSession(QWidget*)) );


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

  if (KAuthorized::authorizeKAction("shell_access")) {
    connect(tabwidget, SIGNAL(mouseDoubleClick()), SLOT(newSession()));

    m_newSessionButton = new QToolButton( tabwidget );
    m_newSessionButton->setPopupMode( QToolButton::MenuButtonPopup );
    m_newSessionButton->setToolTip(i18n("Click for new standard session\nClick and hold for session menu"));
    m_newSessionButton->setIcon( SmallIcon( "tab_new" ) );
    m_newSessionButton->setAutoRaise( true );
    m_newSessionButton->adjustSize();
    m_newSessionButton->setMenu( m_tabbarSessionsCommands );
    connect(m_newSessionButton, SIGNAL(clicked()), SLOT(newSession()));
    tabwidget->setCornerWidget( m_newSessionButton, Qt::BottomLeftCorner );
    m_newSessionButton->installEventFilter(this);

    m_removeSessionButton = new QToolButton( tabwidget );
    m_removeSessionButton->setToolTip(i18n("Close the current session"));
    m_removeSessionButton->setIcon( SmallIconSet( "tab_remove" ) );
    m_removeSessionButton->adjustSize();
    m_removeSessionButton->setAutoRaise(true);
    m_removeSessionButton->setEnabled(false);
    connect(m_removeSessionButton, SIGNAL(clicked()), SLOT(confirmCloseCurrentSession()));
    tabwidget->setCornerWidget( m_removeSessionButton, Qt::BottomRightCorner );

  }*/
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
        m_newSessionButton->showMenu();
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
  if (KAuthorized::authorizeKAction("shell_access")) {
    m_tabbarSessionsCommands = new KMenu( this );
    KAcceleratorManager::manage( m_tabbarSessionsCommands );
    connect(m_tabbarSessionsCommands, SIGNAL(triggered(QAction*)), SLOT(slotNewSessionAction(QAction*)));
  }

  m_session = new KMenu(this);
  KAcceleratorManager::manage( m_session );
  m_edit = new KMenu(this);
  KAcceleratorManager::manage( m_edit );
  m_view = new KMenu(this);
  KAcceleratorManager::manage( m_view );
  if (KAuthorized::authorizeKAction("bookmarks"))
  {
    bookmarkHandler = new KonsoleBookmarkHandler( this, true );
    m_bookmarks = bookmarkHandler->menu();
    // call manually to disable accelerator c-b for add-bookmark initially.
    bookmarks_menu_check();
  }

  if (KAuthorized::authorizeKAction("settings")) {
     m_options = new KMenu(this);
     KAcceleratorManager::manage( m_options );
  }

  if (KAuthorized::authorizeKAction("help"))
     m_help = helpMenu(); //helpMenu(QString(), false);

  if (KAuthorized::authorizeKAction("konsole_rmb")) {
     m_rightButton = new KMenu(this);
     KAcceleratorManager::manage( m_rightButton );
  }

  if (KAuthorized::authorizeKAction("bookmarks"))
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

  m_shortcuts = new KActionCollection( (QObject*) this);

  m_copyClipboard = new KAction(KIcon("editcopy"), i18n("&Copy"), m_shortcuts, "edit_copy");
  connect(m_copyClipboard, SIGNAL(triggered(bool) ), SLOT(slotCopyClipboard()));
  m_pasteClipboard = new KAction(KIcon("editpaste"), i18n("&Paste"), m_shortcuts, "edit_paste");
  connect(m_pasteClipboard, SIGNAL(triggered(bool) ), SLOT(slotPasteClipboard()));
  m_pasteClipboard->setShortcut(Qt::SHIFT+Qt::Key_Insert);
  m_pasteSelection = new KAction(i18n("Paste Selection"), m_shortcuts, "pasteselection");
  connect(m_pasteSelection, SIGNAL(triggered(bool) ), SLOT(slotPasteSelection()));
  m_pasteSelection->setShortcut(Qt::CTRL+Qt::SHIFT+Qt::Key_Insert);

  m_clearTerminal = new KAction(i18n("C&lear Terminal"), m_shortcuts, "clear_terminal");
  connect(m_clearTerminal, SIGNAL(triggered(bool) ), SLOT(slotClearTerminal()));
  m_resetClearTerminal = new KAction(i18n("&Reset && Clear Terminal"), m_shortcuts, "reset_clear_terminal");
  connect(m_resetClearTerminal, SIGNAL(triggered(bool) ), SLOT(slotResetClearTerminal()));
  m_findHistory = new KAction(KIcon("find"), i18n("&Find in History..."), m_shortcuts, "find_history");
  connect(m_findHistory, SIGNAL(triggered(bool) ), SLOT(slotFindHistory()));
  m_findHistory->setEnabled(b_histEnabled);

  m_findNext = new KAction(KIcon("next"), i18n("Find &Next"), m_shortcuts, "find_next");
  connect(m_findNext, SIGNAL(triggered(bool)), SLOT(slotFindNext()));
  m_findNext->setEnabled(b_histEnabled);

  m_findPrevious = new KAction(KIcon("previous"), i18n("Find Pre&vious"), m_shortcuts, "find_previous");
  connect(m_findPrevious, SIGNAL(triggered(bool)), SLOT(slotFindPrevious()));
  m_findPrevious->setEnabled( b_histEnabled );

  m_saveHistory = new KAction(KIcon("filesaveas"), i18n("S&ave History As..."), m_shortcuts, "save_history");
  connect(m_saveHistory, SIGNAL(triggered(bool)), SLOT(slotShowSaveHistoryDialog()));
  m_saveHistory->setEnabled(b_histEnabled );

  m_clearHistory = new KAction(KIcon("history_clear"), i18n("Clear &History"), m_shortcuts, "clear_history");
  connect(m_clearHistory, SIGNAL(triggered(bool)), SLOT(slotClearHistory()));
  m_clearHistory->setEnabled(b_histEnabled);

  m_clearAllSessionHistories = new KAction(KIcon("history_clear"), i18n("Clear All H&istories"), m_shortcuts, "clear_all_histories");
  connect(m_clearAllSessionHistories, SIGNAL(triggered(bool)), SLOT(slotClearAllSessionHistories()));

  m_detachSession = new KAction(i18n("&Detach Session"), m_shortcuts, "detach_session");
  m_detachSession->setIcon( KIcon("tab_breakoff") );
  connect( m_detachSession, SIGNAL( triggered() ), this, SLOT(slotDetachSession()) );
  m_detachSession->setEnabled(false);

  m_renameSession = new KAction(i18n("&Rename Session..."), m_shortcuts, "rename_session");
  connect(m_renameSession, SIGNAL(triggered(bool) ), SLOT(slotRenameSession()));
  m_renameSession->setShortcut(Qt::CTRL+Qt::ALT+Qt::Key_S);

  if (KAuthorized::authorizeKAction("zmodem_upload")) {
    m_zmodemUpload = new KAction( i18n( "&ZModem Upload..." ), m_shortcuts, "zmodem_upload" );
    m_zmodemUpload->setShortcut( Qt::CTRL+Qt::ALT+Qt::Key_U );
    connect( m_zmodemUpload, SIGNAL( triggered() ), this, SLOT( slotZModemUpload() ) );
  }

  monitorActivity = new KToggleAction ( KIcon("activity"), i18n( "Monitor for &Activity" ),
                                        m_shortcuts, "monitor_activity" );
  connect(monitorActivity, SIGNAL(triggered(bool) ), SLOT( slotToggleMonitor() ));
  monitorActivity->setCheckedState( KGuiItem( i18n( "Stop Monitoring for &Activity" ) ) );

  monitorSilence = new KToggleAction ( KIcon("silence"), i18n( "Monitor for &Silence" ), m_shortcuts, "monitor_silence" );
  connect(monitorSilence, SIGNAL(triggered(bool) ), SLOT( slotToggleMonitor() ));
  monitorSilence->setCheckedState( KGuiItem( i18n( "Stop Monitoring for &Silence" ) ) );

  masterMode = new KToggleAction(KIcon("remote"),  i18n( "Send &Input to All Sessions" ), m_shortcuts, "send_input_to_all_sessions" );
  connect(masterMode, SIGNAL(triggered(bool) ), SLOT( slotToggleMasterMode() ));

  showMenubar = new KToggleAction(KIcon("showmenu"),  i18n( "&Show Menu Bar" ), m_shortcuts, "show_menubar" );
  connect(showMenubar, SIGNAL(triggered(bool) ), SLOT( slotToggleMenubar() ));
  showMenubar->setCheckedState( KGuiItem( i18n("&Hide Menu Bar"), "showmenu", QString(), QString() ) );

  m_fullscreen = KStdAction::fullScreen(0, 0, m_shortcuts, this );
  connect( m_fullscreen,SIGNAL(toggled(bool)), this,SLOT(updateFullScreen(bool)));
  m_fullscreen->setChecked(b_fullscreen);

  m_saveProfile = new KAction( i18n( "Save Sessions &Profile..." ), m_shortcuts, "save_sessions_profile" );
  m_saveProfile->setIcon( KIcon("filesaveas") );
  connect( m_saveProfile, SIGNAL( triggered() ), this, SLOT( slotSaveSessionsProfile() ) );

  //help menu
  //if (m_help)
   //  m_help->setAccel(QKeySequence());
     // Don't steal F1 (handbook) accel (esp. since it not visible in
     // "Configure Shortcuts").

  m_closeSession = new KAction(KIcon("fileclose"), i18n("C&lose Session"), m_shortcuts, "close_session");
  connect(m_closeSession, SIGNAL(triggered(bool) ), SLOT( confirmCloseCurrentSession() ));
  m_print = new KAction(KIcon("fileprint"), i18n("&Print Screen..."), m_shortcuts, "file_print");
  connect(m_print, SIGNAL(triggered(bool) ), SLOT( slotPrint() ));
  m_quit = new KAction(KIcon("exit"), i18n("&Quit"), m_shortcuts, "file_quit");
  connect(m_quit, SIGNAL(triggered(bool) ), SLOT( close() ));

  KShortcut shortcut(Qt::CTRL+Qt::ALT+Qt::Key_N);
  shortcut.append(QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_N));

  KAction *action = new KAction(i18n("New Session"), m_shortcuts, "new_session");
  action->setShortcut( shortcut );
  connect( action, SIGNAL( triggered() ), this, SLOT(newSession()) );
  addAction( action );

  action = new KAction(i18n("Activate Menu"), m_shortcuts, "activate_menu");
  action->setShortcut( Qt::CTRL+Qt::ALT+Qt::Key_M );
  connect( action, SIGNAL( triggered() ), this, SLOT(activateMenu()) );
  addAction( action );

  action = new KAction(i18n("List Sessions"), m_shortcuts, "list_sessions");
  connect( action, SIGNAL( triggered() ), this, SLOT(listSessions()) );
  addAction( action );

  action = new KAction(i18n("Go to Previous Session"), m_shortcuts, "previous_session");
  action->setShortcut( QApplication::isRightToLeft() ? Qt::SHIFT+Qt::Key_Right : Qt::SHIFT+Qt::Key_Left );
  connect( action, SIGNAL( triggered() ), this, SLOT(prevSession()) );
  addAction( action );

  action = new KAction(i18n("Go to Next Session"), m_shortcuts, "next_session");
  action->setShortcut( QApplication::isRightToLeft() ? Qt::SHIFT+Qt::Key_Left : Qt::SHIFT+Qt::Key_Right );
  connect( action, SIGNAL( triggered() ), this, SLOT(nextSession()) );
  addAction( action );

  for (int i=1;i<13;i++) { // Due to 12 function keys?
      action = new KAction(i18n("Switch to Session %1", i), m_shortcuts, QString().sprintf("switch_to_session_%02d", i).toLatin1().constData());
      connect( action, SIGNAL( triggered() ), this, SLOT(switchToSession()) );
      addAction( action );
  }

  action = new KAction(i18n("Enlarge Font"), m_shortcuts, "bigger_font");
  connect(action, SIGNAL(triggered(bool) ), SLOT(biggerFont()));
  action = new KAction(i18n("Shrink Font"), m_shortcuts, "smaller_font");
  connect(action, SIGNAL(triggered(bool) ), SLOT(smallerFont()));

  action = new KAction(i18n("Toggle Bidi"), m_shortcuts, "toggle_bidi");
  connect(action, SIGNAL(triggered(bool) ), SLOT(toggleBidi()));
  action->setShortcut(Qt::CTRL+Qt::ALT+Qt::Key_B);
  addAction(action);

  // Should we load all *.desktop files now?  Required for Session shortcuts.
  // --> answer: No, because the Konsole main window won't have an associated SessionManger
  //     at this stage of program execution, so it isn't possible to load session type information.
  // TODO: Reimplement and test session shorcuts
 
  /*if ( KConfigGroup(KGlobal::config(), "General").readEntry("SessionShortcutsEnabled", QVariant(false)).toBool() ) {
    b_sessionShortcutsEnabled = true;
    loadSessionCommands();
    loadScreenSessions();
  }*/
  m_shortcuts->readSettings();

  m_sessionList = new KMenu(this);
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
		KGuiItem closeTabsButton(i18n("Close sessions"),KStdGuiItem::quit().iconName());
		
        if(sessions.count()>1) {
	    switch (
		KMessageBox::warningContinueCancel(
	    	    this,
            	    i18n( "You are about to close %1 open sessions. \n"
                	  "Are you sure you want to continue?" , sessions.count() ),
	    	    i18n("Confirm close") ,
				closeTabsButton,
				QString(),
				KMessageBox::PlainCaption)
	    ) {
		case KMessageBox::Yes :
            return true;
            
		    break;
		case KMessageBox::Cancel :
            return false;
	    }
	}
    }

   return true;
}

/**
 * Adjusts the size of the Konsole main window so that the
 * active terminal display has enough room to display the specified number of lines and
 * columns.
 */

//Implementation note:  setColLin() works by intructing the terminal display widget 
//to resize itself to accomodate the specified number of lines and columns, and then resizes
//the Konsole window to its sizeHint().

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

	//The terminal emulator widget has now been resized to fit in the required number of lines and
	//columns, so the main Konsole window now needs to be resized as well.
	//Normally adjustSize() could be used for this.
	//
	//However in the case of top-level widgets (such as the main Konsole window which
	//we are resizing here), adjustSize() also constrains the size of the widget to 2/3rds of the size
	//of the desktop -- I don't know why.  Unfortunately this means that the new terminal may be smaller
	//than the specified size, causing incorrect display in some applications.
	//So here we ignore the desktop size and just resize to the suggested size.
	resize( sizeHint() );

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
  KMenu *menu = (state & Qt::ControlModifier) ? m_session : m_rightButton;
  if (menu)
     menu->popup(_te->mapToGlobal(QPoint(x,y)));
}

void Konsole::slotTabContextMenu(QWidget* _te, const QPoint & pos)
{
   if (!m_menuCreated)
      makeGUI();

   //SPLIT-VIEW Disabled
  /*
  m_contextMenuSession = sessions.at( tabwidget->indexOf( _te ) );

  m_tabDetachSession->setEnabled( tabwidget->count()>1 );

  m_tabMonitorActivity->setChecked( m_contextMenuSession->isMonitorActivity() );
  m_tabMonitorSilence->setChecked( m_contextMenuSession->isMonitorSilence() );
  m_tabMasterMode->setChecked( m_contextMenuSession->isMasterMode() );

  m_tabPopupTabsMenu->clear();
  int counter=0;
  for (TESession *ses = sessions.first(); ses; ses = sessions.next()) {
    QString title=ses->Title();
    m_tabPopupTabsMenu->insertItem(SmallIconSet(ses->IconName()),title.replace('&',"&&"),counter++);
  }

  m_tabPopupMenu->popup( pos );*/
}

void Konsole::slotTabDetachSession() {
  detachSession( m_contextMenuSession );
}

void Konsole::slotTabRenameSession() {
  renameSession(m_contextMenuSession);
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

void Konsole::slotTabCloseSession(QWidget* sessionWidget)
{
/*	SPLIT-VIEW Disabled
 
    for (uint i=0;i<sessions.count();i++)
	{
		if (sessions.at(i)->widget() == sessionWidget)
			confirmCloseCurrentSession(sessions.at(i));
	}*/
}

void Konsole::slotTabbarContextMenu(const QPoint & pos)
{
   if (!m_menuCreated)
      makeGUI();

  if ( m_tabbarPopupMenu ) m_tabbarPopupMenu->popup( pos );
}

void Konsole::slotTabSetViewOptions(int mode)
{
  //SPLIT-VIEW Disabled
 /* m_tabViewMode = TabViewModes(mode);

  for(int i = 0; i < tabwidget->count(); i++) {
    QIcon icon = iconSetForSession(sessions.at(i));
    QString title;
    if (b_matchTabWinTitle)
      title = sessions.at(i)->displayTitle();
    else
      title = sessions.at(i)->Title();

    title=title.replace('&',"&&");
    switch(mode) {
      case ShowIconAndText:
        tabwidget->setTabIcon( i, icon );
        tabwidget->setTabText( i, title );
        break;
      case ShowTextOnly:
        tabwidget->setTabIcon( i, QIcon() );
        tabwidget->setTabText( i, title );
        break;
      case ShowIconOnly:
        tabwidget->setTabIcon( i, icon );
        tabwidget->setTabText( i, QString() );
        break;
    }
  }*/
}

void Konsole::slotToggleAutoResizeTabs()
{
  //SPLIT-VIEW Disabled
  /*b_autoResizeTabs = !b_autoResizeTabs;

  tabwidget->setAutomaticResizeTabs( b_autoResizeTabs );*/
}

void Konsole::slotTabbarToggleDynamicHide()
{
  //SPLIT-VIEW Disabled
 /* b_dynamicTabHide=!b_dynamicTabHide;
  if (b_dynamicTabHide && tabwidget->count()==1)
    tabwidget->setTabBarHidden(true);
  else
    tabwidget->setTabBarHidden(false);*/
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
      QString(), &ok, this );
  if ( ok ) {
    QString path = KStandardDirs::locateLocal( "data",
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
//KDE4: Need to test this conversion q3strlist to qstringlist
//        config->writeEntry(key, sessions.current()->getArgs());
        QStringList args_sl;
        QStringList args = sessions.current()->getArgs();
        QStringListIterator it( args );
        while(it.hasNext())
            args_sl << QString(it.next());
        config->writeEntry(key, args_sl);

        key = QString("Pgm%1").arg(counter);
        config->writeEntry(key, sessions.current()->getPgm());
// SPLIT-VIEW Disabled
//        key = QString("SessionFont%1").arg(counter);
//        config->writeEntry(key, (sessions.current()->widget())->getVTFont());
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
        //key = QString("TabColor%1").arg(counter);
        //config->writeEntry(key, tabwidget->tabColor((sessions.current())->widget()));
/* Test this when dialogs work again
        key = QString("History%1").arg(counter);
        config->writeEntry(key, sessions.current()->history().getSize());
        key = QString("HistoryEnabled%1").arg(counter);
        config->writeEntry(key, sessions.current()->history().isOn());
*/

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

    // SPLIT-VIEW Disabled
    // config->writeEntry("TabColor", tabwidget->tabTextColor( tabwidget->indexOf(se->widget())));
  }
  config->writeEntry("Fullscreen",b_fullscreen);
  config->writeEntry("scrollbar",n_scroll);
  config->writeEntry("tabbar",n_tabbar);
  config->writeEntry("bellmode",n_bell);
  config->writeEntry("keytab",KeyTrans::find(n_defaultKeytab)->id());
  config->writeEntry("ActiveSession", active);
  config->writeEntry("DefaultSession", m_defaultSessionFilename);
  config->writeEntry("TabViewMode", int(m_tabViewMode));
  config->writeEntry("DynamicTabHide", b_dynamicTabHide);
  config->writeEntry("AutoResizeTabs", b_autoResizeTabs);

  if (se) {
    config->writeEntry("EncodingName", se->encoding());
    config->writeEntry("history", se->history().getSize());
    config->writeEntry("historyenabled", b_histEnabled);

  // SPLIT-VIEW Disabled
  //  config->writeEntry("defaultfont", (se->widget())->getVTFont());
    s_kconfigSchema = colors->find( se->schemaNo() )->relPath();
    config->writeEntry("schema",s_kconfigSchema);
  }

  config->writeEntry("class",QObject::objectName());
  if (config != KGlobal::config())
  {
      saveMainWindowSettings(config);
  }

  if (!s_workDir.isEmpty())
    config->writePathEntry("workdir", s_workDir);

  if (se) {
    // Set the new default font
    // SPLIT-VIEW Disabled
    //defaultFont = se->widget()->getVTFont();
  }
}


// Called by constructor (with config = KGlobal::config())
// and by session-management (with config = sessionconfig).
// So it has to apply the settings when reading them.
void Konsole::readProperties(KConfig* config)
{
    readProperties(config, QString(), false);
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
     b_warnQuit=config->readEntry( "WarnQuit", true );
     b_allowResize=config->readEntry( "AllowResize", false);
     b_bidiEnabled = config->readEntry("EnableBidi", false);
     s_word_seps= config->readEntry("wordseps",":@-./_~");
     b_framevis = config->readEntry("has frame", false);
     Q3PtrList<TEWidget> tes = activeTEs();
     for (TEWidget *_te = tes.first(); _te; _te = tes.next()) {
       _te->setWordCharacters(s_word_seps);
       _te->setTerminalSizeHint( config->readEntry("TerminalSizeHint", false));
       _te->setFrameStyle( b_framevis?(QFrame::WinPanel|QFrame::Sunken):QFrame::NoFrame );
       _te->setBlinkingCursor(config->readEntry("BlinkingCursor", false));
       _te->setCtrlDrag(config->readEntry("CtrlDrag", true));
       _te->setCutToBeginningOfLine(config->readEntry("CutToBeginningOfLine", false));
       _te->setLineSpacing( config->readEntry( "LineSpacing", QVariant(0)).toUInt() );
       _te->setBidiEnabled(b_bidiEnabled);
     }

     monitorSilenceSeconds=config->readEntry("SilenceSeconds", QVariant(10)).toUInt();
     for (TESession *ses = sessions.first(); ses; ses = sessions.next())
       ses->setMonitorSilenceSeconds(monitorSilenceSeconds);

     b_matchTabWinTitle = config->readEntry("MatchTabWinTitle", QVariant(true)).toBool();
     config->setGroup("UTMP");
     b_addToUtmp = config->readEntry("AddToUtmp", QVariant(true)).toBool();
     config->setDesktopGroup();

     // SPLIT_VIEW Disabled
     // Do not set a default value; this allows the System-wide Scheme
     // to set the tab text color.
//     m_tabColor = config->readColorEntry("TabColor");
     //FIXME: Verify this code when KDE4 supports tab colors... kvh
     QVariant v_tabColor = config->readEntry("TabColor");
     m_tabColor = v_tabColor.value<QColor>();
   }

   if (!globalConfigOnly)
   {
      n_defaultKeytab=KeyTrans::find(config->readEntry("keytab","default"))->numb(); // act. the keytab for this session
      b_fullscreen = config->readEntry("Fullscreen", QVariant(false)).toBool();
      n_scroll   = qMin(config->readEntry("scrollbar", QVariant(TEWidget::SCRRIGHT)).toUInt(),2u);
      n_tabbar   = qMin(config->readEntry("tabbar", QVariant(TabBottom)).toUInt(),2u);
      n_bell = qMin(config->readEntry("bellmode", QVariant(TEWidget::BELLSYSTEM)).toUInt(),3u);

      // Options that should be applied to all sessions /////////////

      // (1) set menu items and Konsole members

      QVariant v_defaultFont = config->readEntry("defaultfont", QVariant(KGlobalSettings::fixedFont()));
      defaultFont = v_defaultFont.value<QFont>();

      //set the schema
      s_kconfigSchema=config->readEntry("schema");
      ColorSchema* sch = colors->find(schema.isEmpty() ? s_kconfigSchema : schema);
      if (!sch)
      {
         sch = (ColorSchema*)colors->at(0);  //the default one
         kWarning() << "Could not find schema named " <<s_kconfigSchema<<"; using "<<sch->relPath()<<endl;
         s_kconfigSchema = sch->relPath();
      }
      if (sch->hasSchemaFileChanged()) sch->rereadSchemaFile();
      s_schema = sch->relPath();
      curr_schema = sch->numb();
      pmPath = sch->imagePath();

      if (te) {
        if (sch->useTransparency())
        {
        }
        else
        {
           pixmap_menu_activated(sch->alignment());
        }

        te->setColorTable(sch->table()); //FIXME: set twice here to work around a bug
        te->setColorTable(sch->table());
        te->setScrollbarLocation(n_scroll);
        te->setBellMode(n_bell);
      }

      // History
      m_histSize = config->readEntry("history", QVariant(DEFAULT_HISTORY_SIZE)).toInt();
      b_histEnabled = config->readEntry("historyenabled", true);

      // Tab View Mode
      m_tabViewMode = TabViewModes(config->readEntry("TabViewMode", QVariant(ShowIconAndText)).toInt());
      b_dynamicTabHide = config->readEntry("DynamicTabHide", false);
      b_autoResizeTabs = config->readEntry("AutoResizeTabs", false);

      s_encodingName = config->readEntry( "EncodingName", "" ).toLower();
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
      notifySize(te->Columns(), te->Lines());
      selectTabbar->setCurrentItem(n_tabbar);
      showMenubar->setChecked(!menuBar()->isHidden());
      selectScrollbar->setCurrentItem(n_scroll);
      selectBell->setCurrentItem(n_bell);
      selectSetEncoding->setCurrentItem( se->encodingNo() );
   }
   updateKeytabMenu();

  // SPLIT-VIEW Disabled
  // tabwidget->setAutomaticResizeTabs( b_autoResizeTabs );
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
    QPalette palette;
	palette.setColor( tewidget->backgroundRole(), tewidget->getDefaultBackColor() );
	tewidget->setPalette(palette);
    return;
  }
  // FIXME: respect scrollbar (instead of te->size)
  n_render= item;
  switch (item)
  {
    case 1: // none
    case 2: // tile
			{
              QPalette palette;
		      palette.setBrush( tewidget->backgroundRole(), QBrush(pm) );
		      tewidget->setPalette(palette);
			}
    break;
    case 3: // center
            { QPixmap bgPixmap( tewidget->size() );
              bgPixmap.fill(tewidget->getDefaultBackColor());
              bitBlt( &bgPixmap, ( tewidget->size().width() - pm.width() ) / 2,
                                ( tewidget->size().height() - pm.height() ) / 2,
                      &pm, 0, 0,
                      pm.width(), pm.height() );

              QPalette palette;
		      palette.setBrush( tewidget->backgroundRole(), QBrush(bgPixmap) );
		      tewidget->setPalette(palette);
            }
    break;
    case 4: // full
            {
              float sx = (float)tewidget->size().width() / pm.width();
              float sy = (float)tewidget->size().height() / pm.height();
              QMatrix matrix;
              matrix.scale( sx, sy );

              QPalette palette;
		      palette.setBrush( tewidget->backgroundRole(), QBrush(pm.transformed( matrix )) );
		      tewidget->setPalette(palette);
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

   Q3PtrList<TEWidget> tes = activeTEs();
   for (TEWidget *_te = tes.first(); _te; _te = tes.next())
     _te->setScrollbarLocation(n_scroll);
   activateSession(); // maybe helps in bg
}

void Konsole::slotSelectFont() {
   /* SPLIT-VIEW Disabled
    if ( !se ) return;

   QFont font = se->widget()->getVTFont();
   if ( KFontDialog::getFont( font, true ) != QDialog::Accepted )
      return;

   se->widget()->setVTFont( font );
//  activateSession(); // activates the current*/
}

void Konsole::schema_menu_activated(int item)
{
  //SPLIT-VIEW Disabled

  /*if (!se) return;
  setSchema(item,se->widget());

  activateSession(); // activates the current*/
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

void Konsole::createSessionTab(TEWidget *widget, const QIcon &iconSet,
                               const QString &text, int index)
{
    //SPLIT-VIEW Disabled
 /* switch(m_tabViewMode) {
  case ShowIconAndText:
    tabwidget->insertTab(index, widget, iconSet, text);
    break;
  case ShowTextOnly:
    tabwidget->insertTab(index, widget, text);
    break;
  case ShowIconOnly:
    tabwidget->insertTab(index, widget, iconSet, QString());
    break;
  }
  if ( m_tabColor.isValid() )
    tabwidget->setTabTextColor( tabwidget->indexOf(widget), m_tabColor );*/
}

QIcon Konsole::iconSetForSession(TESession *session) const
{
  if (m_tabViewMode == ShowTextOnly)
    return QIcon();
  return SmallIconSet(session->isMasterMode() ? "remote" : session->IconName());
}


/**
    Toggle the Tabbar visibility
 */
void Konsole::slotSelectTabbar() {
   if (m_menuCreated)
      n_tabbar = selectTabbar->currentItem();

//SPLIT-VIEW Disabled
/*   if ( n_tabbar == TabNone ) {     // Hide tabbar
      tabwidget->setTabBarHidden( true );
   } else {
      if ( tabwidget->isTabBarHidden() )
         tabwidget->setTabBarHidden( false );
      if ( n_tabbar == TabTop )
         tabwidget->setTabPosition( QTabWidget::Top );
      else
         tabwidget->setTabPosition( QTabWidget::Bottom );
   }*/

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
	KNotifyConfigWidget::configure(this);
}

void Konsole::slotConfigureKeys()
{
  KKeyDialog::configure(m_shortcuts);
  m_shortcuts->writeSettings();

  QStringList ctrlKeys;

  for ( int i = 0; i < m_shortcuts->actions().count(); i++ )
  {
    KShortcut shortcut = (m_shortcuts->actions().value( i ))->shortcut();
    for( int j = 0; j < shortcut.count(); j++)
    {
      QKeySequence seq = shortcut.seq(j);
      int key = seq.isEmpty() ? 0 : seq[0]; // First Key of KeySequence
      if ((key & Qt::KeyboardModifierMask) == Qt::ControlModifier)
        ctrlKeys += QKeySequence(key).toString();
    }

    // Are there any shortcuts for Session Menu entries?
    if ( !b_sessionShortcutsEnabled &&
         m_shortcuts->actions().value( i )->shortcut().count() &&
         m_shortcuts->actions().value( i )->objectName().startsWith("SSC_") ) {
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
  args << "konsole";
  KToolInvocation::kdeinitExec( "kcmshell", args );
}

void Konsole::reparseConfiguration()
{
  KGlobal::config()->reparseConfiguration();
  readProperties(KGlobal::config(), QString(), true);

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
  uint count = m_shortcuts->actions().count();
  for ( uint i = 0; i < count; i++ )
  {
    KAction* action = m_shortcuts->actions().value( i );
    bool b_foundSession = false;
    if ( action->objectName().startsWith("SSC_") ) {
      QString name = action->objectName();

      // Check to see if shortcut's session has been loaded.
      for ( QStringList::Iterator it = sl_sessionShortCuts.begin(); it != sl_sessionShortCuts.end(); ++it ) {
        if ( QString::compare( *it, name ) == 0 ) {
          b_foundSession = true;
          break;
        }
      }
      if ( ! b_foundSession ) {
        action->setShortcut( KShortcut() );   // Clear shortcut
        m_shortcuts->writeSettings();
        delete action;           // Remove Action and Accel
        if ( i == 0 ) i = 0;     // Reset index
        else i--;
        count--;                 // = m_shortcuts->count();
      }
    }
  }

  m_shortcuts->readSettings();

  // User may have changed Schema->Set as default schema
  s_kconfigSchema = KGlobal::config()->readEntry("schema");
  ColorSchema* sch = colors->find(s_kconfigSchema);
  if (!sch)
  {
    sch = (ColorSchema*)colors->at(0);  //the default one
    kWarning() << "Could not find schema named " <<s_kconfigSchema<<"; using "<<sch->relPath()<<endl;
    s_kconfigSchema = sch->relPath();
  }
  if (sch->hasSchemaFileChanged()) sch->rereadSchemaFile();
  s_schema = sch->relPath();
  curr_schema = sch->numb();
  pmPath = sch->imagePath();

//SPLIT-VIEW Disabled
/*  for (TESession *_se = sessions.first(); _se; _se = sessions.next()) {
    ColorSchema* s = colors->find( _se->schemaNo() );
    if (s) {
      if (s->hasSchemaFileChanged())
        s->rereadSchemaFile();
      setSchema(s,_se->widget());
    }
  }*/
}

// Called via emulation via session
void Konsole::changeTabTextColor( TESession* ses, int rgb )
{
  //SPLIT-VIEW Disabled

  /*if ( !ses ) return;
  QColor color;
  color.setRgb( rgb );
  if ( !color.isValid() ) {
    kWarning()<<" Invalid RGB color "<<rgb<<endl;
    return;
  }
  tabwidget->setTabTextColor( tabwidget->indexOf(ses->widget()), color );*/
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

void Konsole::updateTitle()
{
  //SPLIT-VIEW Disabled

  //setting window titles, tab text etc. will always trigger a repaint of the affected widget
  //so we take care not to update titles, tab text etc. if the new and old text is the same.

 /* int se_index = tabwidget->indexOf( se->widget() );

  if ( windowTitle() != se->displayTitle() )
        setPlainCaption( se->displayTitle() );

  if ( windowIconText() != se->IconText() )
        setWindowIconText( se->IconText() );
 
  //FIXME:  May trigger redundant repaint of tabwidget tab icons if the icon hasn't changed 
  QIcon icon = iconSetForSession(se);
  tabwidget->setTabIcon(se_index, icon);
  
  QString iconName = se->IconName();
  KToggleAction *ra = session2action.find(se);
  if (ra)
  {
    // FIXME KAction port - should check to see if icon() == KIcon(icon), but currently won't work (as creates two KIconEngines)
    ra->setIcon(KIcon(iconName));
  }

  QString newTabText;
  
  if ( m_tabViewMode != ShowIconOnly )
  {
  	if ( b_matchTabWinTitle )
        newTabText = se->displayTitle().replace('&', "&&");
	else
		newTabText = se->Title();
  }

  if (tabwidget->tabText(se_index) != newTabText)
        tabwidget->setTabText(se_index,newTabText); */
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
 //SPLIT-VIEW Disabled
 
/*  Q3PtrListIterator<TESession> from_it(sessions);
  for (; from_it.current(); ++from_it) {
    TESession *from = from_it.current();
    if (from->isMasterMode()) {
      Q3PtrListIterator<TESession> to_it(sessions);
      for (; to_it.current(); ++to_it) {
        TESession *to = to_it.current();
        if (to!=from)
          disconnect(from->widget(),SIGNAL(keyPressedSignal(QKeyEvent*)),
              to->getEmulation(),SLOT(onKeyPress(QKeyEvent*)));
      }
    }
  }*/
}

void Konsole::enableMasterModeConnections()
{
 //SPLIT-VIEW Disabled
 
 /* Q3PtrListIterator<TESession> from_it(sessions);
  for (; from_it.current(); ++from_it) {
    TESession *from = from_it.current();
    if (from->isMasterMode()) {
      Q3PtrListIterator<TESession> to_it(sessions);
      for (; to_it.current(); ++to_it) {
        TESession *to = to_it.current();
        if (to!=from) {
          connect(from->widget(),SIGNAL(keyPressedSignal(QKeyEvent*)),
              to->getEmulation(),SLOT(onKeyPress(QKeyEvent*)));
        }
      }
    }
    from->setListenToKeyPress(true);
  }*/
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

KUrl Konsole::baseURL() const
{
  KUrl url;
  url.setPath(se->getCwd()+'/');
  return url;
}

void Konsole::enterURL(const QString& URL, const QString&)
{
  QString path, login, host, newtext;

  if (URL.startsWith("file:")) {
    KUrl uglyurl(URL);
    newtext=uglyurl.path();
    KRun::shellQuote(newtext);
    te->emitText("cd "+newtext+"\r");
  }
  else if (URL.contains("://")) {
    KUrl u(URL);
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
      newtext = newtext + ' ' + u.host();
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

void Konsole::sendSignal( QAction* action )
{
  if ( se )
    se->sendSignal( action->data().toInt() );
}

void Konsole::runSession(TESession* s)
{
  KToggleAction *ra = session2action.find(s);
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
      newTitle = i18nc("abbreviation of number","%1 No. %2", s->Title(), count);
    }
  }
  while (!nameOk);

  s->setTitle(newTitle);

  // create a new toggle action for the session
  KToggleAction *ra = new KToggleAction(KIcon(s->IconName()), newTitle.replace('&',"&&"),
      m_shortcuts, QString());
  ra->setActionGroup(m_sessionGroup);
  //ra->setChecked(true);
  connect(ra, SIGNAL(toggled(bool)), SLOT(activateSession()));

  action2session.insert(ra, s);
  session2action.insert(s,ra);
  sessions.append(s);
  if (sessions.count()>1) {
    if (!m_menuCreated)
      makeGUI();
    m_detachSession->setEnabled(true);
  }

  if ( m_menuCreated )
    m_view->addAction( ra );

  //SPLIT-VIEW Disabled
  //createSessionTab(te, SmallIconSet(s->IconName()), newTitle);
  
  //SPLIT-VIEW Disabled
  //setSchema(s->schemaNo(),s->widget());
  //tabwidget->setCurrentIndex(tabwidget->count()-1);
 
  disableMasterModeConnections(); // no duplicate connections, remove old
  enableMasterModeConnections();
 
 // SPLIT-VIEW Disabled
 // if( m_removeSessionButton )
 //   m_removeSessionButton->setEnabled(tabwidget->count()>1);
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
  m_sessionList->addTitle(i18n("Session List"));
  m_sessionList->setKeyboardShortcutsEnabled(true);
  for (TESession *ses = sessions.first(); ses; ses = sessions.next()) {
    QString title=ses->Title();
    m_sessionList->insertItem(SmallIconSet(ses->IconName()),title.replace('&',"&&"),counter++);
  }
  m_sessionList->adjustSize();
  m_sessionList->popup(mapToGlobal(QPoint((width()/2)-(m_sessionList->width()/2),(height()/2)-(m_sessionList->height()/2))));
}

void Konsole::switchToSession()
{
  activateSession( sender()->objectName().right( 2 ).toInt() -1 );
}

void Konsole::activateSession(int position)
{
  if (position<0 || position>=(int)sessions.count())
    return;
  activateSession( sessions.at(position) );
}

void Konsole::activateSession(QWidget* w)
{
  //SPLIT-VIEW Disabled
/*  activateSession(tabwidget->indexOf(w));
  w->setFocus();*/
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
  Q3PtrDictIterator<TESession> it( action2session ); // iterator for dict
  while ( it.current() )
  {
    KToggleAction *ra = (KToggleAction*)it.currentKey();
    if (ra->isChecked()) { s = it.current(); break; }
    ++it;
  }
  if (s!=NULL) activateSession(s);
}

void Konsole::activateSession(TESession *s)
{
  if (se)
  {
   // SPLIT-VIEW Disabled
   // se->setConnect(false);
   // se->setListenToKeyPress(true);
    notifySessionState(se,NOTIFYNORMAL);
    // Delete the session if isn't in the session list any longer.
    if (sessions.find(se) == -1)
    {
      delete se;
      se = 0;
    }
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
  KToggleAction *ra = session2action.find(se);
  if (!ra) {
    se=sessions.first();        // Get new/correct TESession
    ra = session2action.find(se);
  }
  ra->setChecked(true);

  QTimer::singleShot(1,this,SLOT(allowPrevNext())); // hack, hack, hack

  //SPLIT-VIEW Disabled
  /*if (tabwidget->currentWidget() != se->widget())
    tabwidget->setCurrentIndex( tabwidget->indexOf( se->widget() ) );
  te = se->widget();
  if (m_menuCreated) {
    if (selectBell) selectBell->setCurrentItem(te->bellMode());
    updateSchemaMenu();
  }*/

  notifySize(te->Columns(), te->Lines()); // set menu items
  s->setConnect(true);
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

// Set session encoding; don't use any menu items.
// System's encoding list may change, so search for encoding string.
// FIXME: A lot of duplicate code from slotSetSessionEncoding
void Konsole::setSessionEncoding( const QString &encoding, TESession *session )
{
  if ( encoding.isEmpty() )
    return;

  if ( !session )
    session = se;

  // availableEncodingNames and descriptEncodingNames are NOT returned
  // in the same order.
  QStringList items = KGlobal::charsets()->descriptiveEncodingNames();
  QString enc;

  // For purposes of using 'find' add a space after name,
  // otherwise 'iso 8859-1' will find 'iso 8859-13'
  QString t_enc = encoding + ' ';
  int i = 0;

  for( QStringList::ConstIterator it = items.begin(); it != items.end();
      ++it, ++i)
  {
    if ( (*it).indexOf( t_enc ) != -1 )
    {
      enc = *it;
      break;
    }
  }
  if (i >= items.count())
    return;

  bool found = false;
  enc = KGlobal::charsets()->encodingForName(enc);
  QTextCodec * qtc = KGlobal::charsets()->codecForName(enc, found);

  //kDebug()<<"setSessionEncoding="<<enc<<"; "<<i<<"; found="<<found<<endl;
  if ( !found )
    return;

  session->setEncodingNo( i + 1 );    // Take into account Default
  session->getEmulation()->setCodec(qtc);
  if (se == session)
    activateSession(se);

}

void Konsole::slotSetSessionEncoding(TESession *session, const QString &encoding)
{
  if (!selectSetEncoding)
    makeGUI();

  if ( !selectSetEncoding )         // when action/settings=false
    return;

  QStringList items = selectSetEncoding->items();

  QString enc;
  int i = 0;
  for(QStringList::ConstIterator it = items.begin();
      it != items.end(); ++it, ++i)
  {
    if ((*it).indexOf(encoding) != -1)
    {
      enc = *it;
      break;
    }
  }
  if (i >= items.count())
    return;

  bool found = false;
  enc = KGlobal::charsets()->encodingForName(enc);
  QTextCodec * qtc = KGlobal::charsets()->codecForName(enc, found);
  if(!found)
    return;

  session->setEncodingNo( i + 1 );    // Take into account Default
  session->getEmulation()->setCodec(qtc);
  if (se == session)
    activateSession(se);
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
  
  //SPLIT-VIEW Disabled
  //setSchema(s, session->widget());
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
  m_defaultSession = new KSimpleConfig(KStandardDirs::locate("appdata", filename), true /* read only */);
  m_defaultSession->setDesktopGroup();
  b_showstartuptip = m_defaultSession->readEntry("Tips", QVariant(true)).toBool();
  m_defaultSessionFilename=filename;
}

void Konsole::newSession()
{
    Q_ASSERT( sessionManager() && sessionManager()->defaultSessionType() );

    newSession( sessionManager()->defaultSessionType() );
}

void Konsole::slotNewSessionAction(QAction* action)
{
  if ( false /* TODO: check if action is for new window */ )
  {
    // TODO: "type" isn't passed properly
    Konsole* konsole = new Konsole(objectName().toLatin1(), b_histEnabled, !menubar->isHidden(), n_tabbar != TabNone, b_framevis,
        n_scroll != TEWidget::SCRNONE, 0, false, 0);
    konsole->setSessionManager( sessionManager() );
    konsole->newSession();
    konsole->enableFullScripting(b_fullScripting);
    konsole->enableFixedSize(b_fixedSize);
    konsole->setColLin(0,0); // Use defaults
    konsole->initFullScreen();
    konsole->show();
    return;
  }

  QListIterator<SessionInfo*> sessionIter(sessionManager()->availableSessionTypes());
  
  while (sessionIter.hasNext())
  {
      SessionInfo* info = sessionIter.next();
      if (info->name() == action->data().value<QString>())
      {
          newSession(info);
          resetScreenSessions();
      }
  }
}

void Konsole::newSession(const QString &type)
{
  if (type.isEmpty())
    newSession();
  else
  {
    QListIterator<SessionInfo*> sessionTypeIter(sessionManager()->availableSessionTypes());
    while (sessionTypeIter.hasNext())
    {
        SessionInfo* info = sessionTypeIter.next();
        if ( info->name() == type )
            newSession(info);
    }
  }
}

TESession* Konsole::newSession(SessionInfo* type)
{
    //create a new display
    TEWidget* display = new TEWidget(0);
    display->setMinimumSize(150,70);
    
    //copy settings from previous display if available, otherwise load them anew
    if ( !te ) 
    { 
        readProperties(KGlobal::config(), "", true);
        display->setVTFont( type->defaultFont( defaultFont ) );
        display->setScrollbarLocation(n_scroll);
        display->setBellMode(n_bell);
    }
    else
    {
        initTEWidget(display,te);
    }

    
    //create a session and attach the display to it
    TESession* session = sessionManager()->createSession( type->path() );
   
    session->setAddToUtmp(b_addToUtmp);
    session->setXonXoff(true);
    setSessionEncoding( s_encodingName, session );

    if (b_histEnabled && m_histSize)
        session->setHistory(HistoryTypeBuffer(m_histSize));
    else if (b_histEnabled && !m_histSize)
        session->setHistory(HistoryTypeFile());
    else
        session->setHistory(HistoryTypeNone());

    NavigationItem* item = new SessionNavigationItem(session);
    _view->activeSplitter()->activeContainer()->addView(display,item);
    session->addView( display );

    //set color scheme
    ColorSchema* sessionScheme = colors->find( type->colorScheme() );
    if (!sessionScheme)
        sessionScheme=(ColorSchema*)colors->at(0);  //the default one
    int schemeId = sessionScheme->numb();

    session->setSchemaNo(schemeId);
    
    //setup keyboard
    QString key = type->keyboardSetup();
    if (key.isEmpty())
        session->setKeymapNo(n_defaultKeytab);
    else 
    {
        // TODO: Fixes BR77018, see BR83000.
        if (key.endsWith(".keytab"))
            key.remove(".keytab");
        session->setKeymap(key);
    }

    //connect main window <-> session signals and slots
    connect( session,SIGNAL(done(TESession*)),
      this,SLOT(doneSession(TESession*)) );
    connect( session, SIGNAL( updateTitle() ),
      this, SLOT( updateTitle() ) );
    connect( session, SIGNAL( notifySessionState(TESession*, int) ),
      this, SLOT( notifySessionState(TESession*, int)) );
    connect( session, SIGNAL(disableMasterModeConnections()),
      this, SLOT(disableMasterModeConnections()) );
    connect( session, SIGNAL(enableMasterModeConnections()),
      this, SLOT(enableMasterModeConnections()) );
    connect( session, SIGNAL(renameSession(TESession*,const QString&)),
      this, SLOT(slotRenameSession(TESession*, const QString&)) );
    connect( session->getEmulation(), SIGNAL(changeColumns(int)),
      this, SLOT(changeColumns(int)) );
    connect( session->getEmulation(), SIGNAL(changeColLin(int,int)),
      this, SLOT(changeColLin(int,int)) );
    connect( session->getEmulation(), SIGNAL(ImageSizeChanged(int,int)),
      this, SLOT(notifySize(int,int)));
    connect( session, SIGNAL(zmodemDetected(TESession*)),
      this, SLOT(slotZModemDetected(TESession*)));
    connect( session, SIGNAL(updateSessionConfig(TESession*)),
      this, SLOT(slotUpdateSessionConfig(TESession*)));
    connect( session, SIGNAL(resizeSession(TESession*, QSize)),
      this, SLOT(slotResizeSession(TESession*, QSize)));
    connect( session, SIGNAL(setSessionEncoding(TESession*, const QString &)),
      this, SLOT(slotSetSessionEncoding(TESession*, const QString &)));
    connect( session, SIGNAL(getSessionSchema(TESession*, QString &)),
      this, SLOT(slotGetSessionSchema(TESession*, QString &)));
    connect( session, SIGNAL(setSessionSchema(TESession*, const QString &)),
      this, SLOT(slotSetSessionSchema(TESession*, const QString &)));
    connect( session, SIGNAL(changeTabTextColor(TESession*, int)),
      this,SLOT(changeTabTextColor(TESession*, int)) );


    //set up a warning message when the user presses Ctrl+S to avoid confusion
    connect( display,SIGNAL(flowControlKeyPressed(bool)),display,SLOT(outputSuspended(bool)) );

  //activate and run
  te = display;

  addSession(session);
  runSession(session);

  return session;
}

/*
 * Starts a new session based on URL.
 */
void Konsole::newSession(const QString& sURL, const QString& /*title*/)
{
  QStringList args;
  QString protocol, path, login, host;

  KUrl url = KUrl(sURL);
  if ((url.protocol() == "file") && (url.hasPath())) {
    path = url.path();

    //TODO - Make use of session properties here

    sessionManager()->addSetting( SessionManager::InitialWorkingDirectory , 
                                  SessionManager::SingleShot ,
                                  path );
   
    newSession();

   /* newSession(co, QString(), QStringList(), QString(), QString(),
        title.isEmpty() ? path : title, path);*/
    return;
  }
  else if ((!url.protocol().isEmpty()) && (url.hasHost())) {
    protocol = url.protocol();
    bool isSSH = (protocol == "ssh");
    args.append( protocol.toLatin1() ); /* argv[0] == command to run. */
    host = url.host();
    if (url.port() && isSSH) {
      args.append("-p");
      args.append(QByteArray().setNum(url.port()));
    }
    if (url.hasUser()) {
      login = url.user();
      args.append("-l");
      args.append(login.toLatin1());
    }
    args.append(host.toLatin1());
    if (url.port() && !isSSH)
      args.append(QByteArray().setNum(url.port()));
    
    //TODO : Make use of session properties here
#if 0
        newSession( NULL, protocol.toLatin1() /* protocol */, args /* arguments */,
        QString() /*term*/, QString() /*icon*/,
        title.isEmpty() ? path : title /*title*/, QString() /*cwd*/);
#endif
        
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
        	i18n("Are you sure you want to close this session?"),
        	i18n("Close Confirmation"), KGuiItem(i18n("C&lose Session"),"tab_remove"),
        	"ConfirmCloseSession")==KMessageBox::Continue)
   	{
		_se->closeSession();
	}
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

  KToggleAction *ra = session2action.find(s);
  m_view->removeAction( ra );

// SPLIT-VIEW Disabled
//  tabwidget->removePage( s->widget() );
//  delete s->widget();
//  if(m_removeSessionButton )
//    m_removeSessionButton->setEnabled(tabwidget->count()>1);
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
    if (sessions.count() && !_closing)
    {
     kDebug() << __FUNCTION__ << ": searching for session to activate" << endl;
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
  //  uint position=sessions.at();
  }
  if (sessions.count()==1) {
    m_detachSession->setEnabled(false);
  
  // SPLIT-VIEW Disabled
  //  if (b_dynamicTabHide && !tabwidget->isTabBarHidden())
  //    tabwidget->setTabBarHidden(true);
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

void Konsole::slotMovedTab(int from, int to)
{
  //SPLIT-VIEW Disabled
 /* TESession* _se = sessions.take(from);
  sessions.remove(_se);
  sessions.insert(to,_se);

  //get the action for the shell with a tab at position to+1
  KToggleAction* nextSessionAction = session2action.find(sessions.at(to + 1)); 

  KToggleAction *ra = session2action.find(_se);
  Q_ASSERT( ra );

  m_view->removeAction( ra );
  m_view->insertAction( nextSessionAction , ra );

  if (to==tabwidget->currentIndex()) {
    if (!m_menuCreated)
      makeGUI();
  }*/
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

  KToggleAction *ra = session2action.find(se);

  //get the action for the session just after the current session's new position
  KToggleAction* nextSessionAction = session2action.find(sessions.at(position));

  m_view->removeAction( ra );
  m_view->insertAction( nextSessionAction , ra );

  QColor oldcolor; // SPLIT-VIEW Disabled = tabwidget->tabTextColor(tabwidget->indexOf(se->widget()));

 // SPLIT-VIEW Disabled
 // tabwidget->blockSignals(true);
 // tabwidget->removePage(se->widget());
 // tabwidget->blockSignals(false);
 /* QString title = se->Title();
  createSessionTab(se->widget(), iconSetForSession(se),
      title.replace('&', "&&"), position-1);
  tabwidget->setCurrentIndex( tabwidget->indexOf( se->widget() ));
  tabwidget->setTabTextColor(tabwidget->indexOf(se->widget()),oldcolor);*/

  if (!m_menuCreated)
    makeGUI();
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

  //get the action for the session just after the current session's new position
  KToggleAction* nextSessionAction = session2action.find(sessions.at(position+2));

  KToggleAction *ra = session2action.find(se);
  m_view->removeAction( ra );
  m_view->insertAction( nextSessionAction , ra );

  //SPLIT-VIEW Disabled
  /*QColor oldcolor = tabwidget->tabTextColor(tabwidget->indexOf(se->widget()));

  tabwidget->blockSignals(true);
  tabwidget->removePage(se->widget());
  tabwidget->blockSignals(false);
  QString title = se->Title();
  createSessionTab(se->widget(), iconSetForSession(se),
      title.replace('&', "&&"), position+1);
  tabwidget->setCurrentIndex( tabwidget->indexOf( se->widget() ) );
  tabwidget->setTabTextColor(tabwidget->indexOf(se->widget()),oldcolor);
*/
  if (!m_menuCreated)
    makeGUI();
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
 // SPLIT-VIEW Disabled
 // if ( color.isValid() )
 //   tabwidget->setTabTextColor( tabwidget->indexOf(se->widget()), color );
}

void Konsole::initHistory(int lines, bool enable)
{
  return;
  // If no History#= is given in the profile, use the history
  // parameter saved in konsolerc.
  if ( lines < 0 ) lines = m_histSize;

  if ( enable )
    se->setHistory( HistoryTypeBuffer( lines ) );
  else
    se->setHistory( HistoryTypeNone() );
}

void Konsole::slotToggleMasterMode()
{
  if ( masterMode->isChecked() )
  {
      if ( KMessageBox::warningYesNo( this , 
                  i18n("Enabling this option will cause each key press to be sent to all running"
                      " sessions.  Are you sure you want to continue?") ,
                  i18n("Send Input to All Sessions"), 
                  KStdGuiItem::yes() , 
                  KStdGuiItem::no(),
                  i18n("Remember my answer and do not ask again.")) == KMessageBox::Yes )
      {
        setMasterMode( masterMode->isChecked() );
      }
      else
      {
        masterMode->setChecked(false);
      }
  }
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
        K3Icon::Small, 0, K3Icon::DefaultState, 0L, true);
    QPixmap active = KGlobal::instance()->iconLoader()->loadIcon(state_iconname,
        K3Icon::Small, 0, K3Icon::ActiveState, 0L, true);

    // make sure they are not larger than 16x16
    if (normal.width() > 16 || normal.height() > 16)
      normal = normal.scaled(16,16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (active.width() > 16 || active.height() > 16)
      active = active.scaled(16,16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QIcon iconset;
    iconset.addPixmap(normal, QIcon::Normal);
    iconset.addPixmap(active, QIcon::Active);

   // SPLIT-VIEW Disabled
   // tabwidget->setTabIcon(tabwidget->indexOf( session->widget() ), iconset);
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

  if (KAuthorized::authorizeKAction("file_print"))
  {
    m_session->addSeparator();
    m_session->addAction( m_print );
  }

  m_session->addSeparator();
  m_session->addAction( m_closeSession );

  m_session->addSeparator();
  m_session->addAction( m_quit );
}

void Konsole::addSessionCommand( SessionInfo* info )
{
  if ( !info->isAvailable() )
  {
     kDebug() << "Session not available - " << info->name() << endl;
     return;
  }

  // Add shortcuts only once and not for 'New Shell'.
  if ( ( b_sessionShortcutsMapped == true ) || ( cmd_serial == SESSION_NEW_SHELL_ID ) ) return;

  // Add an empty shortcut for each Session.
  QString actionText = info->newSessionText();   
  
  if (actionText.isEmpty())
    actionText=i18n("New %1", info->name());

  QString name = actionText;
  name.prepend("SSC_");  // Allows easy searching for Session ShortCuts
  name.replace(" ", "_");
  sl_sessionShortCuts << name;

  // Is there already this shortcut?
  KAction* sessionAction;
  if ( m_shortcuts->action( name ) ) {
    sessionAction = m_shortcuts->action( name );
  } else {
    sessionAction = new KAction( actionText, m_shortcuts, name );
  }
  connect( sessionAction, SIGNAL( activated() ), sessionNumberMapper, SLOT( map() ) );
  sessionNumberMapper->setMapping( sessionAction, cmd_serial );

}

void Konsole::loadSessionCommands()
{
  cmd_serial = 99;
  cmd_first_screen = -1;

  if (!KAuthorized::authorizeKAction("shell_access"))
     return;

  QListIterator<SessionInfo*> sessionTypeIter(sessionManager()->availableSessionTypes());

  while (sessionTypeIter.hasNext())
      addSessionCommand( sessionTypeIter.next() );

  b_sessionShortcutsMapped = true;
}

void Konsole::createSessionMenus()
{
    //get info about available session types
    //and produce a sorted list
    QListIterator<SessionInfo*> sessionTypeIter(sessionManager()->availableSessionTypes());
    SessionInfo* defaultSession = sessionManager()->defaultSessionType();
    
    QMap<QString,SessionInfo*> sortedNames;
    
    while ( sessionTypeIter.hasNext() )
    {
        SessionInfo* info = sessionTypeIter.next();
    
        if ( info != defaultSession )
        { 
            sortedNames.insert(info->newSessionText(),info);
        }
    }

    //add menu action for default session at top
    QIcon defaultIcon = SmallIconSet(defaultSession->icon());
    QAction* shellMenuAction = m_session->addAction( defaultIcon , defaultSession->newSessionText() );
    QAction* shellTabAction = m_tabbarSessionsCommands->addAction( defaultIcon , 
                                defaultSession->newSessionText() );
   
    shellMenuAction->setData( defaultSession->name() );
    shellTabAction->setData( defaultSession->name() );
    
    m_session->addSeparator();
    m_tabbarSessionsCommands->addSeparator();
        
    //then add the others in alphabetical order
    //TODO case-sensitive.  not ideal?
    QMapIterator<QString,SessionInfo*> nameIter( sortedNames );
    while ( nameIter.hasNext() )
    {
        SessionInfo* info = nameIter.next().value();
        QIcon icon = SmallIconSet(info->icon());
        
        QAction* menuAction = m_session->addAction( icon  , info->newSessionText() ); 
        QAction* tabAction = m_tabbarSessionsCommands->addAction( icon , info->newSessionText() );

        menuAction->setData( info->name() );
        tabAction->setData( info->name() );
    }

    if (m_bookmarksSession)
    {
        m_session->addSeparator();
        m_session->insertItem(SmallIconSet("keditbookmarks"),
                          i18n("New Shell at Bookmark"), m_bookmarksSession);
    
        m_tabbarSessionsCommands->addSeparator();
        m_tabbarSessionsCommands->insertItem(SmallIconSet("keditbookmarks"),
                          i18n("Shell at Bookmark"), m_bookmarksSession);
    }
}

void Konsole::addScreenSession(const QString &path, const QString &socket)
{
  KTemporaryFile *tmpFile = new KTemporaryFile();
  tmpFile->open();
  KSimpleConfig *co = new KSimpleConfig(tmpFile->fileName());
  co->setDesktopGroup();
  co->writeEntry("Name", socket);
  QString txt = i18nc("Screen is a program for controlling screens", "Screen at %1", socket);
  co->writeEntry("Comment", txt);
  co->writePathEntry("Exec", QString::fromLatin1("SCREENDIR=%1 screen -r %2")
    .arg(path).arg(socket));
  QString icon = "konsole";
  cmd_serial++;
  m_session->insertItem( SmallIconSet( icon ), txt, cmd_serial, cmd_serial - 1 );
  m_tabbarSessionsCommands->insertItem( SmallIconSet( icon ), txt, cmd_serial );
  tempfiles.append(tmpFile);
}

void Konsole::loadScreenSessions()
{
  if (!KAuthorized::authorizeKAction("shell_access"))
     return;
  QByteArray screenDir = getenv("SCREENDIR");
  if (screenDir.isEmpty())
    screenDir = QFile::encodeName(QDir::homePath()) + "/.screen/";
  // Some distributions add a shell function called screen that sets
  // $SCREENDIR to ~/tmp. In this case the variable won't be set here.
  if (!QFile::exists(screenDir))
    screenDir = QFile::encodeName(QDir::homePath()) + "/tmp/";
  QStringList sessions;
  // Can't use QDir as it doesn't support FIFOs :(
  DIR *dir = opendir(screenDir);
  if (dir)
  {
    struct dirent *entry;
    while ((entry = readdir(dir)))
    {
      QByteArray path = screenDir + QByteArray("/");
      path  += QByteArray(entry->d_name);
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
    kWarning() << "No schema with serial #"<<numb<<", using "<<s->relPath()<<" (#"<<s->numb()<<")." << endl;
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
     kWarning() << "No schema with the name " <<path<<", using "<<s->relPath()<<endl;
     s_kconfigSchema = s->relPath();
  }
  if (s->hasSchemaFileChanged())
  {
        const_cast<ColorSchema *>(s)->rereadSchemaFile();
  }
  if (s) setSchema(s);
}

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
    s_schema = s->relPath();
    curr_schema = s->numb();
    pmPath = s->imagePath();
  }
  tewidget->setColorTable(s->table()); //FIXME: set twice here to work around a bug

  if (s->useTransparency()) {
    if (!true_transparency) {
    } else {
      tewidget->setBlendColor(qRgba(s->tr_r(), s->tr_g(), s->tr_b(), int(s->tr_x() * 255)));
      QPalette palette;
      palette.setBrush( tewidget->backgroundRole(), QBrush( QPixmap() ) );
      tewidget->setPalette( palette ); // make sure any background pixmap is unset
    }
  } else {
       pixmap_menu_activated(s->alignment(), tewidget);
       tewidget->setBlendColor(qRgba(0, 0, 0, 0xff));
  }

  tewidget->setColorTable(s->table());

  //SPLIT-VIEW Disabled
/*  Q3PtrListIterator<TESession> ses_it(sessions);
  for (; ses_it.current(); ++ses_it)
    if (tewidget==ses_it.current()->widget()) {
      ses_it.current()->setSchemaNo(s->numb());
      break;
    }*/
}

void Konsole::slotDetachSession()
{
  detachSession();
}

void Konsole::detachSession(TESession* _se) {
   //SPLIT-VIEW Disabled

  /*if (!_se) _se=se;

  KToggleAction *ra = session2action.find(_se);
  m_view->removeAction( ra );
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
  Q3PtrListIterator<TESession> from_it(sessions);
    for(; from_it.current(); ++from_it) {
      TESession *from = from_it.current();
      if(from->isMasterMode())
        disconnect(from->widget(), SIGNAL(keyPressedSignal(QKeyEvent*)),
                  _se->getEmulation(), SLOT(onKeyPress(QKeyEvent*)));
    }
  }

  QColor se_tabtextcolor = tabwidget->tabTextColor( tabwidget->indexOf(_se->widget()) );

  disconnect( _se,SIGNAL(done(TESession*)),
              this,SLOT(doneSession(TESession*)) );

  disconnect( _se->getEmulation(),SIGNAL(ImageSizeChanged(int,int)), this,SLOT(notifySize(int,int)));
  disconnect( _se->getEmulation(),SIGNAL(changeColLin(int, int)), this,SLOT(changeColLin(int,int)) );
  disconnect( _se->getEmulation(),SIGNAL(changeColumns(int)), this,SLOT(changeColumns(int)) );
  disconnect( _se, SIGNAL(changeTabTextColor(TESession*, int)), this, SLOT(changeTabTextColor(TESession*, int)) );

  disconnect( _se,SIGNAL(updateTitle()), this,SLOT(updateTitle()) );
  disconnect( _se,SIGNAL(notifySessionState(TESession*,int)), this,SLOT(notifySessionState(TESession*,int)) );
  disconnect( _se,SIGNAL(disableMasterModeConnections()), this,SLOT(disableMasterModeConnections()) );
  disconnect( _se,SIGNAL(enableMasterModeConnections()), this,SLOT(enableMasterModeConnections()) );
  disconnect( _se,SIGNAL(renameSession(TESession*,const QString&)), this,SLOT(slotRenameSession(TESession*,const QString&)) );

  // TODO: "type" isn't passed properly
  Konsole* konsole = new Konsole( objectName().toLatin1(), b_histEnabled, !menubar->isHidden(), n_tabbar != TabNone, b_framevis,
                                 n_scroll != TEWidget::SCRNONE, 0, false, 0);

  konsole->setSessionManager(sessionManager());

  konsole->enableFullScripting(b_fullScripting);
  // TODO; Make this work: konsole->enableFixedSize(b_fixedSize);
  konsole->resize(size());
  konsole->attachSession(_se);
  _se->removeView( _se->primaryView() );
  
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
  delete se_widget;
  if (b_dynamicTabHide && tabwidget->count()==1)
    tabwidget->setTabBarHidden(true);

  if( m_removeSessionButton )
    m_removeSessionButton->setEnabled(tabwidget->count()>1);

  //show detached session
  konsole->show();*/
}

void Konsole::attachSession(TESession* session)
{
  //SPLIT-VIEW Disabled

  /*if (b_dynamicTabHide && sessions.count()==1 && n_tabbar!=TabNone)
    tabwidget->setTabBarHidden(false);

  TEWidget* se_widget = session->widget();

  te=new TEWidget(tabwidget);

  connect( te, SIGNAL(configureRequest(TEWidget*, int, int, int)),
           this, SLOT(configureRequest(TEWidget*,int,int,int)) );

  te->resize(se_widget->size());
  te->setSize(se_widget->Columns(), se_widget->Lines());
  initTEWidget(te, se_widget);
  session->addView(te);
  te->setFocus();
  createSessionTab(te, SmallIconSet(session->IconName()), session->Title());
  setSchema(session->schemaNo() , te);
  if (session->isMasterMode()) {
    disableMasterModeConnections(); // no duplicate connections, remove old
    enableMasterModeConnections();
  }

  QString title=session->Title();
  KToggleAction *ra = new KToggleAction(KIcon(session->IconName()), title.replace('&',"&&"),
                                        m_shortcuts, QString());
  connect(ra, SIGNAL(triggered(bool)), SLOT(activateSession()));

  ra->setActionGroup(m_sessionGroup);
  ra->setChecked(true);

  action2session.insert(ra, session);
  session2action.insert(session,ra);
  sessions.append(session);
  if (sessions.count()>1)
    m_detachSession->setEnabled(true);

  if ( m_menuCreated )
    m_view->addAction( ra );

  connect( session,SIGNAL(done(TESession*)),
           this,SLOT(doneSession(TESession*)) );

  connect( session,SIGNAL(updateTitle()), this,SLOT(updateTitle()) );
  connect( session,SIGNAL(notifySessionState(TESession*,int)), this,SLOT(notifySessionState(TESession*,int)) );

  connect( session,SIGNAL(disableMasterModeConnections()), this,SLOT(disableMasterModeConnections()) );
  connect( session,SIGNAL(enableMasterModeConnections()), this,SLOT(enableMasterModeConnections()) );
  connect( session,SIGNAL(renameSession(TESession*,const QString&)), this,SLOT(slotRenameSession(TESession*,const QString&)) );
  connect( session->getEmulation(),SIGNAL(ImageSizeChanged(int,int)), this,SLOT(notifySize(int,int)));
  connect( session->getEmulation(),SIGNAL(changeColumns(int)), this,SLOT(changeColumns(int)) );
  connect( session->getEmulation(),SIGNAL(changeColLin(int, int)), this,SLOT(changeColLin(int,int)) );

  connect( session, SIGNAL(changeTabTextColor(TESession*, int)), this, SLOT(changeTabTextColor(TESession*, int)) );

  activateSession(session);*/
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

void Konsole::slotRenameSession(TESession* session, const QString &name)
{
  //SPLIT-VIEW Disabled

 /* KToggleAction *ra = session2action.find(session);
  QString title=name;
  title=title.replace('&',"&&");
  ra->setText(title);
  ra->setIcon( KIcon( session->IconName() ) ); // I don't know why it is needed here
  if (m_tabViewMode!=ShowIconOnly) {
    int sessionTabIndex = tabwidget->indexOf( session->widget() );
    tabwidget->setTabText( sessionTabIndex, title );
  }
  updateTitle();*/
}


void Konsole::slotClearAllSessionHistories() {
  for (TESession *_se = sessions.first(); _se; _se = sessions.next())
    _se->clearHistory();
}

//////////////////////////////////////////////////////////////////////

HistoryTypeDialog::HistoryTypeDialog(const HistoryType& histType,
                                     unsigned int histSize,
                                     QWidget *parent)
  : KDialog( parent )
{
  setCaption( i18n("History Configuration") );
  setButtons( Help | Default | Ok | Cancel );
  setDefaultButton(Ok);
  setModal( true );
  showButtonSeparator( true );

  QFrame *mainFrame = new QFrame;
  setMainWidget( mainFrame );

  QHBoxLayout *hb = new QHBoxLayout(mainFrame);

  m_btnEnable    = new QCheckBox(i18n("&Enable"), mainFrame);
  connect(m_btnEnable, SIGNAL(toggled(bool)), SLOT(slotHistEnable(bool)));

  m_label = new QLabel(i18n("&Number of lines: "), mainFrame);

  m_size = new QSpinBox(mainFrame);
  m_size->setRange(0, 10 * 1000 * 1000);
  m_size->setSingleStep(100);
  m_size->setValue(histSize);
  m_size->setSpecialValueText(i18nc("Unlimited (number of lines)", "Unlimited"));

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
  connect(this,SIGNAL(defaultClicked()),SLOT(slotDefault()));
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
    m_finddialog = new KFindDialog(this);
    m_finddialog->setButtons( KDialog::User1|KDialog::Close );
    m_finddialog->setButtonGuiItem( KDialog::User1, KStdGuiItem::find() );
    m_finddialog->setModal(false);
    m_finddialog->setDefaultButton(KDialog::User1);

    m_finddialog->setObjectName("konsolefind");
    m_finddialog->setSupportsWholeWordsFind(false);
    m_finddialog->setHasCursor(false);
    m_finddialog->setHasSelection(false);

    connect(m_finddialog, SIGNAL(user1Clicked()), this,SLOT(slotFind()));
    connect(m_finddialog, SIGNAL(finished()), this,SLOT(slotFindDone()));
  }

  QString string = m_finddialog->pattern();
  m_finddialog->setPattern(string.isEmpty() ? m_find_pattern : string);

  m_find_first = true;
  m_find_found = false;

  m_finddialog->show();
}

void Konsole::slotFindNext()
{
  if( !m_finddialog ) {
    slotFindHistory();
    return;
  }

  QString string;
  string = m_finddialog->pattern();
  m_finddialog->setPattern(string.isEmpty() ? m_find_pattern : string);

  slotFind();
}

void Konsole::slotFindPrevious()
{
  if( !m_finddialog ) {
    slotFindHistory();
    return;
  }

  QString string;
  string = m_finddialog->pattern();
  m_finddialog->setPattern(string.isEmpty() ? m_find_pattern : string);

  long options = m_finddialog->options();
  long reverseOptions = options;
  if (options & KFind::FindBackwards)
    reverseOptions &= ~KFind::FindBackwards;
  else
    reverseOptions |= KFind::FindBackwards;
  m_finddialog->setOptions( reverseOptions );
  slotFind();
  m_finddialog->setOptions( options );
}

void Konsole::slotFind()
{
  if (m_find_first) {
    se->getEmulation()->findTextBegin();
    m_find_first = false;
  }

  bool forward = !(m_finddialog->options() & KFind::FindBackwards);
  m_find_pattern = m_finddialog->pattern();

  QTime time;
  time.start();
  if (se->getEmulation()->findTextNext(m_find_pattern,
                          forward,
                          (m_finddialog->options() & Qt::CaseSensitive),
                          (m_finddialog->options() & KFind::RegularExpression)))
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
  {
	kDebug() << __FUNCTION__ << ": search took " << time.elapsed() << " msecs." << endl;

    KMessageBox::information( m_finddialog,
    	i18n( "Search string '%1' not found." , KStringHandler::csqueeze(m_find_pattern)),
	i18n( "Find" ) );
  }
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
  KUrl originalUrl = saveHistoryDialog->selectedUrl();

  if( originalUrl.isEmpty())
      return;
  KUrl localUrl = KIO::NetAccess::mostLocalUrl( originalUrl, 0 );

  KTemporaryFile* tempFile = 0;

  if( !localUrl.isLocalFile() ) {
    tempFile = new KTemporaryFile();
    tempFile->setPrefix("konsole_history");
    tempFile->open();
    localUrl = KUrl::fromPath(tempFile->fileName());
  }

  int query = KMessageBox::Continue;
  QFileInfo info;
  QString name( localUrl.path() );
  info.setFile( name );
  if( info.exists() )
    query = KMessageBox::warningContinueCancel( this,
      i18n( "A file with this name already exists.\nDo you want to overwrite it?" ), i18n("File Exists"), KGuiItem(i18n("Overwrite")) );

  if (query==KMessageBox::Continue) 
  {
    QFile file(localUrl.path());
    if(!file.open(QIODevice::WriteOnly)) {
      KMessageBox::sorry(this, i18n("Unable to write to file."));
      delete tempFile;
      return;
    }

    QTextStream textStream(&file);
	TerminalCharacterDecoder* decoder = 0;

	if (saveHistoryDialog->currentMimeFilter() == "text/html")
		decoder = new HTMLDecoder();
	else
		decoder = new PlainTextDecoder();

    sessions.current()->getEmulation()->writeToStream( &textStream , decoder);
	delete decoder;

    file.close();
    if(file.error() != QFile::NoError) {
      KMessageBox::sorry(this, i18n("Could not save history."));
      delete tempFile;
      return;
    }

    if (tempFile)
        KIO::NetAccess::file_copy(localUrl, originalUrl);

  }
  delete tempFile;
}

void Konsole::slotShowSaveHistoryDialog()
{
  if (!saveHistoryDialog)
  {
		 saveHistoryDialog = new KFileDialog( QString(":konsole") , QString() , this);
		 saveHistoryDialog->setCaption( i18n("Save History As") );
  		 QStringList mimeTypes;
  		 mimeTypes << "text/plain";
  		 mimeTypes << "text/html";
  		 saveHistoryDialog->setMimeFilter(mimeTypes,"text/plain");

		 connect( saveHistoryDialog , SIGNAL(okClicked()), this , SLOT(slotSaveHistory()) );
  }

  saveHistoryDialog->show();

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

  QStringList files = KFileDialog::getOpenFileNames(QString(), QString(), this,
  	i18n("Select Files to Upload"));
  if (files.isEmpty())
    return;

  se->startZModem(zmodem, QString(), files);
}

void Konsole::slotZModemDetected(TESession *session)
{
  if (!KAuthorized::authorizeKAction("zmodem_download")) return;

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
  KUrlRequesterDlg dlg(KGlobalSettings::documentPath(),
                   i18n("A ZModem file transfer attempt has been detected.\n"
                        "Please specify the folder you want to store the file(s):"),
                   this);
  dlg.setButtonGuiItem(KDialog::Ok, KGuiItem( i18n("&Download"),
                       i18n("Start downloading file to specified folder."),
                       i18n("Start downloading file to specified folder.")));
  if (!dlg.exec())
  {
     session->cancelZModem();
  }
  else
  {
     const KUrl &url = dlg.selectedUrl();
     session->startZModem(zmodem, url.path(), QStringList());
  }
}

void Konsole::slotPrint()
{
  KPrinter printer;
  printer.addDialogPage(new PrintSettings());
  if (printer.setup(this, i18n("Print %1", se->Title())))
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
  Q3PtrList<TEWidget> tes = activeTEs();
  for (TEWidget *_te = tes.first(); _te; _te = tes.next()) {
    _te->setBidiEnabled(b_bidiEnabled);
    _te->repaint();
  }
}

//////////////////////////////////////////////////////////////////////

SizeDialog::SizeDialog(const unsigned int columns,
                       const unsigned int lines,
                       QWidget *parent)
  : KDialog( parent )
{
  setCaption( i18n("Size Configuration") );
  setButtons( Help | Default | Ok | Cancel );

  QFrame *mainFrame = new QFrame;
  setMainWidget( mainFrame );

  QHBoxLayout *hb = new QHBoxLayout(mainFrame);

  m_columns = new QSpinBox(mainFrame);
  m_columns->setRange(20,1000);
  m_columns->setSingleStep(1);
  m_columns->setValue(columns);

  m_lines = new QSpinBox(mainFrame);
  m_lines->setRange(4,1000);
  m_lines->setSingleStep(1);
  m_lines->setValue(lines);

  hb->addWidget(new QLabel(i18n("Number of columns:"), mainFrame));
  hb->addWidget(m_columns);
  hb->addSpacing(10);
  hb->addWidget(new QLabel(i18n("Number of lines:"), mainFrame));
  hb->addWidget(m_lines);
  connect(this, SIGNAL(defaultClicked()), SLOT(slotDefault()));
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

///////////////////////////////////////////////////////////
// This was to apply changes made to KControl fixed font to all TEs...
//  kvh - 03/10/2005 - We don't do this anymore...
void Konsole::slotFontChanged()
{
  TEWidget *oldTe = te;
  Q3PtrList<TEWidget> tes = activeTEs();
  for (TEWidget *_te = tes.first(); _te; _te = tes.next()) {
    te = _te;
//    setFont(n_font);
  }
  te = oldTe;
}

void Konsole::setSessionManager(SessionManager* manager)
{
    _sessionManager = manager;
}
SessionManager* Konsole::sessionManager()
{
    return _sessionManager;
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

void Konsole::enableFullScripting(bool b)
{
    assert(!(b_fullScripting && !b) && "fullScripting can't be disabled");
    if (!b_fullScripting && b)
        (void)new KonsoleScriptingAdaptor(this);
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

void Konsole::showEvent( QShowEvent* /*event*/ )
{
// SPLIT-VIEW Disabled
//	assert( tabwidget && tabwidget->currentWidget() );
//	tabwidget->currentWidget()->setFocus();
}

Q3PtrList<TEWidget> Konsole::activeTEs()
{
   Q3PtrList<TEWidget> ret;
 
 /*
  SPLIT-VIEW Disabled

  if (sessions.count()>0)
     for (TESession *_se = sessions.first(); _se; _se = sessions.next())
        ret.append(_se->widget());
   else if (te)  // check for startup initialization case in newSession()
     ret.append(te); */
   
   return ret;
}

void Konsole::setupTabContextMenu()
{
/* SPLIT-VIEW Disabled
 
   m_tabPopupMenu = new KMenu( this );
   KAcceleratorManager::manage( m_tabPopupMenu );

   m_tabDetachSession= new KAction( i18n("&Detach Session"), actionCollection(), 0 );
   m_tabDetachSession->setIcon( KIcon("tab_breakoff") );
   connect( m_tabDetachSession, SIGNAL( triggered() ), this, SLOT(slotTabDetachSession()) );
   m_tabPopupMenu->addAction( m_tabDetachSession );

   m_tabPopupMenu->addAction( i18n("&Rename Session..."), this,
                         SLOT(slotTabRenameSession()) );
   m_tabPopupMenu->addSeparator();

   m_tabMonitorActivity = new KToggleAction ( i18n( "Monitor for &Activity" ), actionCollection(), "" );
   m_tabMonitorActivity->setIcon( KIcon("activity") );
   connect( m_tabMonitorActivity, SIGNAL( triggered() ), this, SLOT( slotTabToggleMonitor() ) );
   m_tabMonitorActivity->setCheckedState( KGuiItem( i18n( "Stop Monitoring for &Activity" )) );
   m_tabPopupMenu->addAction( m_tabMonitorActivity );

   m_tabMonitorSilence = new KToggleAction ( i18n( "Monitor for &Silence" ), actionCollection(), "" );
   m_tabMonitorSilence->setIcon( KIcon("silence") );
   connect( m_tabMonitorSilence, SIGNAL( triggered() ), this, SLOT( slotTabToggleMonitor() ) );
   m_tabMonitorSilence->setCheckedState( KGuiItem( i18n( "Stop Monitoring for &Silence" ) ) );
   m_tabPopupMenu->addAction( m_tabMonitorSilence );

   m_tabMasterMode = new KToggleAction ( i18n( "Send &Input to All Sessions" ), actionCollection(), "" );
   m_tabMasterMode->setIcon( KIcon( "remote" ) );
   connect( m_tabMasterMode, SIGNAL( triggered() ), this, SLOT( slotTabToggleMasterMode() ) );
   m_tabPopupMenu->addAction( m_tabMasterMode );


   moveSessionLeftAction = new KAction( actionCollection() , "moveSessionLeftAction" );
   moveSessionLeftAction->setShortcut(Qt::CTRL+Qt::SHIFT+Qt::Key_Left);
   connect( moveSessionLeftAction , SIGNAL( triggered() ), this , SLOT(moveSessionLeft()) );

   moveSessionRightAction = new KAction( actionCollection() , "moveSessionRightAction" );
   moveSessionRightAction->setShortcut(Qt::CTRL+Qt::SHIFT+Qt::Key_Right);
   connect( moveSessionRightAction , SIGNAL( triggered() ), this , SLOT (moveSessionRight()) );

   addAction(moveSessionLeftAction);
   addAction(moveSessionRightAction);

   m_tabPopupMenu->addSeparator();
   m_tabPopupTabsMenu = new KMenu( m_tabPopupMenu );
   m_tabPopupMenu->insertItem( i18n("Switch to Tab" ), m_tabPopupTabsMenu );
   connect( m_tabPopupTabsMenu, SIGNAL( activated ( int ) ),
            SLOT( activateSession( int ) ) );

   m_tabPopupMenu->addSeparator();
   m_tabPopupMenu->addAction( SmallIcon("fileclose"), i18n("C&lose Session"), this,
                          SLOT(slotTabCloseSession()) );
*/
}

#include "konsole.moc"

/*
    This file is part of the KDE system
    Copyright (C)  1999,2000 Boloni Laszlo

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

    konsole_part.cpp
 */

#include "konsole_part.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <assert.h>

#include <qfile.h>
#include <qlayout.h>
#include <qwmatrix.h>
#include <qregexp.h>

#include <kaboutdata.h>
#include <kdebug.h>
#include <kfontdialog.h>
#include <kglobalsettings.h>
#include <kiconloader.h>
#include <kinputdialog.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <krun.h>
#include <kstdaction.h>
#include <qlabel.h>

#include <qcheckbox.h>
#include <qspinbox.h>
#include <qpushbutton.h>
#include <kpopupmenu.h>
#include <krootpixmap.h>
#include <kconfig.h>
#include <kaction.h>

// We can't use the ARGB32 visual when embedded in another application
bool argb_visual = false;

extern "C"
{
  /**
   * This function is the 'main' function of this part.  It takes
   * the form 'void *init_lib<library name>()  It always returns a
   * new factory object
   */
  KDE_EXPORT void *init_libkonsolepart()
  {
    return new konsoleFactory;
  }
}

/**
 * We need one static instance of the factory for our C 'main' function
 */
KInstance *konsoleFactory::s_instance = 0L;
KAboutData *konsoleFactory::s_aboutData = 0;

konsoleFactory::konsoleFactory()
{
}

konsoleFactory::~konsoleFactory()
{
  if (s_instance)
    delete s_instance;

  if ( s_aboutData )
    delete s_aboutData;

  s_instance = 0;
  s_aboutData = 0;
}

KParts::Part *konsoleFactory::createPartObject(QWidget *parentWidget, const char *widgetName,
                                         QObject *parent, const char *name, const char *classname,
                                         const QStringList&)
{
  kdDebug(1211) << "konsoleFactory::createPart parentWidget=" << parentWidget << " parent=" << parent << endl;
  KParts::Part *obj = new konsolePart(parentWidget, widgetName, parent, name, classname);
  return obj;
}

KInstance *konsoleFactory::instance()
{
  if ( !s_instance )
    {
      s_aboutData = new KAboutData("konsole", I18N_NOOP("Konsole"), "1.1");
      s_instance = new KInstance( s_aboutData );
    }
  return s_instance;
}

//////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////

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

konsolePart::konsolePart(QWidget *_parentWidget, const char *widgetName, QObject *parent, const char *name, const char *classname)
  : KParts::ReadOnlyPart(parent, name)
,te(0)
,se(0)
,colors(0)
,rootxpm(0)
,blinkingCursor(0)
,showFrame(0)
,selectBell(0)
,selectFont(0)
,selectLineSpacing(0)
,selectScrollbar(0)
,m_keytab(0)
,m_schema(0)
,m_signals(0)
,m_options(0)
,m_popupMenu(0)
,m_histSize(DEFAULT_HISTORY_SIZE)
,m_runningShell( false )
{
  parentWidget=_parentWidget;
  setInstance(konsoleFactory::instance());

  m_extension = new konsoleBrowserExtension(this);

  // This is needed since only konsole.cpp does it
  // Without this -> crash on keypress... (David)
  KeyTrans::loadAll();

  m_streamEnabled = ( classname && strcmp( classname, "TerminalEmulator" ) == 0 );

  QStrList eargs;


  const char* shell = getenv("SHELL");
  if (shell == NULL || *shell == '\0') shell = "/bin/sh";
  eargs.append(shell);
  te = new TEWidget(parentWidget,widgetName);
  te->setMinimumSize(150,70);    // allow resizing, cause resize in TEWidget

  setWidget(te);
  te->setFocus();
  connect( te,SIGNAL(configureRequest(TEWidget*,int,int,int)),
           this,SLOT(configureRequest(TEWidget*,int,int,int)) );

  colors = new ColorSchemaList();
  colors->checkSchemas();
  colors->sort();

  readProperties();

  makeGUI();

  if (m_schema)
  {
     updateSchemaMenu();

     ColorSchema *sch=colors->find(s_schema);
     if (sch)
        curr_schema=sch->numb();
     else
        curr_schema = 0;

     for (uint i=0; i<m_schema->count(); i++)
        m_schema->setItemChecked(i,false);

     m_schema->setItemChecked(curr_schema,true);
  }

  // insert keymaps into menu
  if (m_keytab)
  {
     m_keytab->clear();

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
  }

  applySettingsToGUI();

  // kDebugInfo("Loading successful");

  QTimer::singleShot( 0, this, SLOT( showShell() ) );

}

void konsolePart::doneSession(TESession*)
{
  // see doneSession in konsole.cpp
  if (se)
  {
    kdDebug(1211) << "doneSession - disconnecting done" << endl;
    disconnect( se,SIGNAL(done(TESession*)),
                this,SLOT(doneSession(TESession*)) );
    se->setConnect(false);
    //QTimer::singleShot(100,se,SLOT(terminate()));
    kdDebug(1211) << "se->terminate()" << endl;
    se->terminate();
  }
}

void konsolePart::sessionDestroyed()
{
  kdDebug(1211) << "sessionDestroyed()" << endl;
  disconnect( se, SIGNAL( destroyed() ), this, SLOT( sessionDestroyed() ) );
  se = 0;
  delete this;
}

void konsolePart::configureRequest(TEWidget*_te,int,int x,int y)
{
  if (m_popupMenu)
     m_popupMenu->popup(_te->mapToGlobal(QPoint(x,y)));
}

konsolePart::~konsolePart()
{
  kdDebug(1211) << "konsolePart::~konsolePart() this=" << this << endl;
  if ( se ) {
    disconnect( se, SIGNAL( destroyed() ), this, SLOT( sessionDestroyed() ) );
    kdDebug(1211) << "Deleting se session" << endl;
    delete se;
    se=0;
  }

  if (colors) delete colors;
  colors=0;

  //te is deleted by the framework
}

bool konsolePart::openURL( const KURL & url )
{
  //kdDebug(1211) << "konsolePart::openURL " << url.prettyURL() << endl;

  if (currentURL==url) {
    emit completed();
    return true;
  }

  m_url = url;
  emit setWindowCaption( url.prettyURL() );
  kdDebug(1211) << "Set Window Caption to " << url.prettyURL() << "\n";
  emit started( 0 );

  if ( url.isLocalFile() ) {
      struct stat buff;
      stat( QFile::encodeName( url.path() ), &buff );
      QString text = ( S_ISDIR( buff.st_mode ) ? url.path() : url.directory() );
      showShellInDir( text );
  }

  emit completed();
  return true;
}

void konsolePart::emitOpenURLRequest(const QString &cwd)
{
  KURL url;
  url.setPath(cwd);
  if (url==currentURL)
    return;
  currentURL=url;
  m_extension->emitOpenURLRequest(url);
}


void konsolePart::makeGUI()
{
  if (!kapp->authorizeKAction("konsole_rmb"))
     return;

  KActionCollection* actions = new KActionCollection(this);

  // Send Signal Menu -------------------------------------------------------------
  if (kapp->authorizeKAction("send_signal"))
  {
     m_signals = new KPopupMenu((KMainWindow*)parentWidget);
     m_signals->insertItem( i18n( "&Suspend Task" )   + " (STOP)", SIGSTOP);
     m_signals->insertItem( i18n( "&Continue Task" )  + " (CONT)", SIGCONT);
     m_signals->insertItem( i18n( "&Hangup" )         + " (HUP)",   SIGHUP);
     m_signals->insertItem( i18n( "&Interrupt Task" ) + " (INT)",   SIGINT);
     m_signals->insertItem( i18n( "&Terminate Task" ) + " (TERM)", SIGTERM);
     m_signals->insertItem( i18n( "&Kill Task" )      + " (KILL)",  SIGKILL);
     m_signals->insertItem( i18n( "User Signal &1")   + " (USR1)", SIGUSR1);
     m_signals->insertItem( i18n( "User Signal &2")   + " (USR2)", SIGUSR2);
     connect(m_signals, SIGNAL(activated(int)), SLOT(sendSignal(int)));
  }

  // Settings Menu ----------------------------------------------------------------
  if (kapp->authorizeKAction("settings"))
  {
     m_options = new KPopupMenu((KMainWindow*)parentWidget);

     // Scrollbar
     selectScrollbar = new KSelectAction(i18n("Sc&rollbar"), 0, this,
                                      SLOT(slotSelectScrollbar()), actions);

     QStringList scrollitems;
     scrollitems << i18n("&Hide") << i18n("&Left") << i18n("&Right");
     selectScrollbar->setItems(scrollitems);
     selectScrollbar->plug(m_options);

     // Select Bell
     m_options->insertSeparator();
     selectBell = new KSelectAction(i18n("&Bell"), SmallIconSet( "bell"), 0 , this,
                                 SLOT(slotSelectBell()), actions, "bell");

     QStringList bellitems;
     bellitems << i18n("System &Bell")
            << i18n("System &Notification")
            << i18n("&Visible Bell")
            << i18n("N&one");
     selectBell->setItems(bellitems);
     selectBell->plug(m_options);

     // Select font
     selectFont = new KonsoleFontSelectAction( i18n( "&Font" ), SmallIconSet( "text" ), 0,
                                            this, SLOT(slotSelectFont()), actions, "font");
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

     // Keyboard Options Menu ---------------------------------------------------
     if (kapp->authorizeKAction("keyboard"))
     {
        m_keytab = new KPopupMenu((KMainWindow*)parentWidget);
        m_keytab->setCheckable(true);
        connect(m_keytab, SIGNAL(activated(int)), SLOT(keytab_menu_activated(int)));
        m_options->insertItem( SmallIconSet( "key_bindings" ), i18n( "&Keyboard" ), m_keytab );
     }

     // Schema Options Menu -----------------------------------------------------
     if (kapp->authorizeKAction("schema"))
     {
        m_schema = new KPopupMenu((KMainWindow*)parentWidget);
        m_schema->setCheckable(true);
        connect(m_schema, SIGNAL(activated(int)), SLOT(schema_menu_activated(int)));
        connect(m_schema, SIGNAL(aboutToShow()), SLOT(schema_menu_check()));
        m_options->insertItem( SmallIconSet( "colorize" ), i18n( "Sch&ema" ), m_schema);
     }


     KAction *historyType = new KAction(i18n("&History..."), "history", 0, this,
                                        SLOT(slotHistoryType()), actions, "history");
     historyType->plug(m_options);
     m_options->insertSeparator();

     // Select line spacing
     selectLineSpacing =
       new KSelectAction
       (
        i18n("Li&ne Spacing"),
        SmallIconSet("leftjust"), // Non-code hack.
        0,
        this,
        SLOT(slotSelectLineSpacing()),
        this
       );

     QStringList lineSpacingList;

     lineSpacingList
       << i18n("&0")
       << i18n("&1")
       << i18n("&2")
       << i18n("&3")
       << i18n("&4")
       << i18n("&5")
       << i18n("&6")
       << i18n("&7")
       << i18n("&8");

     selectLineSpacing->setItems(lineSpacingList);
     selectLineSpacing->plug(m_options);

     // Blinking Cursor
     blinkingCursor = new KToggleAction (i18n("Blinking &Cursor"),
                                      0, this,SLOT(slotBlinkingCursor()), actions);
     blinkingCursor->plug(m_options);

     // Frame on/off
     showFrame = new KToggleAction(i18n("Show Fr&ame"), 0,
                                this, SLOT(slotToggleFrame()), actions);
     showFrame->setCheckedState(i18n("Hide Fr&ame"));
     showFrame->plug(m_options);

     // Word Connectors
     KAction *WordSeps = new KAction(i18n("Wor&d Connectors..."), 0, this,
                                  SLOT(slotWordSeps()), actions);
     WordSeps->plug(m_options);

     // Save Settings
     m_options->insertSeparator();
     KAction *saveSettings = new KAction(i18n("&Save as Default"), "filesave", 0, this, 
                    SLOT(saveProperties()), actions, "save_default");
     saveSettings->plug(m_options);
     if (KGlobalSettings::insertTearOffHandle())
        m_options->insertTearOffHandle();
  }

  // Popup Menu -------------------------------------------------------------------
  m_popupMenu = new KPopupMenu((KMainWindow*)parentWidget);
  KAction* selectionEnd = new KAction(i18n("Set Selection End"), 0, te,
                               SLOT(setSelectionEnd()), actions, "selection_end");
  selectionEnd->plug(m_popupMenu);

  KAction *copyClipboard = new KAction(i18n("&Copy"), "editcopy", 0,
                                        te, SLOT(copyClipboard()), actions, "edit_copy");
  copyClipboard->plug(m_popupMenu);

  KAction *pasteClipboard = new KAction(i18n("&Paste"), "editpaste", 0,
                                        te, SLOT(pasteClipboard()), actions, "edit_paste");
  pasteClipboard->plug(m_popupMenu);

  if (m_signals)
  {
     m_popupMenu->insertItem(i18n("&Send Signal"), m_signals);
     m_popupMenu->insertSeparator();
  }

  if (m_options)
  {
     m_popupMenu->insertItem(i18n("S&ettings"), m_options);
     m_popupMenu->insertSeparator();
  }

  KAction *closeSession = new KAction(i18n("&Close Terminal Emulator"), "fileclose", 0, this,
                                      SLOT(closeCurrentSession()), actions, "close_session");
  closeSession->plug(m_popupMenu);
  if (KGlobalSettings::insertTearOffHandle())
    m_popupMenu->insertTearOffHandle();
}

void konsolePart::applySettingsToGUI()
{
  if (showFrame)
     showFrame->setChecked( b_framevis );
  if (selectScrollbar)
     selectScrollbar->setCurrentItem(n_scroll);
  if (selectFont)
     selectFont->setCurrentItem(n_font);
  updateKeytabMenu();
  if (selectBell)
     selectBell->setCurrentItem(n_bell);
  if (selectLineSpacing)
     selectLineSpacing->setCurrentItem(te->lineSpacing());
  if (blinkingCursor)
     blinkingCursor->setChecked(te->blinkingCursor());
  if (m_schema)
     m_schema->setItemChecked(curr_schema,true);
}

void konsolePart::readProperties()
{
  KConfig* config = new KConfig("konsolepartrc",true);
  config->setDesktopGroup();

  b_framevis = config->readBoolEntry("has frame",false);
  b_histEnabled = config->readBoolEntry("historyenabled",true);
  n_bell = QMIN(config->readUnsignedNumEntry("bellmode",TEWidget::BELLSYSTEM),3);
  n_font = QMIN(config->readUnsignedNumEntry("font",3),TOPFONT);
  n_keytab=config->readNumEntry("keytab",0); // act. the keytab for this session
  n_scroll = QMIN(config->readUnsignedNumEntry("scrollbar",TEWidget::SCRRIGHT),2);
  m_histSize = config->readNumEntry("history",DEFAULT_HISTORY_SIZE);
  s_word_seps= config->readEntry("wordseps",":@-./_~");

  QFont tmpFont("fixed");
  tmpFont.setFixedPitch(true);
  tmpFont.setStyleHint(QFont::TypeWriter);
  defaultFont = config->readFontEntry("defaultfont", &tmpFont);
  setFont(QMIN(config->readUnsignedNumEntry("font",3),TOPFONT));

  QString schema = config->readEntry("Schema");

  s_kconfigSchema=config->readEntry("schema");
  ColorSchema* sch = colors->find(schema.isEmpty() ? s_kconfigSchema : schema);
  if (!sch) {
    sch=(ColorSchema*)colors->at(0);  //the default one
  }
  if (sch->hasSchemaFileChanged()) sch->rereadSchemaFile();
  s_schema = sch->relPath();
  curr_schema = sch->numb();
  pmPath = sch->imagePath();
  te->setColorTable(sch->table()); //FIXME: set twice here to work around a bug

  if (sch->useTransparency()) {
    //KONSOLEDEBUG << "Setting up transparency" << endl;
    if (!rootxpm)
      rootxpm = new KRootPixmap(te);
    rootxpm->setFadeEffect(sch->tr_x(), QColor(sch->tr_r(), sch->tr_g(), sch->tr_b()));
    rootxpm->start();
    rootxpm->repaint(true);
  }
  else {
    if (rootxpm) {
      rootxpm->stop();
      delete rootxpm;
      rootxpm=0;
    }
    pixmap_menu_activated(sch->alignment());
  }

  te->setBellMode(n_bell);
  te->setBlinkingCursor(config->readBoolEntry("BlinkingCursor",false));
  te->setFrameStyle( b_framevis?(QFrame::WinPanel|QFrame::Sunken):QFrame::NoFrame );
  te->setLineSpacing( config->readUnsignedNumEntry( "LineSpacing", 0 ) );
  te->setScrollbarLocation(n_scroll);
  te->setWordCharacters(s_word_seps);

  delete config;

  config = new KConfig("konsolerc",true);
  config->setDesktopGroup();
  te->setTerminalSizeHint( config->readBoolEntry("TerminalSizeHint",true) );
  delete config;
}

void konsolePart::saveProperties()
{
  KConfig* config = new KConfig("konsolepartrc");
  config->setDesktopGroup();

  config->writeEntry("bellmode",n_bell);
  config->writeEntry("BlinkingCursor", te->blinkingCursor());
  config->writeEntry("defaultfont", defaultFont);
  config->writeEntry("font",n_font);
  config->writeEntry("history", se->history().getSize());
  config->writeEntry("historyenabled", b_histEnabled);
  config->writeEntry("keytab",n_keytab);
  config->writeEntry("has frame",b_framevis);
  config->writeEntry("LineSpacing", te->lineSpacing());
  config->writeEntry("schema",s_kconfigSchema);
  config->writeEntry("scrollbar",n_scroll);
  config->writeEntry("wordseps",s_word_seps);

  config->sync();
  delete config;
}

void konsolePart::sendSignal(int sn)
{
  if (se) se->sendSignal(sn);
}

void konsolePart::closeCurrentSession()
{
  if ( se ) se->closeSession();
}

void konsolePart::slotToggleFrame()
{
  b_framevis = showFrame->isChecked();
  te->setFrameStyle( b_framevis?(QFrame::WinPanel|QFrame::Sunken):QFrame::NoFrame);
}

void konsolePart::slotSelectScrollbar()
{
  if ( ! se ) return;
  n_scroll = selectScrollbar->currentItem();
  te->setScrollbarLocation(n_scroll);
}

void konsolePart::slotSelectFont() {
  int item = selectFont->currentItem();
  if( item > 9 ) // compensate for the two separators
      --item;
  if( item > 6 )
      --item;
  // KONSOLEDEBUG << "slotSelectFont " << item << endl;
  if (item == DEFAULTFONT) {
    if ( KFontDialog::getFont(defaultFont, true) == QDialog::Rejected ) {
      selectFont->setCurrentItem(n_font);
      return;
    }
  }
  setFont(item);
}

void konsolePart::setFont(int fontno)
{
  if ( ! se ) return;
  QFont f;
  if (fontno == DEFAULTFONT)
    f = defaultFont;
  else
    if (fonts[fontno][0] == '-')
    {
      f.setRawName( fonts[fontno] );
      f.setFixedPitch(true);
      f.setStyleHint( QFont::TypeWriter );
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
      f.setStyleHint(QFont::TypeWriter);
      f.setPixelSize(QString(fonts[fontno]).toInt());
    }


  se->setFontNo(fontno);
  te->setVTFont(f);
  n_font = fontno;
}

void konsolePart::fontNotFound()
{
  QString msg = i18n("Font `%1' not found.\nCheck README.linux.console for help.").arg(fontNotFound_par);
  KMessageBox::information(parentWidget,  msg, i18n("Font Not Found"), QString("font_not_found_%1").arg(fontNotFound_par));
}

void konsolePart::updateKeytabMenu()
{
  if ( se && m_keytab ) {
    m_keytab->setItemChecked(n_keytab,false);
    m_keytab->setItemChecked(se->keymapNo(),true);
    n_keytab = se->keymapNo();
  } else if ( m_keytab ) {    // no se yet, happens at startup
    m_keytab->setItemChecked(n_keytab, true);
  }
}

void konsolePart::keytab_menu_activated(int item)
{
  if ( ! se ) return;
  se->setKeymapNo(item);
  updateKeytabMenu();
}

void konsolePart::schema_menu_activated(int item)
{
  setSchema(item);
  s_kconfigSchema = s_schema; // This is the new default
}

void konsolePart::schema_menu_check()
{
  if (colors->checkSchemas()) {
    colors->sort();
    updateSchemaMenu();
  }
}

void konsolePart::updateSchemaMenu()
{
  if (!m_schema) return;

  m_schema->clear();
  for (int i = 0; i < (int) colors->count(); i++)  {
    ColorSchema* s = (ColorSchema*)colors->at(i);
    QString title=s->title();
    m_schema->insertItem(title.replace('&',"&&"),s->numb(),0);
  }

  if (te && se) {
    m_schema->setItemChecked(se->schemaNo(),true);
  }
}

void konsolePart::setSchema(int numb)
{
  ColorSchema* s = colors->find(numb);
  if (!s) {
    kdWarning() << "No schema found. Using default." << endl;
    s=(ColorSchema*)colors->at(0);
  }
  if (s->numb() != numb)  {
    kdWarning() << "No schema with number " << numb << endl;
  }

  if (s->hasSchemaFileChanged()) {
    const_cast<ColorSchema *>(s)->rereadSchemaFile();
  }
  if (s) setSchema(s);
}

void konsolePart::setSchema(ColorSchema* s)
{
  if (!se) return;
  if (!s) return;

  if (m_schema) {
    m_schema->setItemChecked(curr_schema,false);
    m_schema->setItemChecked(s->numb(),true);
  }

  s_schema = s->relPath();
  curr_schema = s->numb();
  pmPath = s->imagePath();
  te->setColorTable(s->table()); //FIXME: set twice here to work around a bug

  if (s->useTransparency()) {
    if (!rootxpm)
      rootxpm = new KRootPixmap(te);
    rootxpm->setFadeEffect(s->tr_x(), QColor(s->tr_r(), s->tr_g(), s->tr_b()));
    rootxpm->start();
    rootxpm->repaint(true);
  }
  else {
    if (rootxpm) {
      rootxpm->stop();
      delete rootxpm;
      rootxpm=0;
    }
    pixmap_menu_activated(s->alignment());
  }

  te->setColorTable(s->table());
  se->setSchemaNo(s->numb());
}

void konsolePart::notifySize(int /* lines */, int /* columns */)
{
  ColorSchema *sch=colors->find(s_schema);

  if (sch && sch->alignment() >= 3)
    pixmap_menu_activated(sch->alignment());
}

void konsolePart::pixmap_menu_activated(int item)
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
  switch (item) {
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

void konsolePart::slotHistoryType()
{
  if ( ! se ) return;
  HistoryTypeDialog dlg(se->history(), m_histSize, (KMainWindow*)parentWidget);
  if (dlg.exec()) {
    if (dlg.isOn()) {
      if (dlg.nbLines() > 0) {
        se->setHistory(HistoryTypeBuffer(dlg.nbLines()));
        m_histSize = dlg.nbLines();
        b_histEnabled = true;
      }
      else {
        se->setHistory(HistoryTypeFile());
        m_histSize = 0;
        b_histEnabled = true;
      }
    }
    else {
      se->setHistory(HistoryTypeNone());
      m_histSize = dlg.nbLines();
      b_histEnabled = false;
    }
  }
}

void konsolePart::slotSelectBell() {
  n_bell = selectBell->currentItem();
  te->setBellMode(n_bell);
}

void konsolePart::slotSelectLineSpacing()
{
  te->setLineSpacing( selectLineSpacing->currentItem() );
}

void konsolePart::slotBlinkingCursor()
{
  te->setBlinkingCursor(blinkingCursor->isChecked());
}

void konsolePart::slotWordSeps() {
  bool ok;

  QString seps = KInputDialog::getText( i18n( "Word Connectors" ),
      i18n( "Characters other than alphanumerics considered part of a word when double clicking:" ), s_word_seps, &ok, parentWidget );
  if ( ok )
  {
    s_word_seps = seps;
    te->setWordCharacters(s_word_seps);
  }
}

void konsolePart::enableMasterModeConnections()
{
  if ( se ) se->setListenToKeyPress(true);
}

void konsolePart::updateTitle()
{
  if ( se ) emit setWindowCaption( se->fullTitle() );
}

void konsolePart::guiActivateEvent( KParts::GUIActivateEvent * )
{
    // Don't let ReadOnlyPart::guiActivateEvent reset the window caption
}

bool konsolePart::doOpenStream( const QString& )
{
	return m_streamEnabled;
}

bool konsolePart::doWriteStream( const QByteArray& data )
{
	if ( m_streamEnabled )
	{
		QString cmd = QString::fromLocal8Bit( data.data(), data.size() );
		se->sendSession( cmd );
		return true;
	}
	return false;
}

bool konsolePart::doCloseStream()
{
	return m_streamEnabled;
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

  m_setUnlimited = new QPushButton(i18n("&Set Unlimited"), mainFrame);
  connect( m_setUnlimited,SIGNAL(clicked()), this,SLOT(slotSetUnlimited()) );

  hb->addWidget(m_btnEnable);
  hb->addSpacing(10);
  hb->addWidget(new QLabel(i18n("Number of lines:"), mainFrame));
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

konsoleBrowserExtension::konsoleBrowserExtension(konsolePart *parent)
  : KParts::BrowserExtension(parent, "konsoleBrowserExtension")
{
}

konsoleBrowserExtension::~konsoleBrowserExtension()
{
}

void konsoleBrowserExtension::emitOpenURLRequest(const KURL &url)
{
  emit openURLRequest(url);
}

const char* sensibleShell()
{
  const char* shell = getenv("SHELL");
  if (shell == NULL || *shell == '\0') shell = "/bin/sh";
  return shell;
}

void konsolePart::startProgram( const QString& program,
                                const QStrList& args )
{
  if ( se ) delete se;
  se = new TESession(te, program, args, "xterm", parentWidget->winId());
  connect( se,SIGNAL(done(TESession*)),
           this,SLOT(doneSession(TESession*)) );
  connect( se,SIGNAL(openURLRequest(const QString &)),
           this,SLOT(emitOpenURLRequest(const QString &)) );
  connect( se, SIGNAL( updateTitle() ),
           this, SLOT( updateTitle() ) );
  connect( se, SIGNAL(enableMasterModeConnections()),
           this, SLOT(enableMasterModeConnections()) );
  connect( se, SIGNAL( processExited() ),
           this, SLOT( slotProcessExited() ) );
  connect( se, SIGNAL( receivedData( const QString& ) ),
           this, SLOT( slotReceivedData( const QString& ) ) );

  // We ignore the following signals
  //connect( se, SIGNAL(renameSession(TESession*,const QString&)),
  //         this, SLOT(slotRenameSession(TESession*, const QString&)) );
  //connect( se->getEmulation(), SIGNAL(changeColumns(int)),
  //         this, SLOT(changeColumns(int)) );
  //connect( se, SIGNAL(disableMasterModeConnections()),
  //        this, SLOT(disableMasterModeConnections()) );

  if (b_histEnabled && m_histSize)
    se->setHistory(HistoryTypeBuffer(m_histSize));
  else if (b_histEnabled && !m_histSize)
    se->setHistory(HistoryTypeFile());
  else
    se->setHistory(HistoryTypeNone());
  se->setKeymapNo(n_keytab);
  KConfig* config = new KConfig("konsolerc",true);
  config->setGroup("UTMP");
  se->setAddToUtmp( config->readBoolEntry("AddToUtmp",true));
  delete config;

  se->setConnect(true);
  se->setSchemaNo(curr_schema);
  se->run();
  connect( se, SIGNAL( destroyed() ), this, SLOT( sessionDestroyed() ) );
  setFont( n_font ); // we do this here, to make TEWidget recalculate
                     // its geometry..
  te->emitText( QString::fromLatin1( "\014" ) );
}

void konsolePart::showShellInDir( const QString& dir )
{
  if ( ! m_runningShell )
  {
    const char* s = sensibleShell();
    QStrList args;
    args.append( s );
    startProgram( s, args );
    m_runningShell = true;
  };

  if ( ! dir.isNull() )
  {
      QString text = dir;
      KRun::shellQuote(text);
      text = QString::fromLatin1("cd ") + text + '\n';
      te->emitText( text );
  };
}

void konsolePart::showShell()
{
    if ( ! se ) showShellInDir( QString::null );
}

void konsolePart::sendInput( const QString& text )
{
    te->emitText( text );
}

void konsolePart::slotProcessExited()
{
    emit processExited();
}
void konsolePart::slotReceivedData( const QString& s )
{
    emit receivedData( s );
}

#include "konsole_part.moc"

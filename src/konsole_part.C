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

    konsole_part.h
 */

#include "konsole_part.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <kinstance.h>
#include <klocale.h>
#include <krun.h>
#include <kaboutdata.h>
#include <kaction.h>
#include <kparts/partmanager.h>
#include <kiconloader.h>
#include <kdebug.h>
#include <qlabel.h>
#include <qfile.h>
#include <qsplitter.h>
#include <kglobalsettings.h>

#include <qdom.h>

extern "C"
{
  /**
   * This function is the 'main' function of this part.  It takes
   * the form 'void *init_lib<library name>()  It always returns a
   * new factory object
   */
  void *init_libkonsolepart()
  {
      kdDebug(1211) << "Konsole in actions!!!" << endl;
    return new konsoleFactory;
  }
};

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

KParts::Part *konsoleFactory::createPart(QWidget *parentWidget, const char *widgetName,
                                         QObject *parent, const char *name, const char*,
                                         const QStringList& )
{
  kdDebug(1211) << "konsoleFactory::createPart parentWidget=" << parentWidget << " parent=" << parent << endl;
  KParts::Part *obj = new konsolePart(parentWidget, widgetName, parent, name);
  emit objectCreated(obj);
  return obj;
}

KInstance *konsoleFactory::instance()
{
  if ( !s_instance )
    {
      s_aboutData = new KAboutData("konsole", I18N_NOOP("Konsole"), "1.0");
      s_instance = new KInstance( s_aboutData );
    }
  return s_instance;
}

konsolePart::konsolePart(QWidget *parentWidget, const char *widgetName, QObject *parent, const char *name)
  : KParts::ReadOnlyPart(parent, name)
{
  setInstance(konsoleFactory::instance());

  // This is needed since only konsole.C does it
  // Without those two -> crash on keypress... (David)
  KeyTrans::loadAll();

  // create a canvas to insert our widget

  m_extension = new konsoleBrowserExtension(this);

  QStrList eargs;

  const char* shell = getenv("SHELL");
  if (shell == NULL || *shell == '\0') shell = "/bin/sh";
  eargs.append(shell);
  te = new TEWidget(parentWidget,widgetName);
  te->setMinimumSize(150,70);    // allow resizing, cause resize in TEWidget
  te->setScrollbarLocation(TEWidget::SCRRIGHT);
  setWidget(te);
  // faking a KMainwindow - TESession assumes that (wrong design!)
  initial = new TESession((KMainWindow*)parentWidget,
                          te,shell,eargs,"xterm");
  connect( initial,SIGNAL(done(TESession*,int)),
           this,SLOT(doneSession(TESession*,int)) );
  connect( te,SIGNAL(configureRequest(TEWidget*,int,int,int)),
           this,SLOT(configureRequest(TEWidget*,int,int,int)) );
  initial->setConnect(TRUE);
  te->currentSession = initial;

  // At least set the font to the default, if we can't read in
  // the settings
  te->setVTFont(KGlobalSettings::fixedFont());
 
  // Also set some usefull colors
  ColorEntry ctable[TABLE_COLORS];
  memcpy(ctable, te->getColorTable(), sizeof(ctable));
  if (ctable)
    {
      ctable[DEFAULT_BACK_COLOR].color = KGlobalSettings::baseColor();
      ctable[DEFAULT_FORE_COLOR].color = KGlobalSettings::textColor();
      te->setColorTable(ctable); 
    }
  
  initial->setHistory(HistoryTypeBlockArray(1000));

  // setXMLFile("konsole_part.rc");

  // kDebugInfo("Loading successful");
  // kDebugInfo("XML file set");

  initial->run();

  connect( initial, SIGNAL( destroyed() ), this, SLOT( sessionDestroyed() ) );
}

void konsolePart::doneSession(TESession*,int)
{
  // see doneSession in konsole.C
  if (initial)
  {
    kdDebug(1211) << "doneSession - disconnecting done" << endl;;
    disconnect( initial,SIGNAL(done(TESession*,int)),
                this,SLOT(doneSession(TESession*,int)) );
    initial->setConnect(FALSE);
    //QTimer::singleShot(100,initial,SLOT(terminate()));
    kdDebug(1211) << "initial->terminate()" << endl;;
    initial->terminate();
  }
}

void konsolePart::sessionDestroyed()
{
  kdDebug(1211) << "sessionDestroyed()" << endl;;
  disconnect( initial, SIGNAL( destroyed() ), this, SLOT( sessionDestroyed() ) );
  initial = 0;
  delete this;
}

void konsolePart::configureRequest(TEWidget*te,int,int x,int y)
{
    emit m_extension->popupMenu( te->mapToGlobal(QPoint(x,y)), m_url, "inode/directory", S_IFDIR );
}

void konsolePart::slotNew() {
    kdDebug(1211) << "slotNew called\n";
}

void konsolePart::slotSaveFile() {
    kdDebug(1211) << "slotSaveFile called\n";
}

void konsolePart::slotLoadFile() {
    kdDebug(1211) << "slotLoadFile called\n";
}

konsolePart::~konsolePart()
{
  kdDebug(1211) << "konsolePart::~konsolePart() this=" << this << endl;
  if ( initial )
  {
    disconnect( initial, SIGNAL( destroyed() ), this, SLOT( sessionDestroyed() ) );
    kdDebug(1211) << "Deleting initial session" << endl;
    delete initial;
  }
  //te is deleted by the framework
}

bool konsolePart::openURL( const KURL & url )
{
  m_url = url;
  emit setWindowCaption( url.prettyURL() );
  kdDebug(1211) << "Set Window Caption to " << url.prettyURL() << "\n";
  emit started( 0 );

  if ( url.isLocalFile() )
  {
      struct stat buff;
      stat( QFile::encodeName( url.path() ), &buff );
      QString text = ( S_ISDIR( buff.st_mode ) ? url.path() : url.directory() );
#if 0
      text.replace(QRegExp("\\"), "\\\\"); // escape existing '\' first
      text.replace(QRegExp(" "), "\\ "); // escape spaces
      text.replace(QRegExp("("), "\\("); // and '('
      text.replace(QRegExp(")"), "\\)"); // and ')'
      // misses ampersand etc.
#endif
      KRun::shellQuote(text);
      text = QString::fromLatin1("cd ") + text + '\n';
      QKeyEvent e(QEvent::KeyPress, 0,-1,0, text);
      initial->getEmulation()->onKeyPress(&e);
  }

  emit completed();
  return true;

  //  widget->setText(m_file);
  /*
  QFile f(m_file);
  f.open(IO_ReadOnly);
  QDomDocument *doc = new QDomDocument(&f);
  kDebugInfo("This simple stuff kills it!");
  */

  /*
  if (!gofw->ds->loadXML(m_file))
      return false;
  gofw->dv.center = NULL; // FIXME: this is a hack
  gofw->refresh();
    gofw->detail->setValues(gofw->dv.center->val);
  //  return true;
  kDebugInfo("Loading successful");
  */
  return true;
}

bool konsolePart::closeURL()
{
  return true;
}

konsoleBrowserExtension::konsoleBrowserExtension(konsolePart *parent)
  : KParts::BrowserExtension(parent, "konsoleBrowserExtension")
{
}

konsoleBrowserExtension::~konsoleBrowserExtension()
{
}

#include "konsole_part.moc"

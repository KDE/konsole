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

#include <kinstance.h>
#include <klocale.h>
#include <kaboutdata.h>
#include <kaction.h>
#include <kparts/partmanager.h>
#include <kiconloader.h>
#include <kdebug.h>
#include <qlabel.h>
#include <qsplitter.h>

#include <qdom.h>


extern "C"
{
  /**
   * This function is the 'main' function of this part.  It takes
   * the form 'void *init_lib<library name>()  It always returns a
   * new factory object
   */
  void *init_libkonsole()
  {
      printf(" \n\n\nKonsole in actions!!! \n\n\n");
    return new konsoleFactory;
  }
};

/**
 * We need one static instance of the factory for our C 'main'
 * function
 */
KInstance *konsoleFactory::s_instance = 0L;

konsoleFactory::konsoleFactory()
{
}

konsoleFactory::~konsoleFactory()
{
  if (s_instance)
    delete s_instance;

  s_instance = 0;
}

QObject *konsoleFactory::create(QObject *parent, const char *name, const char*,
			       const QStringList& )
{
  QObject *obj = new konsolePart((QWidget*)parent, name);
  emit objectCreated(obj);
  return obj;
}

KInstance *konsoleFactory::instance()
{
  if ( !s_instance )
    {
      KAboutData about("konsole", I18N_NOOP("Konsole"), "1.0");
      s_instance = new KInstance(&about);
    }
  return s_instance;
}

konsolePart::konsolePart(QWidget *parent, const char *name)
  : KParts::ReadOnlyPart(parent, name)
{
  setInstance(konsoleFactory::instance());

  // create a canvas to insert our widget

  m_extension = new konsoleBrowserExtension(this);

  //bool login_shell = false;
  //bool welcome = true;
  bool histon = true;
  const char* wname = PACKAGE;

  //  QCString sz = "";
  //sz = args->getOption("vt_sz");
  //histon = args->isSet("hist");
  //wname = args->getOption("name");
  //login_shell = args->isSet("ls");
  QStrList eargs;
  //  welcome = args->isSet("welcome");

  const char* shell = getenv("SHELL");
  if (shell == NULL || *shell == '\0') shell = "/bin/sh";
  eargs.append(shell);
  te = new TEWidget(parent);
  te->setFocusPolicy(QWidget::ClickFocus);
  te->setMinimumSize(150,70);    // allow resizing, cause resize in TEWidget
  setWidget(te);
  // faking a KTMainwindow - i dont know why it has it this way
  kdDebug() << "The shell is:" << shell << "\n";
  initial = new TESession((KTMainWindow*)parent,
                          te,shell,eargs,"xterm");
  //  initial->run();
  initial->setConnect(TRUE);
  QTimer::singleShot(0/*100*/,initial,SLOT(run()));

  // setXMLFile("konsole_part.rc");

  // kDebugInfo("Loading successful");
  // kDebugInfo("XML file set");

  // This is needed since only konsole.C does it
  // Without those two -> crash on keypress... (David)
  KeyTrans::loadAll();
  initial->getEmulation()->setKeytrans(0);
}


void konsolePart::slotNew() {
  kDebugInfo("slotNew called");
}

void konsolePart::slotSaveFile() {
  kDebugInfo("slotSaveFile called");
}

void konsolePart::slotLoadFile() {
  kDebugInfo("slotLoadFile called");
}

konsolePart::~konsolePart()
{
  delete initial;
  //te is deleted by the framework
}

bool konsolePart::openURL( const KURL & url )
{
  emit setWindowCaption( url.decodedURL() );
  return true;

  // TODO: follow directory m_file

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

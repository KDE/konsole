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
    //    printf(" \n\n\nKonsole in actions!!! \n\n\n");
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
    
  QSplitter *splitter = new QSplitter(parent);
  splitter->setFocusPolicy(QWidget::ClickFocus);
  splitter->setOrientation( QSplitter::Vertical );  
  setWidget(splitter);

  
  gofw = new konsoleWidget(splitter);

  konsoleDetailWidget *detail = new konsoleDetailWidget(splitter);
  gofw->detail = detail;
   
  gofw->ds = new konsoleDataSource_Memory();
  konsoleDataQuery gq;
  gq.getView(*(gofw->ds), gofw->dv);
  gofw->dv.createVisuals(200,200,150);

  QValueList<int> splitinit;
  splitinit.append(80);
  splitinit.append(20);
  splitter->setSizes(splitinit);
  splitter->show();
  
  setXMLFile("konsole_part.rc");

  kDebugInfo("Loading successful");
  kDebugInfo("XML file set");
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

void konsolePart::slotTestDialog() {
  kDebugInfo("slotTestDialog called");
}

void konsolePart::slotDelayedLoad() {
  if (!gofw->ds->open(m_file)) {
    return;
  }
  gofw->dv.center = NULL; 
  gofw->refresh();
  gofw->detail->setValues(gofw->dv.center->val);
}

konsolePart::~konsolePart()
{
  closeURL();
}

bool konsolePart::openFile()
{
  // This is a horrible hack
    QTimer *delayedLoadTimer = new QTimer(this);    
    delayedLoadTimer->start(3,true);
    connect(delayedLoadTimer, SIGNAL(timeout()),
             this, SLOT(slotDelayedLoad()) );    
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

/*
bool konsolePart::closeURL()
{
  return true;
}
*/

konsoleBrowserExtension::konsoleBrowserExtension(konsolePart *parent)
  : KParts::BrowserExtension(parent, "konsoleBrowserExtension")
{
}

konsoleBrowserExtension::~konsoleBrowserExtension()
{
}

/*
    kwrited is a write(1) receiver for KDE.
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

// Own
#include "kwrited.h"

#include <QtCore/QSocketNotifier>
#include <QtGui/QKeyEvent>

#include <kuniqueapplication.h>
#include <kcmdlineargs.h>
#include <klocale.h>
#include <kglobalsettings.h>
#include <kdebug.h>
#include <kcrash.h>
#include <kpty.h>
#include <kuser.h>
#include <kglobal.h>


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>

#ifdef Q_WS_X11
#include <X11/Xlib.h>
#include <fixx11h.h>
#endif

#include <kpluginfactory.h>
#include <kpluginloader.h>

K_PLUGIN_FACTORY(KWritedFactory,
                 registerPlugin<KWritedModule>();
    )
K_EXPORT_PLUGIN(KWritedFactory("konsole"))


/* TODO
   for anyone who likes to do improvements here, go ahead.
   - check FIXMEs below
   - add Menu
     - accept messages (on/off)
     - pop up on incoming messages
     - clear messages
     - allow max. lines
   - add DBus interface?
   - add session awareness.
   - add client complements.
   - kwrited is disabled by default if built without utempter,
     see ../Makefile.am - kwrited doesn't seem to work well without utempter
*/

KWrited::KWrited() : QTextEdit()
{
  int pref_width, pref_height;

  setFont(KGlobalSettings::fixedFont());
  pref_width = (2 * KGlobalSettings::desktopGeometry(0).width()) / 3;
  pref_height = fontMetrics().lineSpacing() * 10;
  setMinimumWidth(pref_width);
  setMinimumHeight(pref_height);
  setReadOnly(true);
//  setFocusPolicy(QWidget::NoFocus);
//  setWordWrap(QTextEdit::WidgetWidth);
//  setTextFormat(Qt::PlainText);

  pty = new KPty();
  pty->open();
  pty->login(KUser().loginName().toLocal8Bit().data(), getenv("DISPLAY"));
  QSocketNotifier *sn = new QSocketNotifier(pty->masterFd(), QSocketNotifier::Read, this);
  connect(sn, SIGNAL(activated(int)), this, SLOT(block_in(int)));

  QString txt = i18n("KWrited - Listening on Device %1", pty->ttyName());
  setWindowTitle(txt);
  puts(txt.toLocal8Bit().data());
}

KWrited::~KWrited()
{
    pty->logout();
    delete pty;
}

void KWrited::block_in(int fd)
{
  char buf[4096];
  int len = read(fd, buf, 4096);
  if (len <= 0)
     return;

  insertPlainText( QString::fromLocal8Bit( buf, len ).remove('\r') );
  show();
  raise();
}

void KWrited::clearText()
{
   clear();
}

void KWrited::contextMenuEvent(QContextMenuEvent * e)
{
   QMenu *menu = createStandardContextMenu();
   menu->addAction("Clear Messages");
   // Add connection and shortcut if possible
   menu->exec(e->globalPos());
   delete menu;
}

KWritedModule::KWritedModule(QObject* parent, const QList<QVariant>&)
    : KDEDModule(parent)
{
    // Done by the factory now
    //KGlobal::locale()->insertCatalog("konsole");
    pro = new KWrited;
}

KWritedModule::~KWritedModule()
{
    delete pro;
    // Done by the factory now
    //KGlobal::locale()->removeCatalog("konsole");
}

#include "kwrited.moc"

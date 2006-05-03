// [kwrited.C] A write(1) receiver for kde.

#include <dcopclient.h>
#include <qsocketnotifier.h>

#include <kuniqueapplication.h>
#include <kcmdlineargs.h>
#include <klocale.h>
#include <kglobalsettings.h>
#include <kdebug.h>
#include <kcrash.h>
#include <kpty.h>
#include <kuser.h>
#include <kglobal.h>

#include "kwrited.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <fixx11h.h>

#include <config.h>

/* TODO
   for anyone who likes to do improvements here, go ahead.
   - check FIXMEs below
   - add Menu
     - accept messages (on/off)
     - pop up on incoming messages
     - clear messages
     - allow max. lines
   - add CORBA interface?
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
  setFocusPolicy(QWidget::NoFocus);
  setWordWrap(QTextEdit::WidgetWidth);
  setTextFormat(Qt::PlainText);

  pty = new KPty();
  pty->open();
  pty->login(KUser().loginName().local8Bit().data(), getenv("DISPLAY"));
  QSocketNotifier *sn = new QSocketNotifier(pty->masterFd(), QSocketNotifier::Read, this);
  connect(sn, SIGNAL(activated(int)), this, SLOT(block_in(int)));

  QString txt = i18n("KWrited - Listening on Device %1").arg(pty->ttyName());
  setCaption(txt);
  puts(txt.local8Bit().data());
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

  insert( QString::fromLocal8Bit( buf, len ).remove('\r') );
  show();
  raise();
}

void KWrited::clearText()
{
   clear();
}

QPopupMenu *KWrited::createPopupMenu( const QPoint &pos )
{
   QPopupMenu *menu = QTextEdit::createPopupMenu( pos );

   menu->insertItem( i18n( "Clear Messages" ),
                     this, SLOT( clearText() ), 
                     0, -1, 0 );

   return menu;
}

KWritedModule::KWritedModule( const QCString& obj )
    : KDEDModule( obj )
{
    KGlobal::locale()->insertCatalogue("konsole");
    pro = new KWrited;
}

KWritedModule::~KWritedModule()
{
    delete pro;
    KGlobal::locale()->removeCatalogue("konsole");
}

extern "C"
KDE_EXPORT KDEDModule* create_kwrited( const QCString& obj )
    {
    return new KWritedModule( obj );
    }

#include "kwrited.moc"

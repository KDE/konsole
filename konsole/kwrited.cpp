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

KWrited::KWrited() : QObject()
{
  wid = new QTextEdit(0, "messages");
  wid->setFont(KGlobalSettings::fixedFont());
  wid->setMinimumWidth(wid->fontMetrics().maxWidth()*80 +
      wid->minimumSizeHint().width());
  wid->setReadOnly(true);
  wid->setFocusPolicy(QWidget::NoFocus);

  pty = new KPty();
  pty->open();
  pty->login(KUser().loginName().local8Bit().data(), getenv("DISPLAY"));
  QSocketNotifier *sn = new QSocketNotifier(pty->masterFd(), QSocketNotifier::Read, this);
  connect(sn, SIGNAL(activated(int)), this, SLOT(block_in(int)));

  QString txt = i18n("KWrited - Listening on Device %1").arg(pty->ttyName());
  wid->setCaption(txt);
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

  wid->insert( QString::fromLocal8Bit( buf, len ).remove('\r') );
  wid->show();
  wid->raise();
}

static KWrited *pro = 0;

void signal_handler(int) {
    delete pro;
    ::exit(0);
}

extern "C" int kdemain(int argc, char* argv[])
{
  KLocale::setMainCatalogue("konsole");
  KCmdLineArgs::init(argc, argv, "kwrited", I18N_NOOP("WriteDaemon"),
	I18N_NOOP("KDE Daemon for receiving 'write' messages."),
	"2.0.0");

  KUniqueApplication::addCmdLineOptions();

  if (!KUniqueApplication::start())
  {
     fputs(i18n("kwrited is already running.\n").local8Bit().data(), stderr);
     exit(1);
  }

  // WABA: Make sure not to enable session management.
  unsetenv("SESSION_MANAGER");
  signal (SIGHUP, signal_handler);
  KCrash::setEmergencySaveFunction(signal_handler);

  KUniqueApplication app;
  pro = new KWrited;
  app.dcopClient()->setDaemonMode( true );
  int r = app.exec();
  delete pro;
  return r;
}

#include "kwrited.moc"


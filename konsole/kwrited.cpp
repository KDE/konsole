// [kwrited.C] A write(1) receiver for kde.

#include <kuniqueapplication.h>
#include <kcmdlineargs.h>
#include <klocale.h>
#include <kglobalsettings.h>
#include <kwrited.h>
#include <kdebug.h>
#include <dcopclient.h>
#include <qsocketnotifier.h>

#include <TEPty.h>
#include <stdlib.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <config.h>
#include <kcrash.h>

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

#ifndef HERE
#define HERE fprintf(stderr,"%s(%d): here\n",__FILE__,__LINE__)
#endif

KWrited::KWrited() : QObject()
{
  wid = new QMultiLineEdit(NULL,"kwrited");
  wid->setFont(KGlobalSettings::fixedFont());
  wid->setMinimumWidth(wid->fontMetrics().maxWidth()*80 +
      wid->minimumSizeHint().width());
  wid->setReadOnly(TRUE);
  wid->setFocusPolicy(QWidget::NoFocus);
  pty = new TEPty();
  QObject::connect(pty, SIGNAL(block_in(const char*,int)), this, SLOT(block_in(const char*,int)));

  pty->makePty();
  int fd = pty->masterFd();
  QSocketNotifier *sn = new QSocketNotifier(fd, QSocketNotifier::Read, this);
  connect(sn, SIGNAL(activated(int)), this, SLOT(block_in(int)));

  wid->setCaption(QString("KWrited - listening on device ") + pty->deviceName());
}

KWrited::~KWrited()
{
    pty->kill();
    pty->donePty(); // this needs to be called manually here, because the app is exiting
    delete pty;
}

void KWrited::block_in(int fd)
{
  char buf[4096];
  int len = read(fd, buf, 4096);
  if (len < 0)
     return;

  if (len < 0) len = 0;
  QCString text( buf, len+1 );
  text[len] = 0;
  wid->insert( QString::fromLocal8Bit( text ) );
  wid->show();
  XRaiseWindow( wid->x11Display(), wid->winId());
}

static KWrited *pro = 0;

void signal_handler(int) {
    delete pro;
    pro = 0;
    ::exit(0);
}

int main(int argc, char* argv[])
{
  KLocale::setMainCatalogue("konsole");
  KCmdLineArgs::init(argc, argv, "kwrited",
	I18N_NOOP("KDE Daemon for receiving 'write' messages."),
	"2.0.0");

  KUniqueApplication::addCmdLineOptions();

  if (!KUniqueApplication::start())
  {
     fprintf(stderr, i18n("kwrited is already running.").local8Bit());
     exit(1);
  }

  // WABA: Make sure not to enable session management.
  unsetenv("SESSION_MANAGER");
  signal (SIGHUP, signal_handler);
  KCrash::setEmergencySaveFunction(signal_handler);

  KUniqueApplication app;
  app.dcopClient()->setDaemonMode( true );

  pro = new KWrited;
  int r = app.exec();
  delete pro;
  pro = 0;
  return r;
}

#include "kwrited.moc"


// [kwrited.C] A write(1) receiver for kde.

#include <kuniqueapp.h>
#include <kcmdlineargs.h>
#include <klocale.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <kwrited.h>
#include <kdebug.h>
#include <dcopclient.h>

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

#if 1
  //FIXME: here we have to do some improvements.
  //       especially, i do not like to have any
  //       program running on the device.
  //       Have to make a new run, i guess.
  QStrList cmd; cmd.append("/bin/cat"); // dummy
  pty->run("/bin/cat",cmd,"dump",TRUE);
#endif

  wid->setCaption(QString("KWrited - listening on device ") + pty->deviceName());
}

KWrited::~KWrited()
{
    pty->kill();
    pty->donePty(); // this needs to be called manually here, because the app is exiting
    delete pty;
}

void KWrited::block_in(const char* txt, int len)
{
  if (len < 0) len = 0;
  QCString text( txt, len+1 );
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


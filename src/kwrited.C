// [kwrited.C] A write(1) receiver for kde.

#include <kapp.h>
#include <kwrited.h>
#include <TEShell.h>
#include <stdio.h>
#include <kwm.h>

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

#define HERE fprintf(stderr,"%s(%d): here\n",__FILE__,__LINE__)

KWrited::KWrited() : QObject()
{
  wid = new QMultiLineEdit(NULL,"kwrited");
  wid->setReadOnly(TRUE);
  wid->setFocusPolicy(QWidget::NoFocus);
  shell = new Shell();
  QObject::connect(shell, SIGNAL(block_in(const char*,int)), this, SLOT(block_in(const char*,int)));

#if 1
  //FIXME: here we have to do some improvements.
  //       especially, i do not like to have any
  //       program running on the device.
  //       Have to make a new run, i guess.
  QStrList cmd; cmd.append("/bin/cat"); // dummy
  shell->run(cmd,"dump",FALSE,TRUE);
#endif

  wid->setCaption(QString("KWrited - listening on device ") + shell->deviceName());
}

KWrited::~KWrited()
{
  //FIXME: make sure we properly remove the utmp stamp on session end or kill.
  exit(0);
}

void KWrited::block_in(const char* txt, int len)
{
  if (len < 0) len = 0;
  char *_text = new char[len+1]; 
  strncpy(_text,txt,len); 
  _text[len] = 0; 
  wid->insert(_text);
  wid->show(); KWM::raise(wid->winId());
  delete _text;
}

int main(int argc, char* argv[])
{
  KApplication app(argc, argv, "kwrited");
  //FIXME: check if we have already have kwrited running.
  KWrited pro;
  app.exec();
  return 0;
}

#include "kwrited.moc"


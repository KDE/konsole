#include "session.h"

#include <qpushbutton.h>

/*! \class TESession

    Sessions are combinations of Shells and Emulations.

    The stuff in here does not belong to the terminal emulation framework,
    but to main.C. It serves it's duty by providing a single reference
    to Shell/Emulation pairs. In fact, it is only there to demonstrate one
    of the abilities of the framework - multible sessions.
*/

TESession::TESession(KTMainWindow* main, TEWidget* te, QStrList & _args, const char* term, int login_session) : schema_no(0), font_no(3), args(_args)
{
  sh = new Shell(login_session);
  em = new VT102Emulation(te,term);

  this->term = strdup(term);

  sh->setSize(te->Lines(),te->Columns()); // not absolutely nessesary
  QObject::connect( sh,SIGNAL(block_in(const char*,int)),
                    em,SLOT(onRcvBlock(const char*,int)) );
  QObject::connect( em,SIGNAL(ImageSizeChanged(int,int)),
                    sh,SLOT(setSize(int,int)));
  QObject::connect( em,SIGNAL(ImageSizeChanged(int,int)),
                    main,SLOT(notifySize(int,int)));
  QObject::connect( em,SIGNAL(sndBlock(const char*,int)),
                    sh,SLOT(send_bytes(const char*,int)) );
  QObject::connect( em,SIGNAL(changeColumns(int)),
                    main,SLOT(changeColumns(int)) );
  QObject::connect( em,SIGNAL(changeTitle(int, const char*)),
                    main,SLOT(changeTitle(int, const char*)) );
  QObject::connect( this,SIGNAL(done(TESession*,int)),
                    main,SLOT(doneSession(TESession*,int)) );
  QObject::connect( sh,SIGNAL(done(int)), this,SLOT(done(int)) );
  //FIXME: note the right place
  QObject::connect( te,SIGNAL(configureRequest(TEWidget*,int,int,int)),
                    main,SLOT(configureRequest(TEWidget*,int,int,int)) );
}

void TESession::run()
{
  sh->run(args,term);
}

void TESession::kill(int signal)
{
  sh->kill(signal);
}

TESession::~TESession()
{
  free(term);
  delete em;
  delete sh;
}

void TESession::setConnect(bool c)
{
  em->setConnect(c);
}

void TESession::done(int status)
{
  emit done(this,status);
}

void TESession::terminate()
{
  delete this;
}

Emulation* TESession::getEmulation()
{
  return em;
}

// following interfaces might be misplaced ///

int TESession::schemaNo()
{
  return schema_no;
}

int TESession::fontNo()
{
  return font_no;
}

const char* TESession::emuName()
{
  return term;
}

void TESession::setSchemaNo(int sn)
{
  schema_no = sn;
}

void TESession::setFontNo(int fn)
{
  font_no = fn;
}

void TESession::setTitle(const char* title)
{
  this->title = title;
}

const char* TESession::Title()
{
  return title.data();
}


#include "session.moc"

#include "session.h"

#include <qpushbutton.h>

TESession::TESession(KTMainWindow* main, TEWidget* te, char* args[], const char* term)
{
  sh = new Shell();
  em = new VT102Emulation(te);

  emuname = term;

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
  QObject::connect( em,SIGNAL(changeTitle(int, char*)),
                    main,SLOT(changeTitle(int, char*)) );
  QObject::connect( this,SIGNAL(done(TESession*)),
                    main,SLOT(doneSession(TESession*)) );
  QObject::connect( sh,SIGNAL(done()), this,SLOT(done()) );
  QObject::connect( te,SIGNAL(configureRequest(TEWidget*,int,int)),
                    main,SLOT(configureRequest(TEWidget*,int,int)) );

  sh->run(args,term);
}

TESession::~TESession()
{
  delete em;
  delete sh;
}

void TESession::setConnect(bool c)
{
  em->setConnect(c);
}

void TESession::done()
{
  setConnect(FALSE);
  emit done(this);
  delete this;
}

Emulation* TESession::getEmulation()
{
  return em;
}

// following interfaces might be misplaces ///

int TESession::schemaNo()
{
  return schema_no;
}

int TESession::fontNo()
{
  return font_no;
}

const char* TESession::term()
{
  return emuname.data();
}

void TESession::setSchemaNo(int sn)
{
  schema_no = sn;
}

void TESession::setFontNo(int fn)
{
  font_no = fn;
}


#include "session.moc"

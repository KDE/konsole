#include "session.h"
#include <qpushbutton.h>
#include <kdebug.h>

#include <stdlib.h>

#ifndef HERE
#define HERE fprintf(stderr,"%s(%d): here\n",__FILE__,__LINE__)
#endif

/*! \class TESession

    Sessions are combinations of TEPTy and Emulations.

    The stuff in here does not belong to the terminal emulation framework,
    but to main.C. It serves it's duty by providing a single reference
    to TEPTy/Emulation pairs. In fact, it is only there to demonstrate one
    of the abilities of the framework - multible sessions.
*/

TESession::TESession(KMainWindow* main, TEWidget* te, const QString &_pgm, QStrList & _args, const QString &_term)
   : schema_no(0)
   , font_no(3)
   , pgm(_pgm)
   , args(_args)
{
  //kdDebug(1211)<<"TESession ctor() new TEPty"<<endl;
  sh = new TEPty();
  //kdDebug(1211)<<"TESession ctor() new TEmuVt102"<<endl;
  em = new TEmuVt102(te);

  term = _term;
  iconText = kapp->caption();

  //kdDebug(1211)<<"TESession ctor() sh->setSize()"<<endl;
  sh->setSize(te->Lines(),te->Columns()); // not absolutely nessesary
  //kdDebug(1211)<<"TESession ctor() connecting"<<endl;
  connect( sh,SIGNAL(block_in(const char*,int)),em,SLOT(onRcvBlock(const char*,int)) );
  connect( em,SIGNAL(ImageSizeChanged(int,int)),sh,SLOT(setSize(int,int)));

  // 'main' should do those connects itself, somehow.
  // These aren't KTMW's slots, but konsole's.(David)

  connect( em,SIGNAL(ImageSizeChanged(int,int)),main,SLOT(notifySize(int,int)));
  connect( em,SIGNAL(sndBlock(const char*,int)),sh,SLOT(send_bytes(const char*,int)) );
  connect( em,SIGNAL(changeColumns(int)),main,SLOT(changeColumns(int)) );

  connect( em, SIGNAL( changeTitle( int, const QString & ) ),
           this, SLOT( setUserTitle( int, const QString & ) ) );

  connect( sh,SIGNAL(done(int)), this,SLOT(done(int)) );
  //kdDebug(1211)<<"TESession ctor() done"<<endl;
}



void TESession::run()
{
  //kdDebug(1211) << "Running the session!" << pgm << "\n";
  //pgm = "pine";
  sh->run(QFile::encodeName(pgm),args,term.latin1(),FALSE);
}

void TESession::setUserTitle( int what, const QString &caption )
{
    // (btw: what=0 changes title and icon, what=1 only icon, what=2 only title
    if ((what == 0) || (what == 2))
       userTitle = caption;
    if ((what == 0) || (what == 1))
       iconText = caption;
    emit updateTitle();
}

QString TESession::fullTitle() const
{
    QString res = title;
    if ( !userTitle.isEmpty() )
        res = userTitle + " - " + res;
    return res;
}


void TESession::kill(int signal)
{
  sh->kill(signal);
}

TESession::~TESession()
{
 //kdDebug(1211) << "disconnnecting..." << endl;
 QObject::disconnect( sh, SIGNAL( done( int ) ),
		      this, SLOT( done( int ) ) );
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

TEmulation* TESession::getEmulation()
{
  return em;
}

// following interfaces might be misplaced ///

int TESession::schemaNo()
{
  return schema_no;
}

int TESession::keymapNo()
{
  return em->keymapNo();
}

QString TESession::keymap()
{
  return em->keymap();
}

int TESession::fontNo()
{
  return font_no;
}

const QString & TESession::Term()
{
  return term;
}

void TESession::setSchemaNo(int sn)
{
  schema_no = sn;
}

void TESession::setKeymapNo(int kn)
{
  em->setKeymap(kn);
}

void TESession::setKeymap(const QString &id)
{
  em->setKeymap(id);
}

void TESession::setFontNo(int fn)
{
  font_no = fn;
}

void TESession::setTitle(const QString& _title)
{
  title = _title;
  //kdDebug(1211)<<"Session setTitle " <<  title <<endl;
}

const QString& TESession::Title()
{
  return title;
}

void TESession::setIconText(const QString& _iconText)
{
  iconText = _iconText;
  //kdDebug(1211)<<"Session setIconText " <<  iconText <<endl;
}

const QString& TESession::IconText()
{
  return iconText;
}

void TESession::setHistory(const HistoryType &hType)
{
  em->setHistory(hType);
}

const HistoryType& TESession::history()
{
  return em->history();
}

QStrList TESession::getArgs()
{
  return args;
}

QString TESession::getPgm()
{
  return pgm;
}


#include "session.moc"

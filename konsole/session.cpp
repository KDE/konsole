#include "session.h"
#include <kdebug.h>
#include <dcopclient.h>
#include <kmessagebox.h>

#include <stdlib.h>
#include <qfile.h>
#include <qdir.h>
#include <qregexp.h>

#ifndef HERE
#define HERE fprintf(stderr,"%s(%d): here\n",__FILE__,__LINE__)
#endif

/*! \class TESession

    Sessions are combinations of TEPTy and Emulations.

    The stuff in here does not belong to the terminal emulation framework,
    but to main.cpp. It serves it's duty by providing a single reference
    to TEPTy/Emulation pairs. In fact, it is only there to demonstrate one
    of the abilities of the framework - multible sessions.
*/

TESession::TESession(TEWidget* _te, const QString &_pgm, QStrList & _args, const QString &_term,const QString &_sessionId, const QString &_initial_cwd)
   : DCOPObject( _sessionId.latin1() )
   , monitorActivity(false)
   , monitorSilence(false)
   , masterMode(false)
   , schema_no(0)
   , font_no(3)
   , silence_seconds(10)
   , add_to_utmp(true)
   , pgm(_pgm)
   , args(_args)
   , sessionId(_sessionId)
   , cwd("")
   , initial_cwd(_initial_cwd)
{
  //kdDebug(1211)<<"TESession ctor() new TEPty"<<endl;
  sh = new TEPty();
  te = _te;
  //kdDebug(1211)<<"TESession ctor() new TEmuVt102"<<endl;
  em = new TEmuVt102(te);

  term = _term;
  iconName = "openterm";
  iconText = kapp->caption();

  //kdDebug(1211)<<"TESession ctor() sh->setSize()"<<endl;
  sh->setSize(te->Lines(),te->Columns()); // not absolutely nessesary
  //kdDebug(1211)<<"TESession ctor() connecting"<<endl;
  connect( sh,SIGNAL(block_in(const char*,int)),em,SLOT(onRcvBlock(const char*,int)) );

  connect( em,SIGNAL(ImageSizeChanged(int,int)),sh,SLOT(setSize(int,int)));
  connect( em,SIGNAL(sndBlock(const char*,int)),sh,SLOT(send_bytes(const char*,int)) );
  connect( em,SIGNAL(lockPty(bool)),sh,SLOT(lockPty(bool)) );

  connect( em, SIGNAL( changeTitle( int, const QString & ) ),
           this, SLOT( setUserTitle( int, const QString & ) ) );

  connect( em, SIGNAL( notifySessionState(int) ),
           this, SLOT( notifySessionState(int) ) );
  monitorTimer = new QTimer(this);
  connect(monitorTimer, SIGNAL(timeout()), this, SLOT(monitorTimerDone()));

  connect( sh,SIGNAL(done(int)), this,SLOT(done(int)) );
  //kdDebug(1211)<<"TESession ctor() done"<<endl;
  if (!sh->error().isEmpty())
     QTimer::singleShot(0, this, SLOT(ptyError()));
}

void TESession::ptyError()
{
  KMessageBox::error(te->topLevelWidget(), sh->error());
  emit done(this, 0);
}

void TESession::changeWidget(TEWidget* w)
{
  te=w;
  em->changeGUI(w);
}

void TESession::run()
{
  //kdDebug(1211) << "Running the session!" << pgm << "\n";
  //pgm = "pine";
  QString appId=kapp->dcopClient()->appId();

  QString cwd_save = QDir::currentDirPath();
  if (!initial_cwd.isEmpty())
     QDir::setCurrent(initial_cwd);
  sh->run(QFile::encodeName(pgm),args,term.latin1(), add_to_utmp,
          ("DCOPRef("+appId+",konsole)").latin1(),
          ("DCOPRef("+appId+","+sessionId+")").latin1());
  if (!initial_cwd.isEmpty())
     QDir::setCurrent(cwd_save);
  else
     initial_cwd=cwd_save;

  sh->setWriteable(false);  // We are reachable via kwrited.
}

void TESession::setUserTitle( int what, const QString &caption )
{
    // (btw: what=0 changes title and icon, what=1 only icon, what=2 only title
    if ((what == 0) || (what == 2))
       userTitle = caption;
    if ((what == 0) || (what == 1))
       iconText = caption;
    if (what == 30)
       renameSession(caption);
    if (what == 31) {
       cwd=caption;
       cwd=cwd.replace( QRegExp("^~"), QDir::homeDirPath() );
       emit openURLRequest(cwd);
    }
    emit updateTitle();
}

QString TESession::fullTitle() const
{
    QString res = title;
    if ( !userTitle.isEmpty() )
        res = userTitle + " - " + res;
    return res;
}

void TESession::monitorTimerDone()
{
  emit notifySessionState(this,NOTIFYSILENCE);
  monitorTimer->start(silence_seconds*1000,true);
}

void TESession::notifySessionState(int state)
{
  if (state==NOTIFYACTIVITY) {
    if (monitorSilence) {
      monitorTimer->stop();
      monitorTimer->start(silence_seconds*1000,true);
    }
    if (!monitorActivity)
      return;
  }

  emit notifySessionState(this, state);
}

bool TESession::sendSignal(int signal)
{
  return sh->kill(signal);
}

bool TESession::closeSession()
{
  return sendSignal(SIGHUP);
}

void TESession::feedSession(const QString &text)
{
  emit clearAllListenToKeyPress();
  setListenToKeyPress(true);
  te->emitText(text);
  setListenToKeyPress(false);
  emit restoreAllListenToKeyPress();
}

void TESession::sendSession(const QString &text)
{
  QString newtext=text;
  newtext.append("\r");
  feedSession(newtext);
}

void TESession::renameSession(const QString &name)
{
  title=name;
  emit renameSession(this,name);
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
  setListenToKeyPress(c);
}

void TESession::setListenToKeyPress(bool l)
{
  em->setListenToKeyPress(l);
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

const QString & TESession::SessionId()
{
  return sessionId;
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

void TESession::setIconName(const QString& _iconName)
{
  iconName = _iconName;
}

void TESession::setIconText(const QString& _iconText)
{
  iconText = _iconText;
  //kdDebug(1211)<<"Session setIconText " <<  iconText <<endl;
}

const QString& TESession::IconName()
{
  return iconName;
}

const QString& TESession::IconText()
{
  return iconText;
}

bool TESession::testAndSetStateIconName (const QString& newname)
{
  if (newname != stateIconName)
    {
      stateIconName = newname;
      return true;
    }
  return false;
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

QString TESession::getCwd()
{
#ifdef HAVE_PROC_CWD
  if (cwd.isEmpty()) {
    QFileInfo Cwd(QString("/proc/%1/cwd").arg(sh->pid()));
    if(Cwd.isSymLink())
      return Cwd.readLink();
  }
#endif /* HAVE_PROC_CWD */
  return cwd;
}

bool TESession::isMonitorActivity() { return monitorActivity; }
bool TESession::isMonitorSilence() { return monitorSilence; }
bool TESession::isMasterMode() { return masterMode; }

void TESession::setMonitorActivity(bool _monitor) { monitorActivity=_monitor; }
void TESession::setMonitorSilence(bool _monitor)
{
  if (monitorSilence==_monitor)
    return;

  monitorSilence=_monitor;
  if (monitorSilence)
    monitorTimer->start(silence_seconds*1000,true);
  else
    monitorTimer->stop();
}

void TESession::setMonitorSilenceSeconds(int seconds)
{
  silence_seconds=seconds;
  if (monitorSilence) {
    monitorTimer->stop();
    monitorTimer->start(silence_seconds*1000,true);
  }
}

void TESession::setMasterMode(bool _master)
{
  masterMode=_master;
}

void TESession::setAddToUtmp(bool set)
{
  add_to_utmp = set;
}

bool TESession::processDynamic(const QCString &fun, const QByteArray &data, QCString& replyType, QByteArray &replyData)
{
    if (fullScripting)
    {
      if (fun == "feedSession(QString)")
      {
        QString arg0;
        QDataStream arg( data, IO_ReadOnly );
        arg >> arg0;
        feedSession(arg0);         
        replyType = "void";
        return true;
      }
      else if (fun == "sendSession(QString)")
      {
        QString arg0;
        QDataStream arg( data, IO_ReadOnly );
        arg >> arg0;
        sendSession(arg0);         
        replyType = "void";
        return true;
      }
    }
    return SessionIface::processDynamic(fun, data, replyType, replyData);

}

QCStringList TESession::functionsDynamic()
{
    QCStringList funcs = SessionIface::functionsDynamic();
    if (fullScripting)
    {
       funcs << "void feedSession(QString text)";
       funcs << "void sendSession(QString text)";
    }
    return funcs;
}


#include "session.moc"

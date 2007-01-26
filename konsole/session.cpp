/*
    This file is part of Konsole, an X terminal.
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
      
#include "session.h"
#include "zmodem_dialog.h"

#include <kdebug.h>
#include <dcopclient.h>
#include <kmessagebox.h>
#include <knotifyclient.h>
#include <klocale.h>
#include <kprocio.h>
#include <krun.h>
#include <kshell.h>
#include <kstandarddirs.h>

#include <stdlib.h>
#include <qfile.h>
#include <qdir.h>
#include <qregexp.h>
#include <qtextedit.h>

/*! \class TESession

    Sessions are combinations of TEPTy and Emulations.

    The stuff in here does not belong to the terminal emulation framework,
    but to main.cpp. It serves it's duty by providing a single reference
    to TEPTy/Emulation pairs. In fact, it is only there to demonstrate one
    of the abilities of the framework - multible sessions.
*/

TESession::TESession(TEWidget* _te, const QString &_term, ulong _winId, const QString &_sessionId, const QString &_initial_cwd)
   : DCOPObject( _sessionId.latin1() )
   , sh(0)
   , connected(true)
   , monitorActivity(false)
   , monitorSilence(false)
   , notifiedActivity(false)
   , masterMode(false)
   , autoClose(true)
   , wantedClose(false)
   , schema_no(0)
   , font_no(3)
   , silence_seconds(10)
   , add_to_utmp(true)
   , xon_xoff(false)
   , pgm(QString())
   , args(QStrList())
   , sessionId(_sessionId)
   , cwd("")
   , initial_cwd(_initial_cwd)
   , zmodemBusy(false)
   , zmodemProc(0)
   , zmodemProgress(0)
   , encoding_no(0)
{
  //kdDebug(1211)<<"TESession ctor() new TEPty"<<endl;
  te = _te;
  //kdDebug(1211)<<"TESession ctor() new TEmuVt102"<<endl;
  em = new TEmuVt102(te);
  font_h = te-> fontHeight();
  font_w = te-> fontWidth();
  QObject::connect(te,SIGNAL(changedContentSizeSignal(int,int)),
                   this,SLOT(onContentSizeChange(int,int)));
  QObject::connect(te,SIGNAL(changedFontMetricSignal(int,int)),
                   this,SLOT(onFontMetricChange(int,int)));

  term = _term;
  winId = _winId;
  iconName = "konsole";

  setPty( new TEPty() );

  connect( em, SIGNAL( changeTitle( int, const QString & ) ),
           this, SLOT( setUserTitle( int, const QString & ) ) );
  connect( em, SIGNAL( notifySessionState(int) ),
           this, SLOT( notifySessionState(int) ) );
  monitorTimer = new QTimer(this);
  connect(monitorTimer, SIGNAL(timeout()), this, SLOT(monitorTimerDone()));

  connect( em, SIGNAL( zmodemDetected() ), this, SLOT(slotZModemDetected()));

  connect( em, SIGNAL( changeTabTextColor( int ) ),
           this, SLOT( changeTabTextColor( int ) ) );

  //kdDebug(1211)<<"TESession ctor() done"<<endl;
}

void TESession::setPty(TEPty *_sh)
{
  if ( sh ) {
    delete sh;
  }
  sh = _sh;
  connect( sh, SIGNAL( forkedChild() ),
           this, SIGNAL( forkedChild() ));

  //kdDebug(1211)<<"TESession ctor() sh->setSize()"<<endl;
  sh->setSize(te->Lines(),te->Columns()); // not absolutely nessesary
  sh->useUtf8(em->utf8());
  //kdDebug(1211)<<"TESession ctor() connecting"<<endl;
  connect( sh,SIGNAL(block_in(const char*,int)),this,SLOT(onRcvBlock(const char*,int)) );

  connect( em,SIGNAL(sndBlock(const char*,int)),sh,SLOT(send_bytes(const char*,int)) );
  connect( em,SIGNAL(lockPty(bool)),sh,SLOT(lockPty(bool)) );
  connect( em,SIGNAL(useUtf8(bool)),sh,SLOT(useUtf8(bool)) );

  connect( sh,SIGNAL(done(int)), this,SLOT(done(int)) );

  if (!sh->error().isEmpty())
     QTimer::singleShot(0, this, SLOT(ptyError()));
}

void TESession::ptyError()
{
  // FIXME:  sh->error() is always empty
  if ( sh->error().isEmpty() )
    KMessageBox::error( te->topLevelWidget(),
       i18n("Konsole is unable to open a PTY (pseudo teletype).  It is likely that this is due to an incorrect configuration of the PTY devices.  Konsole needs to have read/write access to the PTY devices."), 
       i18n("A Fatal Error Has Occurred") );
  else
    KMessageBox::error(te->topLevelWidget(), sh->error());
  emit done(this);
}

void TESession::changeWidget(TEWidget* w)
{
  QObject::disconnect(te,SIGNAL(changedContentSizeSignal(int,int)),
                     this,SLOT(onContentSizeChange(int,int)));
  QObject::disconnect(te,SIGNAL(changedFontMetricSignal(int,int)),
                     this,SLOT(onFontMetricChange(int,int)));
  te=w;
  em->changeGUI(w);
  font_h = te->fontHeight();
  font_w = te->fontWidth();
  sh->setSize(te->Lines(),te->Columns()); // not absolutely nessesary

  te->setDefaultBackColor(modifiedBackground);

  QObject::connect(te,SIGNAL(changedContentSizeSignal(int,int)),
                   this,SLOT(onContentSizeChange(int,int)));
  QObject::connect(te,SIGNAL(changedFontMetricSignal(int,int)),
                   this,SLOT(onFontMetricChange(int,int)));
}

void TESession::setProgram( const QString &_pgm, const QStrList &_args )
{
    pgm = _pgm;
    args = _args;
}

void TESession::run()
{
  // Upon a KPty error, there is no description on what that error was...
  // Check to see if the given program is executable.
  QString exec = QFile::encodeName(pgm);
  exec = KRun::binaryName(exec, false);
  exec = KShell::tildeExpand(exec);
  QString pexec = KGlobal::dirs()->findExe(exec);
  if ( pexec.isEmpty() ) {
    kdError()<<"can not execute "<<exec<<endl;
    QTimer::singleShot(1, this, SLOT(done()));
    return;
  }

  QString appId=kapp->dcopClient()->appId();

  QString cwd_save = QDir::currentDirPath();
  if (!initial_cwd.isEmpty())
     QDir::setCurrent(initial_cwd);
  sh->setXonXoff(xon_xoff);

  int result = sh->run(QFile::encodeName(pgm), args, term.latin1(), 
          winId, add_to_utmp,
          ("DCOPRef("+appId+",konsole)").latin1(),
          ("DCOPRef("+appId+","+sessionId+")").latin1());
  if (result < 0) {     // Error in opening pseudo teletype
    kdWarning()<<"Unable to open a pseudo teletype!"<<endl;
    QTimer::singleShot(0, this, SLOT(ptyError()));
  }
  sh->setErase(em->getErase());

  if (!initial_cwd.isEmpty())
     QDir::setCurrent(cwd_save);
  else
     initial_cwd=cwd_save;

  sh->setWriteable(false);  // We are reachable via kwrited.
}

void TESession::changeTabTextColor( int color )
{
    emit changeTabTextColor( this, color );
}

void TESession::setUserTitle( int what, const QString &caption )
{
    // (btw: what=0 changes title and icon, what=1 only icon, what=2 only title
    if ((what == 0) || (what == 2))
       userTitle = caption;
    if ((what == 0) || (what == 1))
       iconText = caption;
    if (what == 11) {
      QString colorString = caption.section(';',0,0);
      QColor backColor = QColor(colorString);
      if (backColor.isValid()){// change color via \033]11;Color\007
	if (backColor != modifiedBackground) {
	    modifiedBackground = backColor;
	    te->setDefaultBackColor(backColor);
	}
      }
    }
    if (what == 30)
       renameSession(caption);
    if (what == 31) {
       cwd=caption;
       cwd=cwd.replace( QRegExp("^~"), QDir::homeDirPath() );
       emit openURLRequest(cwd);
    }    
    if (what == 32) { // change icon via \033]32;Icon\007
       iconName = caption;
       te->update();
    }

    emit updateTitle(this);
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
  if (monitorSilence) {
    KNotifyClient::event(winId, "Silence", i18n("Silence in session '%1'").arg(title));
    emit notifySessionState(this,NOTIFYSILENCE);
  }
  notifiedActivity=false;
}

void TESession::notifySessionState(int state)
{
  if (state==NOTIFYBELL) {
    te->Bell(em->isConnected(),i18n("Bell in session '%1'").arg(title));
  } else if (state==NOTIFYACTIVITY) {
    if (monitorSilence) {
      monitorTimer->start(silence_seconds*1000,true);
    }
    if (!monitorActivity)
      return;
    if (!notifiedActivity) {
      KNotifyClient::event(winId, "Activity", i18n("Activity in session '%1'").arg(title));
      notifiedActivity=true;
      monitorTimer->start(silence_seconds*1000,true);
    }
  }

  emit notifySessionState(this, state);
}

void TESession::onContentSizeChange(int height, int width)
{
  // ensure that image is at least one line high by one column wide
  const int columns = QMAX( width/font_w , 1 );
  const int lines = QMAX( height/font_h , 1 );

  em->onImageSizeChange( lines , columns );
  sh->setSize( lines , columns );
}

void TESession::onFontMetricChange(int height, int width)
{
  //kdDebug(1211)<<"TESession::onFontMetricChange " << height << " " << width << endl;
  if (connected) {
    font_h = height;
    font_w = width;
  }
}

bool TESession::sendSignal(int signal)
{
  return sh->kill(signal);
}

bool TESession::closeSession()
{
  autoClose = true;
  wantedClose = true;
  if (!sh->isRunning() || !sendSignal(SIGHUP))
  {
     // Forced close.
     QTimer::singleShot(1, this, SLOT(done()));
  }
  return true;
}

void TESession::feedSession(const QString &text)
{
  emit disableMasterModeConnections();
  setListenToKeyPress(true);
  te->emitText(text);
  setListenToKeyPress(false);
  emit enableMasterModeConnections();
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
  QObject::disconnect( sh, SIGNAL( done(int) ),
                       this, SLOT( done(int) ) );
  delete em;
  delete sh;

  delete zmodemProc;
}

void TESession::setConnect(bool c)
{
  connected=c;
  em->setConnect(c);
  setListenToKeyPress(c);
}

void TESession::setListenToKeyPress(bool l)
{
  em->setListenToKeyPress(l);
}

void TESession::done() {
  emit processExited(sh);
  emit done(this);
}

void TESession::done(int exitStatus)
{
  if (!autoClose)
  {
    userTitle = i18n("<Finished>");
    emit updateTitle(this);
    return;
  }
  if (!wantedClose && (exitStatus || sh->signalled()))
  {
    if (sh->normalExit())
      KNotifyClient::event(winId, "Finished", i18n("Session '%1' exited with status %2.").arg(title).arg(exitStatus));
    else if (sh->signalled())
    {
      if (sh->coreDumped())
        KNotifyClient::event(winId, "Finished", i18n("Session '%1' exited with signal %2 and dumped core.").arg(title).arg(sh->exitSignal()));
      else
        KNotifyClient::event(winId, "Finished", i18n("Session '%1' exited with signal %2.").arg(title).arg(sh->exitSignal()));
    }
    else
      KNotifyClient::event(winId, "Finished", i18n("Session '%1' exited unexpectedly.").arg(title));
  }
  emit processExited(sh);
  emit done(this);
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

int TESession::encodingNo()
{
  return encoding_no;
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

void TESession::setEncodingNo(int index)
{
  encoding_no = index;
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

void TESession::clearHistory()
{
  if (history().isOn()) {
    int histSize = history().getSize();
    setHistory(HistoryTypeNone());
    if (histSize)
      setHistory(HistoryTypeBuffer(histSize));
    else
      setHistory(HistoryTypeFile());
  }
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

void TESession::setMonitorActivity(bool _monitor)
{
  monitorActivity=_monitor;
  notifiedActivity=false;
}

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

void TESession::setXonXoff(bool set)
{
  xon_xoff = set;
}

void TESession::slotZModemDetected()
{
  if (!zmodemBusy)
  {
    QTimer::singleShot(10, this, SLOT(emitZModemDetected()));
    zmodemBusy = true;
  }
}

void TESession::emitZModemDetected()
{
  emit zmodemDetected(this);
}

void TESession::cancelZModem()
{
  sh->send_bytes("\030\030\030\030", 4); // Abort
  zmodemBusy = false;
}

void TESession::startZModem(const QString &zmodem, const QString &dir, const QStringList &list)
{
  zmodemBusy = true;
  zmodemProc = new KProcIO;

  (*zmodemProc) << zmodem << "-v";
  for(QStringList::ConstIterator it = list.begin();
      it != list.end();
      ++it)
  {
     (*zmodemProc) << (*it);
  }

  if (!dir.isEmpty())
     zmodemProc->setWorkingDirectory(dir);
  zmodemProc->start(KProcIO::NotifyOnExit, false);

  // Override the read-processing of KProcIO
  disconnect(zmodemProc,SIGNAL (receivedStdout (KProcess *, char *, int)), 0, 0);
  connect(zmodemProc,SIGNAL (receivedStdout (KProcess *, char *, int)),
          this, SLOT(zmodemSendBlock(KProcess *, char *, int)));
  connect(zmodemProc,SIGNAL (receivedStderr (KProcess *, char *, int)),
          this, SLOT(zmodemStatus(KProcess *, char *, int)));
  connect(zmodemProc,SIGNAL (processExited(KProcess *)),
          this, SLOT(zmodemDone()));

  disconnect( sh,SIGNAL(block_in(const char*,int)), this, SLOT(onRcvBlock(const char*,int)) );
  connect( sh,SIGNAL(block_in(const char*,int)), this, SLOT(zmodemRcvBlock(const char*,int)) );
  connect( sh,SIGNAL(buffer_empty()), this, SLOT(zmodemContinue()));

  zmodemProgress = new ZModemDialog(te->topLevelWidget(), false,
                                    i18n("ZModem Progress"));

  connect(zmodemProgress, SIGNAL(user1Clicked()),
          this, SLOT(zmodemDone()));

  zmodemProgress->show();
}

void TESession::zmodemSendBlock(KProcess *, char *data, int len)
{
  sh->send_bytes(data, len);
//  qWarning("<-- %d bytes", len);
  if (sh->buffer_full())
  {
    zmodemProc->suspend();
//    qWarning("ZModem suspend");
  }
}

void TESession::zmodemContinue()
{
  zmodemProc->resume();
//  qWarning("ZModem resume");
}

void TESession::zmodemStatus(KProcess *, char *data, int len)
{
  QCString msg(data, len+1);
  while(!msg.isEmpty())
  {
     int i = msg.find('\015');
     int j = msg.find('\012');
     QCString txt;
     if ((i != -1) && ((j == -1) || (i < j)))
     {
       msg = msg.mid(i+1);
     }
     else if (j != -1)
     {
       txt = msg.left(j);
       msg = msg.mid(j+1);
     }
     else
     {
       txt = msg;
       msg.truncate(0);
     }
     if (!txt.isEmpty())
       zmodemProgress->addProgressText(QString::fromLocal8Bit(txt));
  }
}

void TESession::zmodemRcvBlock(const char *data, int len)
{
  QByteArray ba;
  ba.duplicate(data, len);
  zmodemProc->writeStdin(ba);
//  qWarning("--> %d bytes", len);
}

void TESession::zmodemDone()
{
  if (zmodemProc)
  {
    delete zmodemProc;
    zmodemProc = 0;
    zmodemBusy = false;

    disconnect( sh,SIGNAL(block_in(const char*,int)), this ,SLOT(zmodemRcvBlock(const char*,int)) );
    disconnect( sh,SIGNAL(buffer_empty()), this, SLOT(zmodemContinue()));
    connect( sh,SIGNAL(block_in(const char*,int)), this, SLOT(onRcvBlock(const char*,int)) );

    sh->send_bytes("\030\030\030\030", 4); // Abort
    sh->send_bytes("\001\013\n", 3); // Try to get prompt back
    zmodemProgress->done();
  }
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


void TESession::onRcvBlock( const char* buf, int len )
{
    em->onRcvBlock( buf, len );
    emit receivedData( QString::fromLatin1( buf, len ) );
}

void TESession::print( QPainter &paint, bool friendly, bool exact )
{
    te->print(paint, friendly, exact);
}

QString TESession::schema()
{
  QString currentSchema;
  emit getSessionSchema(this, currentSchema);
  return currentSchema;
}

void TESession::setSchema(const QString &schema)
{
  emit setSessionSchema(this, schema);
}

QString TESession::font()
{
  return te->getVTFont().toString();
}

void TESession::setFont(const QString &font)
{
  QFont tmp;
  if (tmp.fromString(font))
    te->setVTFont(tmp);
  else
    kdWarning()<<"unknown font: "<<font<<endl;
}

QString TESession::encoding()
{
  return em->codec()->name();
}

void TESession::setEncoding(const QString &encoding)
{
  emit setSessionEncoding(this, encoding);
}

QString TESession::keytab()
{
   return keymap();
}

void TESession::setKeytab(const QString &keytab)
{
   setKeymap(keytab);
   emit updateSessionConfig(this);
}

QSize TESession::size()
{
  return em->imageSize();
}

void TESession::setSize(QSize size)
{
  if ((size.width() <= 1) || (size.height() <= 1))
     return;
  
  emit resizeSession(this, size);
}

#include "session.moc"

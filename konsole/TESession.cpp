/* ----------------------------------------------------------------------- */
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

// Standard
#include <assert.h>
#include <stdlib.h>

// Qt
#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QRegExp>
#include <QStringList>
#include <QtDBus/QtDBus>

// KDE
#include <kdebug.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <knotification.h>
#include <kprocio.h>
#include <krun.h>
#include <kshell.h>
#include <kstandarddirs.h>

// Konsole
#include <config-konsole.h>
#include "NavigationItem.h"
#include "schema.h"
#include "sessionadaptor.h"
#include "sessionscriptingadaptor.h"
#include "zmodem_dialog.h"

#include "TESession.h"

int TESession::lastSessionId = 0;

TESession::TESession() : 
     sh(0)
   , te(0)
   , em(0)
   , connected(true)
   , monitorActivity(false)
   , monitorSilence(false)
   , notifiedActivity(false)
   , masterMode(false)
   , autoClose(true)
   , wantedClose(false)
   //, schema_no(0)
   , font_no(3)
   , silence_seconds(10)
   , add_to_utmp(true)
   , xon_xoff(true)
   , fullScripting(false)
   , zmodemBusy(false)
   , zmodemProc(0)
   , zmodemProgress(0)
   , encoding_no(0)
   , navItem(0)
   , _colorScheme(0)
{
    //prepare DBus communication
    (void)new SessionAdaptor(this);

    sessionId = QLatin1String("session") + QString::number(++lastSessionId);    	
    QDBusConnection::sessionBus().registerObject(QLatin1String("/Sessions/")+sessionId, this);

    //create teletype for I/O with shell process
    sh = new TEPty();

    //create emulation backend
    em = new TEmuVt102();

    connect( em, SIGNAL( changeTitle( int, const QString & ) ),
           this, SLOT( setUserTitle( int, const QString & ) ) );
    connect( em, SIGNAL( notifySessionState(int) ),
           this, SLOT( notifySessionState(int) ) );
    connect( em, SIGNAL( zmodemDetected() ), this, SLOT(slotZModemDetected()));
    connect( em, SIGNAL( changeTabTextColor( int ) ),
           this, SLOT( changeTabTextColor( int ) ) );

   
    //connect teletype to emulation backend 
    sh->useUtf8(em->utf8());
    
    connect( sh,SIGNAL(block_in(const char*,int)),this,SLOT(onRcvBlock(const char*,int)) );
    connect( em,SIGNAL(sndBlock(const char*,int)),sh,SLOT(send_bytes(const char*,int)) );
    connect( em,SIGNAL(lockPty(bool)),sh,SLOT(lockPty(bool)) );
    connect( em,SIGNAL(useUtf8(bool)),sh,SLOT(useUtf8(bool)) );

    
    connect( sh,SIGNAL(done(int)), this,SLOT(done(int)) );
 
    //setup timer for monitoring session activity
    monitorTimer = new QTimer(this);
    connect(monitorTimer, SIGNAL(timeout()), this, SLOT(monitorTimerDone()));

    //TODO: Investigate why a single-shot timer is used here 
    if (!sh->error().isEmpty())
        QTimer::singleShot(0, this, SLOT(ptyError()));
}

void TESession::setProgram(const QString& program)
{
    pgm = program;
}

void TESession::setArguments(const QStringList& arguments)
{
    args = arguments;
}

/*! \class TESession

    Sessions are combinations of TEPTy and Emulations.

    The stuff in here does not belong to the terminal emulation framework,
    but to main.cpp. It serves it's duty by providing a single reference
    to TEPTy/Emulation pairs. In fact, it is only there to demonstrate one
    of the abilities of the framework - multiple sessions.
*/

#if 0
TESession::TESession(TEWidget* _te, const QString &_pgm, const QStringList & _args, const QString &_term, ulong _winId, const QString &_sessionId, const QString &_initial_cwd)
   : TESession()
   , pgm(_pgm)
   , args(_args)
   , sessionId(_sessionId)
   , initial_cwd(_initial_cwd)
{
	(void)new SessionAdaptor(this);
	QDBusConnection::sessionBus().registerObject(QLatin1String("/Session")/*"/sessions/"+_sessionId*/, this);
	QDBusConnection::sessionBus().registerService("org.kde.konsole");
  //kDebug(1211)<<"TESession ctor() new TEPty"<<endl;
  sh = new TEPty();
  
  addView(_te);

  em = new TEmuVt102( primaryView() );
  font_h = primaryView()-> fontHeight();
  font_w = primaryView()-> fontWidth();
  QObject::connect(primaryView(),SIGNAL(changedContentSizeSignal(int,int)),
                   this,SLOT(onContentSizeChange(int,int)));
  QObject::connect(primaryView(),SIGNAL(changedFontMetricSignal(int,int)),
                   this,SLOT(onFontMetricChange(int,int)));

  term = _term;
  winId = _winId;
  iconName = "konsole";

  //kDebug(1211)<<"TESession ctor() sh->setSize()"<<endl;
  sh->setSize(primaryView()->Lines(),primaryView()->Columns()); // not absolutely necessary
  sh->useUtf8(em->utf8());
  //kDebug(1211)<<"TESession ctor() connecting"<<endl;
  connect( sh,SIGNAL(block_in(const char*,int)),this,SLOT(onRcvBlock(const char*,int)) );

  connect( em,SIGNAL(sndBlock(const char*,int)),sh,SLOT(send_bytes(const char*,int)) );
  connect( em,SIGNAL(lockPty(bool)),sh,SLOT(lockPty(bool)) );
  connect( em,SIGNAL(useUtf8(bool)),sh,SLOT(useUtf8(bool)) );

  connect( em, SIGNAL( changeTitle( int, const QString & ) ),
           this, SLOT( setUserTitle( int, const QString & ) ) );

  connect( em, SIGNAL( notifySessionState(int) ),
           this, SLOT( notifySessionState(int) ) );
  monitorTimer = new QTimer(this);
  connect(monitorTimer, SIGNAL(timeout()), this, SLOT(monitorTimerDone()));

  connect( em, SIGNAL( zmodemDetected() ), this, SLOT(slotZModemDetected()));

  connect( em, SIGNAL( changeTabTextColor( int ) ),
           this, SLOT( changeTabTextColor( int ) ) );

  connect( sh,SIGNAL(done(int)), this,SLOT(done(int)) );
  //kDebug(1211)<<"TESession ctor() done"<<endl;
  if (!sh->error().isEmpty())
     QTimer::singleShot(0, this, SLOT(ptyError()));
}
#endif

void TESession::ptyError()
{
  // FIXME:  sh->error() is always empty
  if ( sh->error().isEmpty() )
    KMessageBox::error( QApplication::activeWindow() ,
       i18n("Konsole is unable to open a PTY (pseudo teletype).  It is likely that this is due to an incorrect configuration of the PTY devices.  Konsole needs to have read/write access to the PTY devices."),
       i18n("A Fatal Error Has Occurred") );
  else
    KMessageBox::error(QApplication::activeWindow(), sh->error());
  emit done(this);
}

TEWidget* TESession::primaryView()
{
    if (!_views.isEmpty())
        return _views.first();
    else
        return 0;
}

NavigationItem* TESession::navigationItem()
{
    if (!navItem)
    {
        navItem = new SessionNavigationItem(this);
    }
    return navItem;
}

void TESession::addView(TEWidget* widget)
{
     Q_ASSERT( !_views.contains(widget) );

    _views.append(widget);

    if ( em != 0 )
        em->addView(widget);

    //update color scheme of view to match session
    if (_colorScheme)
    {
        widget->setColorTable(_colorScheme->table());
    }
    
    //connect view signals and slots
    QObject::connect( widget ,SIGNAL(changedContentSizeSignal(int,int)),
                   this,SLOT(onContentSizeChange(int,int)));
}

void TESession::removeView(TEWidget* widget)
{
    _views.removeAll(widget);

    if ( em != 0 )
        em->removeView(widget);
}

/*void TESession::changeWidget(TEWidget* w)
{
  Q_ASSERT(0); //Method not updated yet to handle multiple views
}*/

void TESession::run()
{
  //check that everything is in place to run the session
  if (pgm.isEmpty())
      kDebug() << "TESession::run() - program to run not set." << endl;
  if (args.isEmpty())
      kDebug() << "TESession::run() - no command line arguments specified." << endl;
  
  // Upon a KPty error, there is no description on what that error was...
  // Check to see if the given program is executable.
  QString exec = QFile::encodeName(pgm);
  exec = KRun::binaryName(exec, false);
  exec = KShell::tildeExpand(exec);
  QString pexec = KGlobal::dirs()->findExe(exec);
  if ( pexec.isEmpty() ) {
    kError()<<"can not execute "<<exec<<endl;
    QTimer::singleShot(1, this, SLOT(done()));
    return;
  }

  QString dbusService = QDBusConnection::sessionBus().baseService();
  QString cwd_save = QDir::currentPath();
  if (!initial_cwd.isEmpty())
     QDir::setCurrent(initial_cwd);
  sh->setXonXoff(xon_xoff);

  int result = sh->run(QFile::encodeName(pgm), args, term.toLatin1(),
          winId, add_to_utmp,
          dbusService.toLatin1(),
          (QLatin1String("/Sessions/") + sessionId).toLatin1());
  if (result < 0) {     // Error in opening pseudo teletype
    kWarning()<<"Unable to open a pseudo teletype!"<<endl;
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

// NOTE: TESession::hasChildren() was originally written so that the 'do you really want to close?'
// prompt delivered when closing a session would only be shown if the session had running processes.
// However, it was decided that stat-ing everything in /proc was very expensive, and others
// disagreed with the basic idea.
//
// This code is left here in case anyone wants to know a way of figuring out
// the PIDs of processes running in the terminal sessions :)
bool TESession::hasChildren()
{
	int sessionPID=sh->pid();

	//get ids of active processes from /proc and look at each process
	//to see whether it's parent is the session process,
	//in which case the session has active children
	//
	//This relies on scanning the whole of /proc, which may be expensive if there are an exceptionally
	//large number of processes running.  If you can think of a better method - please implement it!	
	
	QDir procDir("/proc");
	if (procDir.exists())
	{
		QStringList files=procDir.entryList();
		for (int i=0;i<files.count();i++)
		{
			//directory entries in /proc which begin with and contain only digits are process ids
			if ( files.at(i)[0].isDigit() )
			{
				int pid = files.at(i).toInt();
				
				// this assumes that child processes must have a pid > their parent processes
				// so we don't need to consider processes with a lower pid
				// I'm not absolutely sure whether this is valid or not though
				
				if ( pid <= sessionPID )
					continue;
				
				QFile processInfo( QString("/proc/%1/stat").arg(pid) );
				if ( processInfo.open( QIODevice::ReadOnly ) )
				{
					
					//open process info file and extract parent id
					QString infoText(processInfo.readAll());
					//process info file looks like this:
					//
					//process_id (process_name) S process_parent_id
					//... and we want the process_parent_id
					QRegExp rx("^[\\d]+ \\(.*\\) .[\\s]");

					if (rx.indexIn(infoText) != -1)
					{
						int offset = rx.matchedLength();
						int endOfPPID = infoText.indexOf(' ',offset);
						int ppid = infoText.mid(offset,endOfPPID-offset).toInt();

						if ( ppid == sessionPID )
							return true;
					}
					
					processInfo.close();
				}
			}
		}
	}

	return false;
}

void TESession::setUserTitle( int what, const QString &caption )
{
	bool modified = false; //set to true if anything is actually changed (eg. old title != new title )
	
    // (btw: what=0 changes title and icon, what=1 only icon, what=2 only title
    if ((what == 0) || (what == 2)) {
       	if ( _userTitle != caption ) {
			_userTitle = caption;
			modified = true;
		}
    }
    
    if ((what == 0) || (what == 1))
	{
		if ( iconText != caption ) {
       		iconText = caption;
			modified = true;
		}
	}
	
    if (what == 11) {
      QString colorString = caption.section(';',0,0);
      kDebug() << __FILE__ << __LINE__ << ": setting background colour to " << colorString << endl;
      QColor backColor = QColor(colorString);
      if (backColor.isValid()){// change color via \033]11;Color\007
	if (backColor != modifiedBackground) {
	    modifiedBackground = backColor;
	    
        QListIterator<TEWidget*> viewIter(_views);
        while (viewIter.hasNext())
            viewIter.next()->setDefaultBackColor(backColor);
	}
      }
    }
    
	if (what == 30) {
		if ( title != caption ) {
       		renameSession(caption);
			return;
		}
	}
	
    if (what == 31) {
       cwd=caption;
       cwd=cwd.replace( QRegExp("^~"), QDir::homePath() );
       emit openUrlRequest(cwd);
	}
	
    if (what == 32) { // change icon via \033]32;Icon\007
    	if ( iconName != caption ) {
	   		iconName = caption;

            QListIterator< TEWidget* > viewIter(_views);
            while (viewIter.hasNext())
       		    viewIter.next()->update();
			modified = true;
		}
    }

	if ( modified )
    	emit updateTitle();
}

QString TESession::userTitle() const
{
    return _userTitle;
}

QString TESession::displayTitle() const
{
    if (!_userTitle.isEmpty())
        return _userTitle;
    else
        return Title();
}

/*QString TESession::fullTitle() const
{
    QString res = title;
    if ( !_userTitle.isEmpty() )
        res = _userTitle + " - " + res;
    return res;
}*/

void TESession::monitorTimerDone()
{
  //FIXME: The idea here is that the notification popup will appear to tell the user than output from
  //the terminal has stopped and the popup will disappear when the user activates the session.
  //
  //This breaks with the addition of multiple views of a session.  The popup should disappear
  //when any of the views of the session becomes active
  if (monitorSilence) {
    KNotification::event("Silence", i18n("Silence in session '%1'", title), QPixmap(), 
                    QApplication::activeWindow(), 
                    KNotification::CloseWhenWidgetActivated);
    emit notifySessionState(this,NOTIFYSILENCE);
  }
  else
  {
    emit notifySessionState(this,NOTIFYNORMAL);
  }
  
  notifiedActivity=false;
}

void TESession::notifySessionState(int state)
{
  Q_ASSERT( primaryView() );

  if (state==NOTIFYBELL) 
  {
    primaryView()->Bell(em->isConnected(),i18n("Bell in session '%1'", title));
  } 
  else if (state==NOTIFYACTIVITY) 
  {
    if (monitorSilence) {
      monitorTimer->setSingleShot(true);
      monitorTimer->start(silence_seconds*1000);
    }
    
    //FIXME:  See comments in TESession::monitorTimerDone()
    if (!notifiedActivity) {
      KNotification::event("Activity", i18n("Activity in session '%1'", title), QPixmap(),
                      QApplication::activeWindow(), 
      KNotification::CloseWhenWidgetActivated);
      notifiedActivity=true;
    }
      monitorTimer->setSingleShot(true);
      monitorTimer->start(silence_seconds*1000);
  }

  if ( state==NOTIFYACTIVITY && !monitorActivity )
          state = NOTIFYNORMAL;
  if ( state==NOTIFYSILENCE && !monitorSilence )
          state = NOTIFYNORMAL;

  emit notifySessionState(this, state);
}

void TESession::onContentSizeChange(int height, int width)
{
  updateTerminalSize();
}

void TESession::updateTerminalSize()
{
    QListIterator<TEWidget*> viewIter(_views);

    int minLines = -1;
    int minColumns = -1;

    //select largest number of lines and columns that will fit in all visible views
    while ( viewIter.hasNext() )
    {
        TEWidget* view = viewIter.next();
        if ( view->isHidden() == false )
        {
            minLines = (minLines == -1) ? view->Lines() : qMin( minLines , view->Lines() );
            minColumns = (minColumns == -1) ? view->Columns() : qMin( minColumns , view->Columns() );
        }
    }  

    if ( minLines != -1 && minColumns != -1 )
    {
        em->onImageSizeChange( minLines , minColumns );
        sh->setSize( minLines , minColumns );
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
  Q_ASSERT( primaryView() );

  emit disableMasterModeConnections();
  setListenToKeyPress(true);
  primaryView()->emitText(text);
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
  delete em;
  delete sh;
  delete navItem;
  delete zmodemProc;

  QListIterator<TEWidget*> viewIter(_views);
  while (viewIter.hasNext())
      viewIter.next()->deleteLater();
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
  emit processExited();
  emit done(this);
}

void TESession::done(int exitStatus)
{
  if (!autoClose)
  {
    _userTitle = i18n("<Finished>");
    emit updateTitle();
    return;
  }
  if (!wantedClose && (exitStatus || sh->signalled()))
  {
    QString message;

    if (sh->normalExit())
      message = i18n("Session '%1' exited with status %2.", title, exitStatus);
    else if (sh->signalled())
    {
      if (sh->coreDumped())
        message = i18n("Session '%1' exited with signal %2 and dumped core.", title, sh->exitSignal());
      else
        message = i18n("Session '%1' exited with signal %2.", title, sh->exitSignal());
    }
    else
        message = i18n("Session '%1' exited unexpectedly.", title);

    //FIXME: See comments in TESession::monitorTimerDone()
    KNotification::event("Finished", message , QPixmap(),
                         QApplication::activeWindow(), 
                         KNotification::CloseWhenWidgetActivated);
  }
  emit processExited();
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

//int TESession::schemaNo()
//{
//  return schema_no;
//}

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

const QString & TESession::Term() const
{
  return term;
}

const QString & TESession::SessionId() const
{
  return sessionId;
}

/*void TESession::setSchemaNo(int sn)
{
  schema_no = sn;
}*/

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
  //kDebug(1211)<<"Session setTitle " <<  title <<endl;
}

const QString& TESession::Title() const
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
  //kDebug(1211)<<"Session setIconText " <<  iconText <<endl;
}

const QString& TESession::IconName() const
{
  return iconName;
}

const QString& TESession::IconText() const
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

QStringList TESession::getArgs()
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

  notifySessionState(NOTIFYNORMAL);
}

void TESession::setMonitorSilence(bool _monitor)
{
  if (monitorSilence==_monitor)
    return;

  monitorSilence=_monitor;
  if (monitorSilence) {
    monitorTimer->setSingleShot(true);
    monitorTimer->start(silence_seconds*1000);
  }
  else
    monitorTimer->stop();

  notifySessionState(NOTIFYNORMAL);
}

void TESession::setMonitorSilenceSeconds(int seconds)
{
  silence_seconds=seconds;
  if (monitorSilence) {
    monitorTimer->setSingleShot(true);
    monitorTimer->start(silence_seconds*1000);
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

  zmodemProgress = new ZModemDialog(QApplication::activeWindow(), false,
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
  QByteArray msg(data, len+1);
  while(!msg.isEmpty())
  {
     int i = msg.indexOf('\015');
     int j = msg.indexOf('\012');
     QByteArray txt;
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
  QByteArray ba( data, len );
  zmodemProc->writeStdin( ba );
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

void TESession::enableFullScripting(bool b)
{
    assert(!(fullScripting && !b) && "fullScripting can't be disabled");
    if (!fullScripting && b)
        (void)new SessionScriptingAdaptor(this);
}

void TESession::onRcvBlock( const char* buf, int len )
{
    em->onRcvBlock( buf, len );
    emit receivedData( QString::fromLatin1( buf, len ) );
}

void TESession::print( QPainter &paint, bool friendly, bool exact )
{
    Q_ASSERT( primaryView() );

    primaryView()->print(paint, friendly, exact);
}

ColorSchema* TESession::schema()
{
    return _colorScheme;
}

/*QString TESession::schema()
{
  QString currentSchema;
  emit getSessionSchema(this, currentSchema);
  return currentSchema;
}*/

//void TESession::setSchema(const QString &schema)
//{
//  emit setSessionSchema(this, schema);
//}

void TESession::setSchema(ColorSchema* schema)
{
    _colorScheme = schema;

    //update color scheme for attached views
    QListIterator<TEWidget*> viewIter(_views);
    while (viewIter.hasNext())
    {
        viewIter.next()->setColorTable(schema->table());
    }
}

QString TESession::font()
{
  Q_ASSERT( primaryView() );

  return primaryView()->getVTFont().toString();
}

void TESession::setFont(const QString &font)
{
  Q_ASSERT( primaryView() );

  QFont tmp;
  if (tmp.fromString(font))
    primaryView()->setVTFont(tmp);
  else
    kWarning()<<"unknown font: "<<font<<endl;
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

#include "TESession.moc"

/*
    This file is part of Konsole, an X _terminal.

    Copyright (C) 2006-2007 by Robert Knight <robertknight@gmail.com>
    Copyright (C) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

    This program is free software; you can redistribute it and/or modify
    it under the _terms of the GNU General Public License as published by
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
#include <QTime>

// KDE
#include <KDebug>
#include <KLocale>
#include <KMessageBox>
#include <KNotification>
#include <K3ProcIO>
#include <KRun>
#include <kshell.h>
#include <KStandardDirs>

// Konsole
#include <config-konsole.h>
#include "ColorScheme.h"
//TODO - Re-add adaptors
//#include "sessionadaptor.h"
//#include "sessionscriptingadaptor.h"
#include "ZModemDialog.h"

#include "Session.h"

using namespace Konsole;

int Session::lastSessionId = 0;

Session::Session() : 
    _shellProcess(0)
   , _emulation(0)
   , connected(true)
   , monitorActivity(false)
   , monitorSilence(false)
   , notifiedActivity(false)
   , masterMode(false)
   , autoClose(true)
   , wantedClose(false)
   //, schema_no(0)
   , _fontNo(3)
   , _silenceSeconds(10)
   , _addToUtmp(true)
   , _flowControl(true)
   , _fullScripting(false)
   , _winId(0)
   , _sessionId(0)
   , _zmodemBusy(false)
   , _zmodemProc(0)
   , _zmodemProgress(0)
   , _encoding_no(0)
{
    //prepare DBus communication
    //TODO - Re-add Session adaptor

    // TODO
    // numeric session identifier exposed via DBus isn't very user-friendly, but is this an issue? 
    _sessionId = ++lastSessionId; 
    //QDBusConnection::sessionBus().registerObject(QLatin1String("/Sessions/session")+QString::number(_sessionId), this);

    //create teletype for I/O with shell process
    _shellProcess = new Pty();

    //create emulation backend
    _emulation = new Vt102Emulation();

    connect( _emulation, SIGNAL( changeTitle( int, const QString & ) ),
           this, SLOT( setUserTitle( int, const QString & ) ) );
    connect( _emulation, SIGNAL( notifySessionState(int) ),
           this, SLOT( notifySessionState(int) ) );
    connect( _emulation, SIGNAL( zmodemDetected() ), this, SLOT(slotZModemDetected()));
    connect( _emulation, SIGNAL( changeTabTextColor( int ) ),
           this, SLOT( changeTabTextColor( int ) ) );

   
    //connect teletype to emulation backend 
    _shellProcess->useUtf8(_emulation->utf8());
    
    connect( _shellProcess,SIGNAL(block_in(const char*,int)),this,
            SLOT(onReceiveBlock(const char*,int)) );
    connect( _emulation,SIGNAL(sendBlock(const char*,int)),_shellProcess,
            SLOT(send_bytes(const char*,int)) );
    connect( _emulation,SIGNAL(lockPty(bool)),_shellProcess,SLOT(lockPty(bool)) );
    connect( _emulation,SIGNAL(useUtf8(bool)),_shellProcess,SLOT(useUtf8(bool)) );

    
    connect( _shellProcess,SIGNAL(done(int)), this,SLOT(done(int)) );
 
    //setup timer for monitoring session activity
    monitorTimer = new QTimer(this);
    connect(monitorTimer, SIGNAL(timeout()), this, SLOT(monitorTimerDone()));

    //TODO: Investigate why a single-shot timer is used here 
    if (!_shellProcess->error().isEmpty())
        QTimer::singleShot(0, this, SLOT(ptyError()));
}

void Session::setType(const QString& type) { _type = type; }
QString Session::type() const { return _type; }

void Session::setProgram(const QString& program)
{
    _program = program;
}

void Session::setArguments(const QStringList& arguments)
{
    _arguments = arguments;
}

void Session::ptyError()
{
  // FIXME:  _shellProcess->error() is always empty
  if ( _shellProcess->error().isEmpty() )
  {
    KMessageBox::error( QApplication::activeWindow() ,
       i18n("Konsole is unable to open a PTY (pseudo teletype)."  
            "It is likely that this is due to an incorrect configuration" 
            "of the PTY devices.  Konsole needs to have read/write access" 
            "to the PTY devices."),
       i18n("A Fatal Error Has Occurred") );
  }
  else
    KMessageBox::error(QApplication::activeWindow(), _shellProcess->error());
  
  emit done(this);
}

QList<TerminalDisplay*> Session::views() const
{
    return _views;
}

void Session::addView(TerminalDisplay* widget)
{
     Q_ASSERT( !_views.contains(widget) );

    _views.append(widget);

    if ( _emulation != 0 )
    {
        // connect emulation - view signals and slots
        connect( widget , SIGNAL(keyPressedSignal(QKeyEvent*)) , _emulation ,
               SLOT(onKeyPress(QKeyEvent*)) );
        connect( widget , SIGNAL(mouseSignal(int,int,int,int)) , _emulation , 
               SLOT(onMouse(int,int,int,int)) );
        connect( widget , SIGNAL(sendStringToEmu(const char*)) , _emulation ,
               SLOT(sendString(const char*)) ); 

        // allow emulation to notify view when the foreground process 
        // indicates whether or not it is interested in mouse signals
        connect( _emulation , SIGNAL(programUsesMouse(bool)) , widget , 
               SLOT(setUsesMouse(bool)) );
    }

    widget->setScreenWindow(_emulation->createWindow());

    //connect view signals and slots
    QObject::connect( widget ,SIGNAL(changedContentSizeSignal(int,int)),this,
                    SLOT(onContentSizeChange(int,int)));

    QObject::connect( widget ,SIGNAL(destroyed(QObject*)) , this , 
                    SLOT(viewDestroyed(QObject*)) );

}

void Session::viewDestroyed(QObject* view)
{
    TerminalDisplay* display = (TerminalDisplay*)view;
    
    Q_ASSERT( _views.contains(display) );

    removeView(display);
}

void Session::removeView(TerminalDisplay* widget)
{
    _views.removeAll(widget);

    if ( _emulation != 0 )
    {
        // disconnect
        //  - key presses signals from widget 
        //  - mouse activity signals from widget
        //  - string sending signals from widget
        //
        //  ... and any other signals connected in addView()
        disconnect( widget, 0, _emulation, 0);
        
        // disconnect state change signals emitted by emulation
        disconnect( _emulation , 0 , widget , 0);
    }
}

void Session::run()
{
  //check that everything is in place to run the session
  if (_program.isEmpty())
      kDebug() << "Session::run() - program to run not set." << endl;
  if (_arguments.isEmpty())
      kDebug() << "Session::run() - no command line arguments specified." << endl;
  
  // Upon a KPty error, there is no description on what that error was...
  // Check to see if the given program is executable.
  QString exec = QFile::encodeName(_program);
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
  if (!_initial_cwd.isEmpty())
     QDir::setCurrent(_initial_cwd);
  _shellProcess->setXonXoff(_flowControl);

  int result = _shellProcess->run(QFile::encodeName(_program), 
                                  _arguments, 
                                  _term.toLatin1(),
                                  _winId, 
                                  _addToUtmp,
                                  dbusService.toLatin1(),
                                  (QLatin1String("/Sessions/") + 
                                   QString::number(_sessionId)).toLatin1());

  if (result < 0) {     // Error in opening pseudo teletype
    kWarning()<<"Unable to open a pseudo teletype!"<<endl;
    QTimer::singleShot(0, this, SLOT(ptyError()));
  }
  _shellProcess->setErase(_emulation->getErase());

  if (!_initial_cwd.isEmpty())
     QDir::setCurrent(cwd_save);
  else
     _initial_cwd=cwd_save;

  _shellProcess->setWriteable(false);  // We are reachable via kwrited.

}

void Session::changeTabTextColor( int color )
{
    emit changeTabTextColor( this, color );
}

void Session::setUserTitle( int what, const QString &caption )
{
	bool modified = false; //set to true if anything is actually changed (eg. old _title != new _title )
	
    // (btw: what=0 changes _title and icon, what=1 only icon, what=2 only _title
    if ((what == 0) || (what == 2)) {
       	if ( _userTitle != caption ) {
			_userTitle = caption;
			modified = true;
		}
    }
    
    if ((what == 0) || (what == 1))
	{
		if ( _iconText != caption ) {
       		_iconText = caption;
			modified = true;
		}
	}
	
    if (what == 11) {
      QString colorString = caption.section(';',0,0);
      kDebug() << __FILE__ << __LINE__ << ": setting background colour to " << colorString << endl;
      QColor backColor = QColor(colorString);
      if (backColor.isValid()){// change color via \033]11;Color\007
	if (backColor != _modifiedBackground) {
	    _modifiedBackground = backColor;
	    
        QListIterator<TerminalDisplay*> viewIter(_views);
        while (viewIter.hasNext())
            viewIter.next()->setDefaultBackColor(backColor);
	}
      }
    }
    
	if (what == 30) {
		if ( _title != caption ) {
       		renameSession(caption);
			return;
		}
	}
	
    if (what == 31) {
       _cwd=caption;
       _cwd=_cwd.replace( QRegExp("^~"), QDir::homePath() );
       emit openUrlRequest(_cwd);
	}
	
    if (what == 32) { // change icon via \033]32;Icon\007
    	if ( _iconName != caption ) {
	   		_iconName = caption;

            QListIterator< TerminalDisplay* > viewIter(_views);
            while (viewIter.hasNext())
       		    viewIter.next()->update();
			modified = true;
		}
    }

	if ( modified )
    	emit updateTitle();
}

QString Session::userTitle() const
{
    return _userTitle;
}

QString Session::displayTitle() const
{
    if (!_userTitle.isEmpty())
        return _userTitle;
    else
        return title();
}

void Session::monitorTimerDone()
{
  //FIXME: The idea here is that the notification popup will appear to tell the user than output from
  //the _terminal has stopped and the popup will disappear when the user activates the session.
  //
  //This breaks with the addition of multiple views of a session.  The popup should disappear
  //when any of the views of the session becomes active
  if (monitorSilence) {
    KNotification::event("Silence", i18n("Silence in session '%1'", _title), QPixmap(), 
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

void Session::notifySessionState(int state)
{
  if (state==NOTIFYBELL) 
  {
      emit bellRequest( i18n("Bell in session '%1'",_title) );
  } 
  else if (state==NOTIFYACTIVITY) 
  {
    if (monitorSilence) {
      monitorTimer->setSingleShot(true);
      monitorTimer->start(_silenceSeconds*1000);
    }
    
    //FIXME:  See comments in Session::monitorTimerDone()
    if (!notifiedActivity) {
      KNotification::event("Activity", i18n("Activity in session '%1'", _title), QPixmap(),
                      QApplication::activeWindow(), 
      KNotification::CloseWhenWidgetActivated);
      notifiedActivity=true;
    }
      monitorTimer->setSingleShot(true);
      monitorTimer->start(_silenceSeconds*1000);
  }

  if ( state==NOTIFYACTIVITY && !monitorActivity )
          state = NOTIFYNORMAL;
  if ( state==NOTIFYSILENCE && !monitorSilence )
          state = NOTIFYNORMAL;

  emit notifySessionState(this, state);
}

void Session::onContentSizeChange(int /*height*/, int /*width*/)
{
  updateTerminalSize();
}

void Session::updateTerminalSize()
{
    QListIterator<TerminalDisplay*> viewIter(_views);

    int minLines = -1;
    int minColumns = -1;

    //select largest number of lines and columns that will fit in all visible views
    while ( viewIter.hasNext() )
    {
        TerminalDisplay* view = viewIter.next();
        if ( view->isHidden() == false )
        {
            minLines = (minLines == -1) ? view->lines() : qMin( minLines , view->lines() );
            minColumns = (minColumns == -1) ? view->columns() : qMin( minColumns , view->columns() );
        }
    }  

    // backend emulation must have a _terminal of at least 1 column x 1 line in size
    if ( minLines > 0 && minColumns > 0 )
    {
        _emulation->onImageSizeChange( minLines , minColumns );
        _shellProcess->setSize( minLines , minColumns );
    }
}

bool Session::sendSignal(int signal)
{
  return _shellProcess->kill(signal);
}

bool Session::closeSession()
{
  autoClose = true;
  wantedClose = true;
  if (!_shellProcess->isRunning() || !sendSignal(SIGHUP))
  {
     // Forced close.
     QTimer::singleShot(1, this, SLOT(done()));
  }
  return true;
}

void Session::feedSession(const QString &text)
{
  emit disableMasterModeConnections();
  setListenToKeyPress(true);
  _emulation->sendText(text);
  setListenToKeyPress(false);
  emit enableMasterModeConnections();
}

void Session::sendSession(const QString &text)
{
  QString newtext=text;
  newtext.append("\r");
  feedSession(newtext);
}

void Session::renameSession(const QString &name)
{
  _title=name;
  emit renameSession(this,name);
}

Session::~Session()
{
  delete _emulation;
  delete _shellProcess;
  delete _zmodemProc;

  QListIterator<TerminalDisplay*> viewIter(_views);
  while (viewIter.hasNext())
      viewIter.next()->deleteLater();
}

/*void Session::setConnect(bool c)
{
  connected=c;
  _emulation->setConnect(c);
  setListenToKeyPress(c);
}*/

void Session::setListenToKeyPress(bool l)
{
  _emulation->setListenToKeyPress(l);
}

void Session::done() {
  emit processExited();
  emit done(this);
}

void Session::done(int exitStatus)
{
  if (!autoClose)
  {
    _userTitle = i18n("<Finished>");
    emit updateTitle();
    return;
  }
  if (!wantedClose && (exitStatus || _shellProcess->signalled()))
  {
    QString message;

    if (_shellProcess->normalExit())
      message = i18n("Session '%1' exited with status %2.", _title, exitStatus);
    else if (_shellProcess->signalled())
    {
      if (_shellProcess->coreDumped())
        message = i18n("Session '%1' exited with signal %2 and dumped core.", _title, _shellProcess->exitSignal());
      else
        message = i18n("Session '%1' exited with signal %2.", _title, _shellProcess->exitSignal());
    }
    else
        message = i18n("Session '%1' exited unexpectedly.", _title);

    //FIXME: See comments in Session::monitorTimerDone()
    KNotification::event("Finished", message , QPixmap(),
                         QApplication::activeWindow(), 
                         KNotification::CloseWhenWidgetActivated);
  }
  emit processExited();
  emit done(this);
}

void Session::terminate()
{
  delete this;
}

Emulation* Session::emulation() const
{
  return _emulation;
}

// following interfaces might be misplaced ///

int Session::encodingNo() const
{
  return _encoding_no;
}

int Session::keymapNo() const
{
  return _emulation->keymapNo();
}

QString Session::keymap() const
{
  return _emulation->keymap();
}

int Session::fontNo() const
{
  return _fontNo;
}

const QString& Session::terminalType() const
{
  return _term;
}

void Session::setTerminalType(const QString& _terminalType)
{
    _term = _terminalType;
}

int Session::sessionId() const
{
  return _sessionId;
}

void Session::setEncodingNo(int index)
{
  _encoding_no = index;
}

void Session::setKeymapNo(int kn)
{
  _emulation->setKeymap(kn);
}

void Session::setKeymap(const QString &id)
{
  _emulation->setKeymap(id);
}

void Session::setFontNo(int fn)
{
  _fontNo = fn;
}

void Session::setTitle(const QString& title)
{
    if ( title != _title )
    {
        _title = title;
        emit updateTitle();
    }
}

const QString& Session::title() const
{
  return _title;
}

void Session::setIconName(const QString& iconName)
{
    if ( iconName != _iconName )
    {
        _iconName = iconName;
        emit updateTitle();
    }
}

void Session::setIconText(const QString& iconText)
{
  _iconText = iconText;
  //kDebug(1211)<<"Session setIconText " <<  _iconText <<endl;
}

const QString& Session::iconName() const
{
  return _iconName;
}

const QString& Session::iconText() const
{
  return _iconText;
}

bool Session::testAndSetStateIconName (const QString& newname)
{
  if (newname != _stateIconName)
    {
      _stateIconName = newname;
      return true;
    }
  return false;
}

void Session::setHistory(const HistoryType &hType)
{
  _emulation->setHistory(hType);
}

const HistoryType& Session::history()
{
  return _emulation->history();
}

void Session::clearHistory()
{
  if (history().isEnabled()) {
    int histSize = history().maximumLineCount();
    setHistory(HistoryTypeNone());
    if (histSize)
      setHistory(HistoryTypeBuffer(histSize));
    else
      setHistory(HistoryTypeFile());
  }
}

QStringList Session::getArgs()
{
  return _arguments;
}

QString Session::getPgm()
{
  return _program;
}

QString Session::currentWorkingDirectory()
{
  Q_ASSERT(false);

  return QString();
}

bool Session::isMonitorActivity() const { return monitorActivity; }
bool Session::isMonitorSilence()  const { return monitorSilence; }
bool Session::isMasterMode()      const { return masterMode; }

void Session::setMonitorActivity(bool _monitor)
{
  monitorActivity=_monitor;
  notifiedActivity=false;

  notifySessionState(NOTIFYNORMAL);
}

void Session::setMonitorSilence(bool _monitor)
{
  if (monitorSilence==_monitor)
    return;

  monitorSilence=_monitor;
  if (monitorSilence) {
    monitorTimer->setSingleShot(true);
    monitorTimer->start(_silenceSeconds*1000);
  }
  else
    monitorTimer->stop();

  notifySessionState(NOTIFYNORMAL);
}

void Session::setMonitorSilenceSeconds(int seconds)
{
  _silenceSeconds=seconds;
  if (monitorSilence) {
    monitorTimer->setSingleShot(true);
    monitorTimer->start(_silenceSeconds*1000);
  }
}

void Session::setMasterMode(bool _master)
{
  masterMode=_master;
}

void Session::setAddToUtmp(bool set)
{
  _addToUtmp = set;
}

void Session::setXonXoff(bool set)
{
  _flowControl = set;
}

void Session::slotZModemDetected()
{
  if (!_zmodemBusy)
  {
    QTimer::singleShot(10, this, SLOT(emitZModemDetected()));
    _zmodemBusy = true;
  }
}

void Session::emitZModemDetected()
{
  emit zmodemDetected(this);
}

void Session::cancelZModem()
{
  _shellProcess->send_bytes("\030\030\030\030", 4); // Abort
  _zmodemBusy = false;
}

void Session::startZModem(const QString &zmodem, const QString &dir, const QStringList &list)
{
  _zmodemBusy = true;
  _zmodemProc = new K3ProcIO;

  (*_zmodemProc) << zmodem << "-v";
  for(QStringList::ConstIterator it = list.begin();
      it != list.end();
      ++it)
  {
     (*_zmodemProc) << (*it);
  }

  if (!dir.isEmpty())
     _zmodemProc->setWorkingDirectory(dir);
  _zmodemProc->start(K3ProcIO::NotifyOnExit, false);

  // Override the read-processing of K3ProcIO
  disconnect(_zmodemProc,SIGNAL (receivedStdout (K3Process *, char *, int)), 0, 0);
  connect(_zmodemProc,SIGNAL (receivedStdout (K3Process *, char *, int)),
          this, SLOT(zmodemSendBlock(K3Process *, char *, int)));
  connect(_zmodemProc,SIGNAL (receivedStderr (K3Process *, char *, int)),
          this, SLOT(zmodemStatus(K3Process *, char *, int)));
  connect(_zmodemProc,SIGNAL (processExited(K3Process *)),
          this, SLOT(zmodemDone()));

  disconnect( _shellProcess,SIGNAL(block_in(const char*,int)), this, SLOT(onReceiveBlock(const char*,int)) );
  connect( _shellProcess,SIGNAL(block_in(const char*,int)), this, SLOT(zmodemRcvBlock(const char*,int)) );
  connect( _shellProcess,SIGNAL(buffer_empty()), this, SLOT(zmodemContinue()));

  _zmodemProgress = new ZModemDialog(QApplication::activeWindow(), false,
                                    i18n("ZModem Progress"));

  connect(_zmodemProgress, SIGNAL(user1Clicked()),
          this, SLOT(zmodemDone()));

  _zmodemProgress->show();
}

void Session::zmodemSendBlock(K3Process *, char *data, int len)
{
  _shellProcess->send_bytes(data, len);
//  qWarning("<-- %d bytes", len);
  if (_shellProcess->buffer_full())
  {
    _zmodemProc->suspend();
//    qWarning("ZModem suspend");
  }
}

void Session::zmodemContinue()
{
  _zmodemProc->resume();
//  qWarning("ZModem resume");
}

void Session::zmodemStatus(K3Process *, char *data, int len)
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
       _zmodemProgress->addProgressText(QString::fromLocal8Bit(txt));
  }
}

void Session::zmodemRcvBlock(const char *data, int len)
{
  QByteArray ba( data, len );
  _zmodemProc->writeStdin( ba );
//  qWarning("--> %d bytes", len);
}

void Session::zmodemDone()
{
  if (_zmodemProc)
  {
    delete _zmodemProc;
    _zmodemProc = 0;
    _zmodemBusy = false;

    disconnect( _shellProcess,SIGNAL(block_in(const char*,int)), this ,SLOT(zmodemRcvBlock(const char*,int)) );
    disconnect( _shellProcess,SIGNAL(buffer_empty()), this, SLOT(zmodemContinue()));
    connect( _shellProcess,SIGNAL(block_in(const char*,int)), this, SLOT(onReceiveBlock(const char*,int)) );

    _shellProcess->send_bytes("\030\030\030\030", 4); // Abort
    _shellProcess->send_bytes("\001\013\n", 3); // Try to get prompt back
    _zmodemProgress->done();
  }
}

void Session::enableFullScripting(bool b)
{
  assert( 0 );
  // TODO Re-add adaptors
  //  assert(!(_fullScripting && !b) && "_fullScripting can't be disabled");
  //  if (!_fullScripting && b)
  //      (void)new SessionScriptingAdaptor(this);
}

void Session::onReceiveBlock( const char* buf, int len )
{
    _emulation->onReceiveBlock( buf, len );
    emit receivedData( QString::fromLatin1( buf, len ) );
}

QString Session::encoding()
{
  return _emulation->codec()->name();
}

void Session::setEncoding(const QString &encoding)
{
  emit setSessionEncoding(this, encoding);
}

QString Session::keytab()
{
   return keymap();
}

void Session::setKeytab(const QString &keytab)
{
   setKeymap(keytab);
   emit updateSessionConfig(this);
}

QSize Session::size()
{
  return _emulation->imageSize();
}

void Session::setSize(QSize size)
{
  if ((size.width() <= 1) || (size.height() <= 1))
     return;

  emit resizeSession(this, size);
}

#include "Session.moc"

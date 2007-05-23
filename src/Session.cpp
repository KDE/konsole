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

// Own
#include "Session.h"

// Standard
#include <assert.h>
#include <stdlib.h>

// Qt
#include <QtGui/QApplication>
#include <QtCore/QByteRef>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QRegExp>
#include <QtCore/QStringList>
#include <QtDBus/QtDBus>
#include <QtCore/QDate>

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
//TODO - Re-add adaptors
//#include "sessionadaptor.h"
//#include "sessionscriptingadaptor.h"
#include "Pty.h"
#include "TerminalDisplay.h"
#include "Vt102Emulation.h"
#include "ZModemDialog.h"


using namespace Konsole;

int Session::lastSessionId = 0;

Session::Session() : 
    _shellProcess(0)
   , _emulation(0)
   , _monitorActivity(false)
   , _monitorSilence(false)
   , _notifiedActivity(false)
   , _autoClose(true)
   , _wantedClose(false)
   , _silenceSeconds(10)
   , _addToUtmp(true)
   , _flowControl(true)
   , _fullScripting(false)
   , _winId(0)
   , _sessionId(0)
   , _zmodemBusy(false)
   , _zmodemProc(0)
   , _zmodemProgress(0)
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

    connect( _emulation, SIGNAL( titleChanged( int, const QString & ) ),
           this, SLOT( setUserTitle( int, const QString & ) ) );
    connect( _emulation, SIGNAL( stateSet(int) ),
           this, SLOT( activityStateSet(int) ) );
    connect( _emulation, SIGNAL( zmodemDetected() ), this , 
            SLOT( fireZModemDetected() ) );
    connect( _emulation, SIGNAL( changeTabTextColorRequest( int ) ),
           this, SIGNAL( changeTabTextColorRequest( int ) ) );

   
    //connect teletype to emulation backend 
    _shellProcess->setUtf8Mode(_emulation->utf8());
    
    connect( _shellProcess,SIGNAL(receivedData(const char*,int)),this,
            SLOT(onReceiveBlock(const char*,int)) );
    connect( _emulation,SIGNAL(sendData(const char*,int)),_shellProcess,
            SLOT(sendData(const char*,int)) );
    connect( _emulation,SIGNAL(lockPtyRequest(bool)),_shellProcess,SLOT(lockPty(bool)) );
    connect( _emulation,SIGNAL(useUtf8Request(bool)),_shellProcess,SLOT(setUtf8Mode(bool)) );

    
    connect( _shellProcess,SIGNAL(done(int)), this,SLOT(done(int)) );
 
    //setup timer for monitoring session activity
    _monitorTimer = new QTimer(this);
    connect(_monitorTimer, SIGNAL(timeout()), this, SLOT(monitorTimerDone()));
}

bool Session::running() const
{
    return _shellProcess->isRunning();
}

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
// Mysterious errors about PTYs used to be shown even when there was nothing wrong
// with the PTY itself because the code which starts the session ( in ::run() ) assumes
// that if starting the shell process fails then a PTY error is the problem, when
// there are other things that can go wrong as well

#if 0
    // FIXME:  _shellProcess->error() is always empty
  if ( _shellProcess->error().isEmpty() )
  {
    KMessageBox::error( QApplication::activeWindow() ,
       i18n("Konsole is unable to open a PTY (pseudo teletype). "
            "It is likely that this is due to an incorrect configuration "
            "of the PTY devices.  Konsole needs to have read/write access "
            "to the PTY devices."),
       i18n("A Fatal Error Has Occurred") );
  }
  else
    KMessageBox::error(QApplication::activeWindow(), _shellProcess->error());
  
  emit finished();
#endif
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
               SLOT(sendKeyEvent(QKeyEvent*)) );
        connect( widget , SIGNAL(mouseSignal(int,int,int,int)) , _emulation , 
               SLOT(sendMouseEvent(int,int,int,int)) );
        connect( widget , SIGNAL(sendStringToEmu(const char*)) , _emulation ,
               SLOT(sendString(const char*)) ); 

        // allow emulation to notify view when the foreground process 
        // indicates whether or not it is interested in mouse signals
        connect( _emulation , SIGNAL(programUsesMouse(bool)) , widget , 
               SLOT(setUsesMouse(bool)) );
  
        widget->setScreenWindow(_emulation->createWindow());
    }

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

  // if 'exec' is not specified, fall back to shell
  if ( exec.isEmpty() )
      exec = getenv("SHELL");

  // if no arguments are specified, fall back to shell
  QStringList arguments =  _arguments.join(QChar(' ')).isEmpty() ? 
                                    QStringList() << exec : _arguments;


  exec = KRun::binaryName(exec, false);
  exec = KShell::tildeExpand(exec);
  QString pexec = KGlobal::dirs()->findExe(exec);
  if ( pexec.isEmpty() ) {
    kError()<<"can not execute "<<exec<<endl;
    QTimer::singleShot(1, this, SIGNAL(finished()));
    return;
  }

  QString dbusService = QDBusConnection::sessionBus().baseService();
  QString cwd_save = QDir::currentPath();
  if (!_initialWorkingDir.isEmpty())
     QDir::setCurrent(_initialWorkingDir);
  _shellProcess->setXonXoff(_flowControl);

  int result = _shellProcess->start(QFile::encodeName(_program), 
                                  arguments, 
                                  _term,
                                  _winId, 
                                  _addToUtmp,
                                  dbusService,
                                  (QLatin1String("/Sessions/") + 
                                   QString::number(_sessionId)));

  if (result < 0) 
  {
    return;
    //QTimer::singleShot(0, this, SLOT(ptyError()));
  }
  _shellProcess->setErase(_emulation->getErase());

  if (!_initialWorkingDir.isEmpty())
     QDir::setCurrent(cwd_save);
  else
     _initialWorkingDir=cwd_save;

  _shellProcess->setWriteable(false);  // We are reachable via kwrited.

}

void Session::setUserTitle( int what, const QString &caption )
{
    //set to true if anything is actually changed (eg. old _nameTitle != new _nameTitle )
	bool modified = false; 
	
    // (btw: what=0 changes _nameTitle and icon, what=1 only icon, what=2 only _nameTitle
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
	if (backColor != _modifiedBackground) 
    {
	    _modifiedBackground = backColor;
	   
        // bail out here until the code to connect the terminal display
        // to the changeBackgroundColor() signal has been written
        // and tested - just so we don't forget to do this.
        Q_ASSERT( 0 );

        emit changeBackgroundColorRequest(backColor);
	}
      }
    }
    
	if (what == 30) {
		if ( _nameTitle != caption ) {
       		setTitle(Session::NameRole,caption);
			return;
		}
	}
	
    if (what == 31) {
       QString cwd=caption;
       cwd=cwd.replace( QRegExp("^~"), QDir::homePath() );
       emit openUrlRequest(cwd);
	}
	
    if (what == 32) { // change icon via \033]32;Icon\007
    	if ( _iconName != caption ) {
	   		_iconName = caption;

			modified = true;
		}
    }

	if ( modified )
    	emit titleChanged();
}

QString Session::userTitle() const
{
    return _userTitle;
}
void Session::setTabTitleFormat(TabTitleContext context , const QString& format)
{
    if ( context == LocalTabTitle )
        _localTabTitleFormat = format;
    else if ( context == RemoteTabTitle )
        _remoteTabTitleFormat = format;
}
QString Session::tabTitleFormat(TabTitleContext context) const
{
    if ( context == LocalTabTitle )
        return _localTabTitleFormat;
    else if ( context == RemoteTabTitle )
        return _remoteTabTitleFormat;

    return QString();
}

void Session::monitorTimerDone()
{
  //FIXME: The idea here is that the notification popup will appear to tell the user than output from
  //the _terminal has stopped and the popup will disappear when the user activates the session.
  //
  //This breaks with the addition of multiple views of a session.  The popup should disappear
  //when any of the views of the session becomes active
  if (_monitorSilence) {
    KNotification::event("Silence", i18n("Silence in session '%1'", _nameTitle), QPixmap(), 
                    QApplication::activeWindow(), 
                    KNotification::CloseWhenWidgetActivated);
    emit stateChanged(NOTIFYSILENCE);
  }
  else
  {
    emit stateChanged(NOTIFYNORMAL);
  }
  
  _notifiedActivity=false;
}

void Session::activityStateSet(int state)
{
  if (state==NOTIFYBELL) 
  {
      emit bellRequest( i18n("Bell in session '%1'",_nameTitle) );
  } 
  else if (state==NOTIFYACTIVITY) 
  {
    if (_monitorSilence) {
      _monitorTimer->setSingleShot(true);
      _monitorTimer->start(_silenceSeconds*1000);
    }
    
    //FIXME:  See comments in Session::monitorTimerDone()
    if (!_notifiedActivity) {
      KNotification::event("Activity", i18n("Activity in session '%1'", _nameTitle), QPixmap(),
                      QApplication::activeWindow(), 
      KNotification::CloseWhenWidgetActivated);
      _notifiedActivity=true;
    }
      _monitorTimer->setSingleShot(true);
      _monitorTimer->start(_silenceSeconds*1000);
  }

  if ( state==NOTIFYACTIVITY && !_monitorActivity )
          state = NOTIFYNORMAL;
  if ( state==NOTIFYSILENCE && !_monitorSilence )
          state = NOTIFYNORMAL;

  emit stateChanged(state);
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
        _emulation->setImageSize( minLines , minColumns );
        _shellProcess->setWindowSize( minLines , minColumns );
    }
}

bool Session::sendSignal(int signal)
{
  return _shellProcess->kill(signal);
}

void Session::close()
{
  _autoClose = true;
  _wantedClose = true;
  if (!_shellProcess->isRunning() || !sendSignal(SIGHUP))
  {
     // Forced close.
     QTimer::singleShot(1, this, SIGNAL(finished()));
  }
}

void Session::sendText(const QString &text) const
{
  _emulation->sendText(text);
}

Session::~Session()
{
  delete _emulation;
  delete _shellProcess;
  delete _zmodemProc;
}

void Session::setProfileKey(const QString& key) 
{ 
    _profileKey = key; 
    emit profileChanged(key);
}
QString Session::profileKey() const { return _profileKey; }

void Session::done(int exitStatus)
{
  if (!_autoClose)
  {
    _userTitle = i18n("<Finished>");
    emit titleChanged();
    return;
  }
  if (!_wantedClose && (exitStatus || _shellProcess->signalled()))
  {
    QString message;

    if (_shellProcess->normalExit())
      message = i18n("Session '%1' exited with status %2.", _nameTitle, exitStatus);
    else if (_shellProcess->signalled())
    {
      if (_shellProcess->coreDumped())
        message = i18n("Session '%1' exited with signal %2 and dumped core.", _nameTitle, _shellProcess->exitSignal());
      else
        message = i18n("Session '%1' exited with signal %2.", _nameTitle, _shellProcess->exitSignal());
    }
    else
        message = i18n("Session '%1' exited unexpectedly.", _nameTitle);

    //FIXME: See comments in Session::monitorTimerDone()
    KNotification::event("Finished", message , QPixmap(),
                         QApplication::activeWindow(), 
                         KNotification::CloseWhenWidgetActivated);
  }
  emit finished();
}

Emulation* Session::emulation() const
{
  return _emulation;
}

QString Session::keyBindings() const
{
  return _emulation->keyBindings();
}

QString Session::terminalType() const
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

void Session::setKeyBindings(const QString &id)
{
  _emulation->setKeyBindings(id);
}

void Session::setTitle(TitleRole role , const QString& newTitle)
{
    if ( title(role) != newTitle )
    {
        if ( role == NameRole )
            _nameTitle = newTitle;
        else if ( role == DisplayedTitleRole )
            _displayTitle = newTitle;

        emit titleChanged();
    }
}

QString Session::title(TitleRole role) const
{
    if ( role == NameRole )
        return _nameTitle;
    else if ( role == DisplayedTitleRole )
        return _displayTitle;
    else
        return QString();
}

void Session::setIconName(const QString& iconName)
{
    if ( iconName != _iconName )
    {
        _iconName = iconName;
        emit titleChanged();
    }
}

void Session::setIconText(const QString& iconText)
{
  _iconText = iconText;
  //kDebug(1211)<<"Session setIconText " <<  _iconText <<endl;
}

QString Session::iconName() const
{
  return _iconName;
}

QString Session::iconText() const
{
  return _iconText;
}

void Session::setHistoryType(const HistoryType &hType)
{
  _emulation->setHistory(hType);
}

const HistoryType& Session::historyType() const
{
  return _emulation->history();
}

void Session::clearHistory()
{
    _emulation->clearHistory();    
}

QStringList Session::arguments() const
{
  return _arguments;
}

QString Session::program() const
{
  return _program;
}

bool Session::isMonitorActivity() const { return _monitorActivity; }
bool Session::isMonitorSilence()  const { return _monitorSilence; }

void Session::setMonitorActivity(bool _monitor)
{
  _monitorActivity=_monitor;
  _notifiedActivity=false;

  activityStateSet(NOTIFYNORMAL);
}

void Session::setMonitorSilence(bool _monitor)
{
  if (_monitorSilence==_monitor)
    return;

  _monitorSilence=_monitor;
  if (_monitorSilence) 
  {
    _monitorTimer->setSingleShot(true);
    _monitorTimer->start(_silenceSeconds*1000);
  }
  else
    _monitorTimer->stop();

  activityStateSet(NOTIFYNORMAL);
}

void Session::setMonitorSilenceSeconds(int seconds)
{
  _silenceSeconds=seconds;
  if (_monitorSilence) {
    _monitorTimer->setSingleShot(true);
    _monitorTimer->start(_silenceSeconds*1000);
  }
}

void Session::setAddToUtmp(bool set)
{
  _addToUtmp = set;
}

void Session::setFlowControlEnabled(bool enabled)
{
  _flowControl = enabled;
}

void Session::fireZModemDetected()
{
  if (!_zmodemBusy)
  {
    QTimer::singleShot(10, this, SIGNAL(zmodemDetected()));
    _zmodemBusy = true;
  }
}

void Session::cancelZModem()
{
  _shellProcess->sendData("\030\030\030\030", 4); // Abort
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
  _shellProcess->sendData(data, len);
//  qWarning("<-- %d bytes", len);
  if (_shellProcess->bufferFull())
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

    _shellProcess->sendData("\030\030\030\030", 4); // Abort
    _shellProcess->sendData("\001\013\n", 3); // Try to get prompt back
    _zmodemProgress->done();
  }
}

void Session::onReceiveBlock( const char* buf, int len )
{
    _emulation->receiveData( buf, len );
    emit receivedData( QString::fromLatin1( buf, len ) );
}

QSize Session::size()
{
  return _emulation->imageSize();
}

void Session::setSize(const QSize& size)
{
  if ((size.width() <= 1) || (size.height() <= 1))
     return;

  emit resizeRequest(size);
}
int Session::foregroundProcessId() const
{
    return _shellProcess->foregroundProcessGroup();
}
int Session::processId() const
{
    return _shellProcess->pid();
}

#include "Session.moc"

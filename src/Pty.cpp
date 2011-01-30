/*
    This file is part of Konsole, an X terminal.
    Copyright 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

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

// Own
#include "Pty.h"

// System
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#ifndef Q_WS_WIN
#include <termios.h>
#include <signal.h>
#endif

// Qt
#include <QtCore/QStringList>
#include <QtCore/QTextCodec>
#include <QtCore/QDebug>

// KDE
#include <KStandardDirs>
#include <KLocale>
#include <KDebug>
#ifndef Q_WS_WIN
#include <KPty>
#include <KPtyDevice>
#endif
#include <kde_file.h>

using namespace Konsole;

void Pty::setWindowSize(int lines, int cols)
{
  _windowColumns = cols;
  _windowLines = lines;
/* FIXME */
#ifndef Q_WS_WIN
  if (pty()->masterFd() >= 0)
    pty()->setWinSize(lines, cols);
#else
    write(QByteArray("\e[") + QString("%1;%2t").arg(lines).arg(cols).toLatin1());
#endif
}
QSize Pty::windowSize() const
{
    return QSize(_windowColumns,_windowLines);
}

void Pty::setFlowControlEnabled(bool enable)
{
  _xonXoff = enable;

#ifndef Q_WS_WIN
  if (pty()->masterFd() >= 0)
  {
    struct ::termios ttmode;
    pty()->tcGetAttr(&ttmode);
    if (!enable)
      ttmode.c_iflag &= ~(IXOFF | IXON);
    else
      ttmode.c_iflag |= (IXOFF | IXON);
    if (!pty()->tcSetAttr(&ttmode))
      kWarning() << "Unable to set terminal attributes.";
  }
#endif
}
bool Pty::flowControlEnabled() const
{
#ifndef Q_WS_WIN
    if (pty()->masterFd() >= 0)
    {
        struct ::termios ttmode;
        pty()->tcGetAttr(&ttmode);
        return ttmode.c_iflag & IXOFF &&
               ttmode.c_iflag & IXON;
    }  
    kWarning() << "Unable to get flow control status, terminal not connected.";
#endif
    return false;
}

void Pty::setUtf8Mode(bool enable)
{
#ifdef IUTF8 // XXX not a reasonable place to check it.
  _utf8 = enable;

  if (pty()->masterFd() >= 0)
  {
    struct ::termios ttmode;
    pty()->tcGetAttr(&ttmode);
    if (!enable)
      ttmode.c_iflag &= ~IUTF8;
    else
      ttmode.c_iflag |= IUTF8;
    if (!pty()->tcSetAttr(&ttmode))
      kWarning() << "Unable to set terminal attributes.";
  }
#endif
}

void Pty::setErase(char erase)
{
  _eraseChar = erase;

#ifndef Q_WS_WIN
  if (pty()->masterFd() >= 0)
  {
    struct ::termios ttmode;
    pty()->tcGetAttr(&ttmode);
    ttmode.c_cc[VERASE] = erase;
    if (!pty()->tcSetAttr(&ttmode))
      kWarning() << "Unable to set terminal attributes.";
  }
#endif
}

char Pty::erase() const
{
#ifndef Q_WS_WIN
    if (pty()->masterFd() >= 0)
    {
        struct ::termios ttyAttributes;
        pty()->tcGetAttr(&ttyAttributes);
        return ttyAttributes.c_cc[VERASE];
    }
#endif
    return _eraseChar;
}

void Pty::addEnvironmentVariables(const QStringList& environment)
{
    QListIterator<QString> iter(environment);
    while (iter.hasNext())
    {
        QString pair = iter.next();

        // split on the first '=' character
        int pos = pair.indexOf('=');
        
        if ( pos >= 0 )
        {
            QString variable = pair.left(pos);
            QString value = pair.mid(pos+1);

            setEnv(variable,value);
        }
    }
}

int Pty::start(const QString& program, 
               const QStringList& programArguments, 
               const QStringList& environment, 
               ulong winid, 
               bool addToUtmp,
               const QString& dbusService, 
               const QString& dbusSession)
{
  clearProgram();

  // For historical reasons, the first argument in programArguments is the 
  // name of the program to execute, so create a list consisting of all
  // but the first argument to pass to setProgram()
  Q_ASSERT(programArguments.count() >= 1);
  setProgram(program.toLatin1(),programArguments.mid(1));

  addEnvironmentVariables(environment);

  if ( !dbusService.isEmpty() )
     setEnv("KONSOLE_DBUS_SERVICE",dbusService);
  if ( !dbusSession.isEmpty() )
     setEnv("KONSOLE_DBUS_SESSION", dbusSession);

  setEnv("WINDOWID", QString::number(winid));

  // unless the LANGUAGE environment variable has been set explicitly
  // set it to a null string
  // this fixes the problem where KCatalog sets the LANGUAGE environment
  // variable during the application's startup to something which
  // differs from LANG,LC_* etc. and causes programs run from
  // the terminal to display messages in the wrong language
  //
  // this can happen if LANG contains a language which KDE
  // does not have a translation for
  //
  // BR:149300
  setEnv("LANGUAGE",QString(),false /* do not overwrite existing value if any */);
#ifndef Q_WS_WIN
  setUseUtmp(addToUtmp);

  struct ::termios ttmode;
  pty()->tcGetAttr(&ttmode);
  if (!_xonXoff)
    ttmode.c_iflag &= ~(IXOFF | IXON);
  else
    ttmode.c_iflag |= (IXOFF | IXON);
#ifdef IUTF8 // XXX not a reasonable place to check it.
  if (!_utf8)
    ttmode.c_iflag &= ~IUTF8;
  else
    ttmode.c_iflag |= IUTF8;
#endif

  if (_eraseChar != 0)
      ttmode.c_cc[VERASE] = _eraseChar;
  
  if (!pty()->tcSetAttr(&ttmode))
    kWarning() << "Unable to set terminal attributes.";

  pty()->setWinSize(_windowLines, _windowColumns);
#endif
  qDebug() << "trying to start process!" << endl;
  KProcess::start();

  if (!waitForStarted())
      return -1;

  return 0;
}

void Pty::setWriteable(bool writeable)
{
#ifndef Q_WS_WIN
  KDE_struct_stat sbuf;
  KDE_stat(pty()->ttyName(), &sbuf);
  if (writeable)
    chmod(pty()->ttyName(), sbuf.st_mode | S_IWGRP);
  else
    chmod(pty()->ttyName(), sbuf.st_mode & ~(S_IWGRP|S_IWOTH));
#endif
}

Pty::Pty(int masterFd, QObject* parent)
#ifndef Q_WS_WIN
    : KPtyProcess(masterFd,parent)
#else
    : KProcess(parent)
#endif
{
    init();
}
Pty::Pty(QObject* parent)
#ifndef Q_WS_WIN
    : KPtyProcess(parent)
#else
    : KProcess(parent)
#endif
{
    init();
}
void Pty::init()
{
  _windowColumns = 0;
  _windowLines = 0;
  _eraseChar = 0;
  _xonXoff = true;
  _utf8 =true;

  connect(pty(), SIGNAL(readyRead()) , this , SLOT(dataReceived()));
#ifndef Q_WS_WIN
  setPtyChannels(KPtyProcess::AllChannels);
#else
  setOutputChannelMode(MergedChannels);
#endif
}


#ifdef Q_WS_WIN
KProcess* Pty::pty()
{
    return this;
}
#endif

Pty::~Pty()
{
}

void Pty::sendData(const char* data, int length)
{
  if (!length)
      return;
  
  if (!pty()->write(data,length)) 
  {
    kWarning() << "Pty::doSendJobs - Could not send input data to terminal process.";
    return;
  }
#ifdef Q_WS_WIN
  emit receivedData(data,length);
#endif
}

void Pty::dataReceived() 
{
    QTextCodec *codec = QTextCodec::codecForName("IBM 850");
    QString string = codec->toUnicode(pty()->readAll());
    QByteArray data = string.toLatin1();
    emit receivedData(data.constData(),data.count());
}

void Pty::lockPty(bool lock)
{
    Q_UNUSED(lock);

// TODO: Support for locking the Pty
  //if (lock)
    //suspend();
  //else
    //resume();
}

int Pty::foregroundProcessGroup() const
{
#ifndef Q_WS_WIN
    int pid = tcgetpgrp(pty()->masterFd());
#else
    int pid = KProcess::pid();
#endif

    if ( pid != -1 )
    {
        return pid;
    } 

    return 0;
}

void Pty::setupChildProcess()
{
#ifndef Q_WS_WIN
    KPtyProcess::setupChildProcess();
    
    // reset all signal handlers
    // this ensures that terminal applications respond to 
    // signals generated via key sequences such as Ctrl+C
    // (which sends SIGINT)
    struct sigaction action;
    sigemptyset(&action.sa_mask);
    action.sa_handler = SIG_DFL;
    action.sa_flags = 0;
    for (int signal=1;signal < NSIG; signal++)
        sigaction(signal,&action,0L);
#endif
}


#include "Pty.moc"

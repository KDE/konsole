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

/* If you're compiling konsole on non-Linux platforms and find
   problems that you can track down to this file, please have
   a look into ../README.ports, too.
*/

/*! \file
*/


/*! \class Pty

    \brief Ptys provide a pseudo terminal connection to a program.

    Although closely related to pipes, these pseudo terminal connections have
    some ability, that makes it necessary to uses them. Most importent, they
    know about changing screen sizes and UNIX job control.

    Within the terminal emulation framework, this class represents the
    host side of the terminal together with the connecting serial line.

    One can create many instances of this class within a program.
    As a side effect of using this class, a signal(2) handler is
    installed on SIGCHLD.

    \par FIXME

    [NOTE: much of the technical stuff below will be replaced by forkpty.]

    publish the SIGCHLD signal if not related to an instance.

    clearify Pty::done vs. Pty::~Pty semantics.
    check if pty is restartable via run after done.

    \par Pseudo terminals

    Pseudo terminals are a unique feature of UNIX, and always come in form of
    pairs of devices (/dev/ptyXX and /dev/ttyXX), which are connected to each
    other by the operating system. One may think of them as two serial devices
    linked by a null-modem cable. Being based on devices the number of
    simultanous instances of this class is (globally) limited by the number of
    those device pairs, which is 256.

    Another technic are UNIX 98 PTY's. These are supported also, and preferred
    over the (obsolete) predecessor.

    There's a sinister ioctl(2), signal(2) and job control stuff
    necessary to make everything work as it should.

    Much of the stuff can be simplified by using openpty from glibc2.
    Compatibility issues with obsolete installations and other unixes
    my prevent this.
*/

// Own
#include "Pty.h"

// System
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>

// Qt
#include <QtCore/QStringList>

// KDE
#include <KStandardDirs>
#include <KLocale>
#include <KDebug>
#include <KPty>

using namespace Konsole;

void Pty::donePty()
{
  emit done(exitStatus());
}

void Pty::setSize(int lines, int cols)
{
  pty()->setWinSize(lines, cols);
}

void Pty::setXonXoff(bool on)
{
  pty()->setXonXoff(on);
}

void Pty::useUtf8(bool on)
{
  pty()->setUtf8Mode(on);
}

void Pty::setErase(char erase)
{
  struct termios tios;
  int fd = pty()->slaveFd();
  
  if(tcgetattr(fd, &tios))
  {
    qWarning("Uh oh.. can't get terminal attributes..");
    return;
  }
  tios.c_cc[VERASE] = erase;
  if(tcsetattr(fd, TCSANOW, &tios))
    qWarning("Uh oh.. can't set terminal attributes..");
}

/*!
    start the client program.
*/
int Pty::run(const char* _pgm, QStringList & _args, const char* _term, ulong winid, bool _addutmp,
               const char* _konsole_dbus_service, const char* _konsole_dbus_session)
{
  clearArguments();

  setBinaryExecutable(_pgm);

  QStringListIterator it( _args );
  while (it.hasNext())
    arguments.append( it.next().toUtf8() );

  if (_term && _term[0])
     setEnvironment("TERM",_term);
  if (_konsole_dbus_service && _konsole_dbus_service[0])
     setEnvironment("KONSOLE_DBUS_SERVICE",_konsole_dbus_service);
  if (_konsole_dbus_session && _konsole_dbus_session[0])
     setEnvironment("KONSOLE_DBUS_SESSION", _konsole_dbus_session);
  setEnvironment("WINDOWID", QString::number(winid));

  setUsePty(All, _addutmp);

  if ( start(NotifyOnExit, (Communication) (Stdin | Stdout)) == false )
     return -1;

  resume(); // Start...
  return 0;

}

void Pty::setWriteable(bool writeable)
{
  struct stat sbuf;
  stat(pty()->ttyName(), &sbuf);
  if (writeable)
    chmod(pty()->ttyName(), sbuf.st_mode | S_IWGRP);
  else
    chmod(pty()->ttyName(), sbuf.st_mode & ~(S_IWGRP|S_IWOTH));
}

/*!
    Create an instance.
*/
Pty::Pty()
{
  m_bufferFull = false;
  connect(this, SIGNAL(receivedStdout(K3Process *, char *, int )),
	  this, SLOT(dataReceived(K3Process *,char *, int)));
  connect(this, SIGNAL(processExited(K3Process *)),
          this, SLOT(donePty()));
  connect(this, SIGNAL(wroteStdin(K3Process *)),
          this, SLOT(writeReady()));

  setUsePty(All, false); // utmp will be overridden later
}

/*!
    Destructor.
*/
Pty::~Pty()
{
}

/*! sends a character through the line */
void Pty::send_byte(char c)
{
  send_bytes(&c,1);
}

/*! sends a 0 terminated string through the line */
void Pty::send_string(const char* s)
{
  send_bytes(s,strlen(s));
}

void Pty::writeReady()
{
  pendingSendJobs.erase(pendingSendJobs.begin());
  m_bufferFull = false;
  doSendJobs();
}

void Pty::doSendJobs() {
  if(pendingSendJobs.isEmpty())
  {
     emit buffer_empty();
     return;
  }
  
  SendJob& job = pendingSendJobs.first();

  
  if (!writeStdin( job.data(), job.length() ))
  {
    qWarning("Pty::doSendJobs - Could not send input data to terminal process.");
    return;
  }
  m_bufferFull = true;
}

void Pty::appendSendJob(const char* s, int len)
{
  pendingSendJobs.append(SendJob(s,len));
}

/*! sends len bytes through the line */
void Pty::send_bytes(const char* s, int len)
{
  appendSendJob(s,len);
  if (!m_bufferFull)
     doSendJobs();
}

/*! indicates that a block of data is received */
void Pty::dataReceived(K3Process *,char *buf, int len)
{
 // kDebug() << __FUNCTION__ << ": received " << len << " bytes - '" << buf << "'" << endl;

  emit block_in(buf,len);
}

void Pty::lockPty(bool lock)
{
  if (lock)
    suspend();
  else
    resume();
}

int Pty::foregroundProcessGroup() const
{
    int pid = tcgetpgrp(pty()->masterFd());

    if ( pid != -1 )
    {
        return pid;
    } 

    return 0;
}

#include "Pty.moc"

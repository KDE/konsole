/* -------------------------------------------------------------------------- */
/*                                                                            */
/* [TEPty.C]               Pseudo Terminal Device                             */
/*                                                                            */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>            */
/*                                                                            */
/* This file is part of Konsole - an X terminal for KDE                       */
/* -------------------------------------------------------------------------- */

/* If you're compiling konsole on non-Linux platforms and find
   problems that you can track down to this file, please have
   a look into ../README.ports, too.
*/

/*! \file
*/


/*! \class TEPty

    \brief Ptys provide a pseudo terminal connection to a program.

    Although closely related to pipes, these pseudo terminal connections have
    some ability, that makes it nessesary to uses them. Most importent, they
    know about changing screen sizes and UNIX job control.

    Within the terminal emulation framework, this class represents the
    host side of the terminal together with the connecting serial line.

    One can create many instances of this class within a program.
    As a side effect of using this class, a signal(2) handler is
    installed on SIGCHLD.

    \par FIXME

    [NOTE: much of the technical stuff below will be replaced by forkpty.]

    publish the SIGCHLD signal if not related to an instance.

    clearify TEPty::done vs. TEPty::~TEPty semantics.
    check if pty is restartable via run after done.

    \par Pseudo terminals

    Pseudo terminals are a unique feature of UNIX, and always come in form of
    pairs of devices (/dev/ptyXX and /dev/ttyXX), which are connected to each
    other by the operating system. One may think of them as two serial devices
    linked by a null-modem cable. Being based on devices the number of
    simultanous instances of this class is (globally) limited by the number of
    those device pairs, which is 256.

    Another technic are UNIX 98 PTY's. These are supported also, and prefered
    over the (obsolete) predecessor.

    There's a sinister ioctl(2), signal(2) and job control stuff
    nessesary to make everything work as it should.

    Much of the stuff can be simplified by using openpty from glibc2.
    Compatibility issues with obsolete installations and other unixes
    my prevent this.
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
                     
#include <kstandarddirs.h>
#include <klocale.h>
#include <kdebug.h>

#include <TEPty.h>


void TEPty::donePty()
{
  emit done();
}

void TEPty::setSize(int lines, int cols)
{
  setPtySize(lines, cols);
}

/*!
    start the client program.
*/
int TEPty::run(const char* _pgm, QStrList & _args, const char* _term, bool _addutmp,
               const char* _konsole_dcop, const char* _konsole_dcop_session)
{
  clearArguments();

  setBinaryExecutable(_pgm);

  QStrListIterator it( _args );
  for (; it.current(); ++it )
    arguments.append(it.current());

  if (_term && _term[0]) 
     setEnvironment("TERM",_term);
  if (_konsole_dcop && _konsole_dcop[0]) 
     setEnvironment("KONSOLE_DCOP",_konsole_dcop);
  if (_konsole_dcop_session && _konsole_dcop_session[0]) 
     setEnvironment("KONSOLE_DCOP_SESSION", _konsole_dcop_session);

  setUsePty(true, _addutmp);

  if (!start(NotifyOnExit, (Communication) (Stdin | Stdout)))
     return -1;

  resume(); // Start...
  return 0;

}

void TEPty::setWriteable(bool writeable)
{
  struct stat sbuf;
  stat(ptySlaveName(), &sbuf);
  if (writeable)
    chmod(ptySlaveName(), sbuf.st_mode | S_IWGRP);
  else
    chmod(ptySlaveName(), sbuf.st_mode & ~(S_IWGRP|S_IWOTH));
}

/*!
    Create an instance.
*/
TEPty::TEPty()
{
  m_bufferFull = false;
  connect(this, SIGNAL(receivedStdout(KProcess *, char *, int )),
	  this, SLOT(dataReceived(KProcess *,char *, int)));
  connect(this, SIGNAL(processExited(KProcess *)),
          this, SLOT(donePty()));
  connect(this, SIGNAL(wroteStdin(KProcess *)),
          this, SLOT(doSendJobs()));
}

/*!
    Destructor.
*/
TEPty::~TEPty()
{
}

/*! sends a character through the line */
void TEPty::send_byte(char c)
{
  send_bytes(&c,1);
}

/*! sends a 0 terminated string through the line */
void TEPty::send_string(const char* s)
{
  send_bytes(s,strlen(s));
}

void TEPty::doSendJobs() {
  if(pendingSendJobs.isEmpty())
  {
     if (m_bufferFull)
     {
        m_bufferFull = false;
        emit buffer_empty();
     }
     return;
  }
  
  SendJob& job = pendingSendJobs.first();
  if (!writeStdin(job.buffer.data(), job.length))
  {
    qWarning("Uh oh.. can't write data..");
    return;
  }
  pendingSendJobs.remove(pendingSendJobs.begin());
}

void TEPty::appendSendJob(const char* s, int len)
{
  pendingSendJobs.append(SendJob(s,len));
}

/*! sends len bytes through the line */
void TEPty::send_bytes(const char* s, int len)
{
  bool sent = writeStdin(s, len);
  if (!sent)
  {
    appendSendJob(s,len);
    m_bufferFull = true;
  }
}

/*! indicates that a block of data is received */
void TEPty::dataReceived(KProcess *,char *buf, int len)
{
  emit block_in(buf,len);
}

void TEPty::lockPty(bool lock)
{
  if (lock)
    suspend();
  else
    resume();
}

#include "TEPty.moc"

/* -------------------------------------------------------------------------- */
/*                                                                            */
/* [shell.cpp]                       Shell                                    */
/*                                                                            */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>            */
/*                                                                            */
/* This file is part of Konsole - an X terminal for KDE                       */
/* -------------------------------------------------------------------------- */

/*! /class Shell (FIXME: rename to TEpty or so)

    Shells provide a pseudo terminal connection to a program. Although it is
    closely related to a pipe, these pseudo terminal connections have some
    ability, that makes it nessesary to uses them. Most importent, they know
    about changing screen sizes (/sa setSize).

    Pseudo terminals are a unique feature of UNIX, and always come in form of
    pairs of devices (/dev/ptyXX and /dev/ttyXX), which are connected to each
    other by the operating system. One may think of them as two serial devices
    linked by a null-modem cable. Being based on devices the number of
    simultanous instances of this class is (globally) limited by the number of
    those device pairs, which is 256.

    The pty is for the Shell while the program gets the tty.

    There's a sinister ioctl(2), signal(2) and job control stuff
    nessesary to make everything work as it should. (/sa makeShell)
*/

#include <stdio.h>
#include <stdlib.h>

#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <grp.h>
#include "../../config.h"

#if defined (_HPUX_SOURCE)
#define _TERMIOS_INCLUDED
#include <bsdtty.h>
#endif

#ifdef HAVE_SYS_STROPTS_H
#include <sys/stropts.h>
#define _NEW_TTY_CTRL
#endif

#include <assert.h>
#include <time.h>
#include <signal.h>
#include <qintdict.h>
#include <sys/wait.h>

#include "TEShell.h"
#include "TEShell.moc"
#include <qstring.h>

#define HERE fprintf(stdout,"%s(%d): here\n",__FILE__,__LINE__)

FILE* syslog = NULL; //stdout;

/* -------------------------------------------------------------------------- */

void Shell::setSize(int lines, int columns)
// Tell the teletype handler what size the window is.
// Called after a window size change.
{ struct winsize wsize;
  if(fd < 0) return;
  wsize.ws_row = (unsigned short)lines;
  wsize.ws_col = (unsigned short)columns;
  ioctl(fd,TIOCSWINSZ,(char *)&wsize);
}

static char ptynam[] = "/dev/ptyxx";
static char ttynam[] = "/dev/ttyxx";

static QIntDict<Shell> shells;

static void catchChild(int)
// Catch a SIGCHLD signal and propagate that the child died.
{ int status;
  pid_t pid = wait(&status);
  Shell* sh = shells.find(pid);
  if (sh) { shells.remove(pid); sh->doneShell(status); }
}

void Shell::doneShell(int status)
{
  emit done(status);
}

int Shell::run(QStrList & args, const char* term)
{
  pid_t comm_pid = fork();
  if (comm_pid <  0) { fprintf(stderr,"Can't fork\n"); return -1; }
  if (comm_pid == 0) makeShell(ttynam,args,term);
  if (comm_pid >  0) shells.insert(comm_pid,this);
  return 0;
}

void Shell::makeShell(const char* dev, QStrList & args, 
	const char* term)
// only used internally. See `run' for interface
{ int sig; char* t;
  // open and set all standard files to master/slave tty
  int tt = open(dev, O_RDWR | O_EXCL);
  
  // Solaris apparently needs this 
#if defined(SVR4) || defined(__SVR4)
  ioctl(tt, I_PUSH, "ptem");
  ioctl(tt, I_PUSH, "ldterm");
#endif

  //reset signal handlers for child process
  for (sig = 1; sig < NSIG; sig++) signal(sig,SIG_DFL);
 
  // Don't know why, but his is vital for SIGHUP to find the child.
  // Could be, we get rid of the controling terminal by this.
  for (int i = 0; i < getdtablesize(); i++) if (i != tt && i != fd) close(i);

  dup2(tt,fileno(stdin));
  dup2(tt,fileno(stdout));
  dup2(tt,fileno(stderr));
  
  if (tt > 2) close(tt);

  // Setup job control

  // "There be dragons."
  //   (Ancient world map)
  
  if (setsid() < 0) perror("failed to set process group"); // (vital for bash)

#if defined(TIOCSCTTY)  
  ioctl(0, TIOCSCTTY, 0);
#endif  

  int pgrp = getpid();                 // This sequence is necessary for
  ioctl(0, TIOCSPGRP, (char *)&pgrp);  // event propagation. Omitting this
  setpgid(0,0);                        // is not noticeable with all
  close(open(dev, O_WRONLY, 0));       // clients (bash,vi). Because bash
  setpgid(0,0);                        // heals this, use '-e' to test it.

/*
#if defined(TIOCSPTLCK)
  int flag = 1;                        // Linux-only security solution:
  if (ioctl(fd,TIOCSPTLCK,&flag))      // prohibit opening tty from now on
#endif
    perror("Warning: The session is insecure.");
*/
  close(fd);
  
  // drop privileges
  setuid(getuid()); setgid(getgid());

  // propagate emulation
  if (term && term[0]) setenv("TERM",term,1);

  // convert QStrList into char*[]
  unsigned int i;
  char **argv = (char**)malloc(sizeof(char*)*(args.count()+1));
  for (i = 0; i<args.count(); i++) argv[i] = strdup(args.at(i));
  argv[i] = 0L;

  // setup for login shells
  char *f = argv[0];
  if ( login_shell )                   // see sh(1)
  {
    t = strrchr( argv[0], '/' );
    t = strdup(t);
    *t = '-';
    argv[0] = t;
  }

  // finally, pass to the new program
  execvp(f, argv);
  perror("exec failed");
  exit(1);                             // control should never come here.
}

int openShell()
{ int ptyfd; char *s3, *s4;
  static char ptyc3[] = "pqrstuvwxyzabcde";
  static char ptyc4[] = "0123456789abcdef";

  // Find a master pty that we can open ////////////////////////////////

  ptyfd = -1;
  for (s3 = ptyc3; *s3 != 0; s3++) 
  {
    for (s4 = ptyc4; *s4 != 0; s4++) 
    {
      ptynam[8] = ttynam[8] = *s3;
      ptynam[9] = ttynam[9] = *s4;
      if ((ptyfd = open(ptynam,O_RDWR)) >= 0) 
      {
        if (geteuid() == 0 || access(ttynam,R_OK|W_OK) == 0) break;
        close(ptyfd); ptyfd = -1;
      }
    }
    if (ptyfd >= 0) break;
  }
  if (ptyfd < 0) { fprintf(stderr,"Can't open a pseudo teletype\n"); exit(1); }
  fcntl(ptyfd,F_SETFL,O_NDELAY);

  return ptyfd;
}

Shell::Shell(int ls)
/* setup shell */
{
  login_shell=ls;

  fd = openShell();

  signal(SIGCHLD,catchChild);

  mn = new QSocketNotifier(fd, QSocketNotifier::Read);
  mw = new QSocketNotifier(fd, QSocketNotifier::Write);
  connect( mn, SIGNAL(activated(int)), this, SLOT(DataReceived(int)) );
  connect( mw, SIGNAL(activated(int)), this, SLOT(DataWritten(int)) );
}

Shell::~Shell()
{
  delete mn;
  delete mw;
  close(fd);
}

void Shell::send_byte(char c)
{ 
  write(fd,&c,1); mw->setEnabled(TRUE);
}

void Shell::send_string(const char* s)
{
  write(fd,s,strlen(s)); mw->setEnabled(TRUE);
}

void Shell::send_bytes(const char* s, int len)
{
  write(fd,s,len); mw->setEnabled(TRUE);
}

void Shell::DataReceived(int)
{ char buf[4096];
  int n = read(fd, buf, 4096);
  emit block_in(buf,n);
  if (syslog) // if (debugging) ...
  {
    int i;
    for (i = 0; i < n; i++) fputc(buf[i],syslog);
    fflush(syslog);
  }
}

void Shell::DataWritten(int)
{
  mw->setEnabled(FALSE); written();
}

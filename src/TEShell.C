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

/*! \file
*/

/*! \class Shell

    \brief Shells provide a pseudo terminal connection to a program.
    
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

    clearify Shell::done vs. Shell::~Shell semantics.

    remove the `login_shell' parameter from the Shell::Shell.
    Move parameters from Shell::run to Shell::Shell.

    \par Pseudo terminals

    Pseudo terminals are a unique feature of UNIX, and always come in form of
    pairs of devices (/dev/ptyXX and /dev/ttyXX), which are connected to each
    other by the operating system. One may think of them as two serial devices
    linked by a null-modem cable. Being based on devices the number of
    simultanous instances of this class is (globally) limited by the number of
    those device pairs, which is 256.

    The pty is for the Shell while the program gets the tty.

    Another technic are UNIX 98 PTY's. These are supported also, and prefered
    over the (obsolete) predecessor.

    There's a sinister ioctl(2), signal(2) and job control stuff
    nessesary to make everything work as it should.
*/

#include <stdlib.h>
#include <stdio.h>

#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#ifdef __FreeBSD__
#include <sys/time.h>
#endif
#include <sys/resource.h>
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
#include <sys/stat.h>

#include "TEShell.h"
#include "TEShell.moc"
#include <qstring.h>

#include <kapp.h>
#include <kmsgbox.h>

#define HERE fprintf(stdout,"%s(%d): here\n",__FILE__,__LINE__)

FILE* syslog_file = NULL; //stdout;

static QIntDict<Shell> shells;

/* -------------------------------------------------------------------------- */

//#include <unistd.h>
//#include <stdlib.h>
//#include <sys/types.h>
//#include <sys/wait.h>

#define PTY_FILENO 3
#define BASE_CHOWN "konsole_grantpty"

static int chownpty(int fd, int grant)
// param fd: the fd of a master pty.
// param grant: 1 to grant, 0 to revoke
// returns 1 on success 0 on fail
{
  void(*tmp)(int) = signal(SIGCHLD,SIG_DFL);
  pid_t pid = fork();
  if (pid < 0)
  {
    signal(SIGCHLD,tmp);
    return 0;
  }
  if (pid == 0)
  {
    /* We pass the master pseudo terminal as file descriptor PTY_FILENO. */
    if (fd != PTY_FILENO && dup2(fd, PTY_FILENO) < 0) exit(1);
    QString path = KApplication::kde_bindir() + "/" + BASE_CHOWN;
    execle(path.data(), BASE_CHOWN, grant?"--grant":"--revoke", NULL, NULL);
    exit(1); // should not be reached
  }
  if (pid > 0)
  { int w;
  retry:
    int rc = waitpid (pid, &w, 0);
    if (rc != pid)
    { // signal from other child, behave like catchChild.
      // guess this gives quite some control chaos...
      Shell* sh = shells.find(rc);
      if (sh) { shells.remove(rc); sh->doneShell(w); }
      goto retry;
    }
    signal(SIGCHLD,tmp);
    return (rc != -1 && WIFEXITED(w) && WEXITSTATUS(w) == 0);
  }
  signal(SIGCHLD,tmp);
  return 0; //dummy.
}

/* -------------------------------------------------------------------------- */

/*!
   Informs the client program about the
   actual size of the window.
*/

void Shell::setSize(int lines, int columns)
{ struct winsize wsize; //FIXME: put into Shell.
  wsize.ws_row = (unsigned short)lines;
  wsize.ws_col = (unsigned short)columns;
  if(fd < 0) return;
  ioctl(fd,TIOCSWINSZ,(char *)&wsize);
}

//! Catch a SIGCHLD signal and propagate that the child died.
static void catchChild(int)
{ int status;
  pid_t pid = waitpid(-1,&status,WNOHANG);
  Shell* sh = shells.find(pid);
  if (sh) { shells.remove(pid); sh->doneShell(status); }
}

void Shell::doneShell(int status)
{
  if (needGrantPty) chownpty(fd,FALSE);
  emit done(status);
}

/*!
    start the client program.
*/
int Shell::run(QStrList & args, const char* term)
{
  comm_pid = fork();
  if (comm_pid <  0) { fprintf(stderr,"Can't fork\n"); return -1; }
  if (comm_pid == 0) makeShell(ttynam,args,term);
  if (comm_pid >  0) shells.insert(comm_pid,this);
  return 0;
}

int Shell::openShell()
{ int ptyfd = -1;
  needGrantPty = TRUE;

  // Find a master pty that we can open ////////////////////////////////

  // first we try UNIX PTY's

#ifdef TIOCGPTN
  strcpy(ptynam,"/dev/ptmx");
  strcpy(ttynam,"/dev/pts/");
  ptyfd = open(ptynam,O_RDWR);
  if (ptyfd >= 0) // got the master pty
  { int ptyno;
    if (ioctl(ptyfd, TIOCGPTN, &ptyno) == 0)
    { struct stat sbuf;
      sprintf(ttynam,"/dev/pts/%d",ptyno);
      if (stat(ttynam,&sbuf) == 0 && S_ISCHR(sbuf.st_mode))
        needGrantPty = FALSE;
      else
      {
        close(ptyfd);
        ptyfd = -1;
      }
    }
    else
    {
      close(ptyfd);
      ptyfd = -1;
    }
  }
#endif

#if defined(_SCO_DS) || defined(__USLC__) /* SCO OSr5 and UnixWare */
  if (ptyfd < 0)
  { for (int idx = 0; idx < 256; idx++)
    { sprintf(ptynam, "/dev/ptyp%d", idx);
      sprintf(ttynam, "/dev/ttyp%d", idx);
      if (access(ttynam, F_OK) < 0) { idx = 256; break; }
      if ((ptyfd = open (ptynam, O_RDWR)) >= 0)
      { if (access (ttynam, R_OK|W_OK) == 0) break;
        close(ptyfd); ptyfd = -1;
      }
    }
  }
#endif

  if (ptyfd < 0) // Linux, FIXME: Trouble on other systems?
  { for (char* s3 = "pqrstuvwxyzabcde"; *s3 != 0; s3++) 
    { for (char* s4 = "0123456789abcdef"; *s4 != 0; s4++) 
      { sprintf(ptynam,"/dev/pty%c%c",*s3,*s4);
        sprintf(ttynam,"/dev/tty%c%c",*s3,*s4);
        if ((ptyfd = open(ptynam,O_RDWR)) >= 0) 
        { if (geteuid() == 0 || access(ttynam,R_OK|W_OK) == 0) break;
          close(ptyfd); ptyfd = -1;
        }
      }
      if (ptyfd >= 0) break;
    }
  }

  if (ptyfd < 0)
  {
    //FIXME: handle more gracefully.
    fprintf(stderr,"Can't open a pseudo teletype\n"); exit(1);
  }
  if (needGrantPty && !chownpty(ptyfd,TRUE))
  {
    fprintf(stderr,"konsole: chownpty failed for device %s::%s.\n",ptynam,ttynam);
    fprintf(stderr,"       : This means the session can be eavesdroped.\n");
    fprintf(stderr,"       : Make sure konsole_grantpty is installed in\n");
    fprintf(stderr,"       : %s and setuid root.\n",KApplication::kde_bindir().data());
  }
  fcntl(ptyfd,F_SETFL,O_NDELAY);

  return ptyfd;
}

//! only used internally. See `run' for interface
void Shell::makeShell(const char* dev, QStrList & args, 
	const char* term)
{ int sig; char* t;

  if (fd < 0) // no master pty could be opened
  {
  //FIXME:
  //fprintf(stderr,"opening master pty failed.\n");
  //exit(1);
  }

#ifdef TIOCSPTLCK
  int flag = 0; ioctl(fd,TIOCSPTLCK,&flag); // unlock pty
#endif
  // open and set all standard files to slave tty
  int tt = open(dev, O_RDWR);

  if (tt < 0) // the slave pty could be opened
  {
  //FIXME:
  //fprintf(stderr,"opening slave pty (%s) failed.\n",dev);
  //exit(1);
  }
  
#if (defined(SVR4) || defined(__SVR4)) && (defined(i386) || defined(__i386__))
  // Solaris x86
  ioctl(tt, I_PUSH, "ptem");
  ioctl(tt, I_PUSH, "ldterm");
#endif

  //reset signal handlers for child process
  for (sig = 1; sig < NSIG; sig++) signal(sig,SIG_DFL);
 
  // Don't know why, but his is vital for SIGHUP to find the child.
  // Could be, we get rid of the controling terminal by this.
  // getrlimit is a getdtablesize() equivalent, more portable (David Faure)
  struct rlimit rlp;
  getrlimit(RLIMIT_NOFILE, &rlp);
  for (int i = 0; i < rlp.rlim_cur; i++) if (i != tt) close(i);

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

/*! 
    Create a shell.
    \param _login_shell is a hack. FIXME: remove.
*/
Shell::Shell(int _login_shell)
{
  login_shell=_login_shell;

  fd = openShell();

  signal(SIGCHLD,catchChild);

  mn = new QSocketNotifier(fd, QSocketNotifier::Read);
  connect( mn, SIGNAL(activated(int)), this, SLOT(DataReceived(int)) );
}

/*!
    Destructor.
    Note that the related client program is not killed
    (yet) when a shell is deleted.
*/
Shell::~Shell()
{
  delete mn;
  close(fd);
}

/*! send signal to child */
void Shell::kill(int signal)
{
  ::kill(comm_pid,signal);
}

/*! sends a character through the line */
void Shell::send_byte(char c)
{ 
  write(fd,&c,1);
}

/*! sends a 0 terminated string through the line */
void Shell::send_string(const char* s)
{
  write(fd,s,strlen(s));
}

/*! sends len bytes through the line */
void Shell::send_bytes(const char* s, int len)
{
  write(fd,s,len);
}

/*! indicates that a block of data is received */
void Shell::DataReceived(int)
{ char buf[4096];
  int n = read(fd, buf, 4096);
  emit block_in(buf,n);
  if (syslog_file) // if (debugging) ...
  {
    int i;
    for (i = 0; i < n; i++) fputc(buf[i],syslog_file);
    fflush(syslog_file);
  }
}

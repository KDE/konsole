/* -------------------------------------------------------------------------- */
/*                                                                            */
/* [TEPty.h]                 Pseudo Terminal Device                           */
/*                                                                            */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>            */
/*                                                                            */
/* This file is part of Konsole - an X terminal for KDE                       */
/*                                                                            */
/* -------------------------------------------------------------------------- */

/*! \file
*/

#ifndef TE_PTY_H
#define TE_PTY_H

#include <config.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <kprocess.h>
#include <termios.h>
#include <qsocketnotifier.h>
#include <qtimer.h>
#include <qstrlist.h>
#include <qvaluelist.h>
#include <qarray.h>

class TEPty: public KProcess
{
Q_OBJECT

  public:

    TEPty();
    ~TEPty();

  public:

    /*!
        having a `run' separate from the constructor allows to make
        the necessary connections to the signals and slots of the
        instance before starting the execution of the client.
    */
    int run(const char* pgm, QStrList & args, const char* term, int addutmp);

  public slots:

    void send_bytes(const char* s, int len);
    void setSize(int lines, int columns);

  signals:

    /*!
        emitted when the client program terminates.
        \param status the wait(2) status code of the terminated client program.
    */
    void done(int status);

    /*!
        emitted when a new block of data comes in.
        \param s - the data
        \param len - the length of the block
    */
    void block_in(const char* s, int len);

  public:

    void send_byte(char s);
    void send_string(const char* s);

    const char* deviceName();
    
  protected:
      virtual int commSetupDoneC();
      virtual int setupCommunication(Communication comm);
  
  protected slots:
      void DataReceived(int, int& len);
  public slots:
      void donePty();
      
  private:
    void makePty(const char* dev, const char* pgm, QStrList & args, const char* term, int addutmp);
    int  openPty();
    void appendSendJob(const char* s, int len);

  private slots:
    void doSendJobs();

  private:

    struct winsize wsize;
    int fd;
    bool             needGrantPty;
    char ptynam[50]; // "/dev/ptyxx" | "/dev/ptmx"
    char ttynam[50]; // "/dev/ttyxx" | "/dev/pts/########..."
    const char *pgm;
    const char *term;
    int addutmp;

    struct SendJob {
      SendJob() {}
      SendJob(const char* b, int len) {
        buffer.duplicate(b,len);
        start = 0;
        length = len;
      }
      QArray<char> buffer;
      int start;
      int length;
    };
    QValueList<SendJob> pendingSendJobs;
    QTimer* pSendJobTimer;

friend int chownpty(int,int);
};

#endif

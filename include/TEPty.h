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

#include <qobject.h>
#include <termios.h>
#include <qsocketnotifier.h>
#include <qtimer.h>
#include <qstrlist.h>

class TEPty: public QObject
{
Q_OBJECT

  public:

    TEPty();
    ~TEPty();

  public:

    int run(QStrList & args, const char* term, int login_shell, int addutmp);
    void kill(int signal);

  public:

    void send_byte(char s);
    void send_string(const char* s);

  public slots:

    void send_bytes(const char* s, int len);

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

  private slots:

    void DataReceived(int);

  public slots:

    void setSize(int lines, int columns);

  public:

    const char* deviceName();

  private:

    void makePty(const char* dev, QStrList & args, const char* term, int login_shell, int addutmp);
    int  openPty();
    void donePty(int status);

  private:

    struct winsize wsize;

    int              fd;
    pid_t            comm_pid;
    QSocketNotifier* mn;

    bool             needGrantPty;
    char ptynam[50]; // "/dev/ptyxx" | "/dev/ptmx"
    char ttynam[50]; // "/dev/ttyxx" | "/dev/pts/########..."

friend int chownpty(int,int);
friend void catchChild(int);
};

#endif

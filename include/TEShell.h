/* -------------------------------------------------------------------------- */
/*                                                                            */
/* [Shell.h]                        Shell                                     */
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

#ifndef SHELL_H
#define SHELL_H

#include <qobject.h>
#include <termios.h>
#include <qsocknot.h>
#include <qtimer.h>
#include <qstrlist.h>

/* ---| Shell Parameters |--------------------------------------------------- */

//FIXME: clean this up!

class Shell: public QObject
{
Q_OBJECT

  public:
    Shell(int login_shell);
    ~Shell();

  public:
    int run(QStrList & args, const char* term);

  public:
    /*! this is of internal use only. FIXME: make private */
    void doneShell(int status);
    void kill(int signal);

  public:
    void send_byte(char s);
    void send_string(const char* s);

  public slots:
    void send_bytes(const char* s, int len);

  signals:
    /*!
        emitted when the shell instance terminates.
        \param status the wait(2) status code of the terminated child program.
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

  private:

    void makeShell(const char* dev, QStrList & args, const char* term);

  private:

    int              fd;
    pid_t            comm_pid;
    int		           login_shell;
    QSocketNotifier* mn;
};

#endif

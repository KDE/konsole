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

  signals:
    void done(int status);
  public:
    void doneShell(int status);

  public:
    void send_byte(char s);
    void send_string(const char* s);

  public slots:
    void send_bytes(const char* s, int len);

  signals:

    void block_in(const char* s, int len);
    void written();         // Ready to write

  private slots:

    void DataReceived(int); //
    void DataWritten(int);  //

  public slots:

    void setSize(int lines, int columns); // 

  private:

    void makeShell(const char* dev, QStrList & args, const char* term);

  private:

    int              fd;
    struct termios   tp;
    int		     login_shell;
    QSocketNotifier* mn;
    QSocketNotifier* mw;
};

#endif

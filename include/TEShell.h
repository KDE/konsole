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

/* ---| Shell Parameters |--------------------------------------------------- */

//FIXME: clean this up!

class Shell: public QObject
{
Q_OBJECT

  public:
    Shell();
    ~Shell();

  public:
    int run(char* argv[], const char* term);

  signals:
    void done();
  public:
    void doneShell();

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

    void makeShell(const char* dev, char* argv[], const char* term);

  private:

    int              fd;
    struct termios   tp;

    QSocketNotifier* mn;
    QSocketNotifier* mw;
};

#endif

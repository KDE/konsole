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

#include <kprocess.h>
#include <qsocketnotifier.h>
#include <qstrlist.h>
#include <qvaluelist.h>
#include <qmemarray.h>

class TEPty: public KProcess
{
Q_OBJECT

  public:

    TEPty();
    ~TEPty();

  public:

    /*!
     * having a `run' separate from the constructor allows to make
     * the necessary connections to the signals and slots of the
     * instance before starting the execution of the client.
     */
    int run( const char* pgm, QStrList & args, const char* term, bool _addutmp,
             const char* konsole_dcop = "", const char* konsole_dcop_session = "" );
    void setWriteable(bool writeable);
    QString error() { return m_strError; }

    // Make public
    void commClose() { KProcess::commClose(); }
    void openMasterPty() { KProcess::openMasterPty(); }

  public slots:
    void lockPty(bool lock);
    void send_bytes(const char* s, int len);
    void setSize(int lines, int cols);

  signals:

    /*!
        emitted when the client program terminates.
    */
    void done();

    /*!
        emitted when a new block of data comes in.
        \param s - the data
        \param len - the length of the block
    */
    void block_in(const char* s, int len);

  public:

    void send_byte(char s);
    void send_string(const char* s);

  protected slots:
      void DataReceived(int, int& len);
  public slots:
      void donePty();
      
  private:
    void appendSendJob(const char* s, int len);

  private slots:
    void doSendJobs();

  private:

    QString m_strError;

    struct SendJob {
      SendJob() {}
      SendJob(const char* b, int len) {
        buffer.duplicate(b,len);
        length = len;
      }
      QMemArray<char> buffer;
      int length;
    };
    QValueList<SendJob> pendingSendJobs;

};

#endif

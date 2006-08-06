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
    int run( const char* pgm, QStrList & args, const char* term, ulong winid, bool _addutmp,
             const char* konsole_dcop = "", const char* konsole_dcop_session = "" );
    void setWriteable(bool writeable);
    QString error() { return m_strError; }
    void setXonXoff(bool on);
    void setSize(int lines, int cols);
    void setErase(char erase);

  public slots:
    void useUtf8(bool on);
    void lockPty(bool lock);
    void send_bytes(const char* s, int len);

  signals:

    /*!
        emitted when the client program terminates.
    */
    void done(int returnCode);

    /*!
        emitted when a new block of data comes in.
        \param s - the data
        \param len - the length of the block
    */
    void block_in(const char* s, int len);
    
    /*!
        emitted when buffer_full becomes false
    */
    void buffer_empty();

    // this will be emitted in the child process
    // after forking (and, obviously, before exec())
    void forkedChild();

  public:

    void send_byte(char s);
    void send_string(const char* s);
    bool buffer_full() { return m_bufferFull; }

    // override from KProcess to allow the client of konsolePart
    // to set up things after fork() but before exec()
    virtual int commSetupDoneC();

  protected slots:
      void dataReceived(KProcess *, char *buf, int len);
  public slots:
      void donePty();
      
  private:
    void appendSendJob(const char* s, int len);

  private slots:
    void doSendJobs();
    void writeReady();

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
    bool m_bufferFull;
};

#endif

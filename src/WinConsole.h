/*
    This file is part of Konsole, KDE's terminal emulator.

    Copyright 2013 by Patrick Spendrin <ps_ml@gmx.de>

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

#ifndef WINCONSOLE_H
#define WINCONSOLE_H

// Qt
#include <QtCore/QProcess>
#include <QtCore/QSize>

// KcwSH
#include <kcwsh/terminal.h>

// Konsole
#include "konsole_export.h"

class QStringList;

namespace Konsole
{

/**
 * The Pty class is used to start the terminal process,
 * send data to it, receive data from it and manipulate
 * various properties of the pseudo-teletype interface
 * used to communicate with the process.
 *
 * To use this class, construct an instance and connect
 * to the sendData slot and receivedData signal to
 * send data to or receive data from the process.
 *
 * To start the terminal process, call the start() method
 * with the program name and appropriate arguments.
 */
class KONSOLEPRIVATE_EXPORT WinConsole : public QProcess, public KcwSH::Terminal
{
    Q_OBJECT

public:
    /**
     * Constructs a new Pty.
     *
     * Connect to the sendData() slot and receivedData() signal to prepare
     * for sending and receiving data from the terminal process.
     *
     * To start the terminal process, call the run() method with the
     * name of the program to start and appropriate arguments.
     */
    explicit WinConsole(QObject* parent = 0);

    /**
     * @return the pid of the command running as the terminal
     */
    int pid() const;

    /**
     * Returns the process id of the teletype's current foreground
     * process.  This is the process which is currently reading
     * input sent to the terminal via. sendData()
     *
     * If there is a problem reading the foreground process group,
     * 0 will be returned.
     */
    int foregroundProcessGroup() const;

    /**
     * Close the underlying terminal
     */
    void closePty();

    /**
     * Sets the size of the window (in columns and lines of characters)
     * used by this teletype.
     */
    void setWindowSize(int columns, int lines);

    /** Returns the size of the window used by this teletype.  See setWindowSize() */
    QSize windowSize() const;

    /**
     * Reimplemented
     */
    QProcess::ProcessState state() const;

    /**
     * Sets the initial working directory.
     */
    void setInitialWorkingDirectory(const QString& dir);

    /**
     * Starts the terminal process.
     *
     * Returns 0 if the process was started successfully or non-zero
     * otherwise.
     *
     * @param program Path to the program to start
     * @param arguments Arguments to pass to the program being started
     * @param environment A list of key=value pairs which will be added
     * to the environment for the new process.  At the very least this
     * should include an assignment for the TERM environment variable.
     */
    int start(const QString& program,
              const QStringList& arguments,
              const QStringList& environment);

    /** 
     * Control whether the pty device is writeable by group members.
     * This doesn't do anything on windows
     */
    void setWriteable(bool writeable);

    /**
     * reimplementation from QProcess
     * @param msecs the number of milliseconds to wait before returning with
     * false as return value
     */
    bool waitForFinished(int msecs);

    void setFlowControlEnabled(bool e);
    bool flowControlEnabled() const;

    /**
     * following functions are reimplemented for signal forwarding from
     * KcwSH::Terminal
     */
    void sizeChanged();
    void bufferChanged();
    void cursorPositionChanged();
    void hasScrolled();

    void titleChanged();

    void hasQuit();

signals:
    void cursorChanged(int x, int y);
    void termTitleChanged(int i, QString title);
    void outputChanged();
    void scrollHappened(int x, int y);

private:
    // FIXME:Patrick no friend declaration would be better
    friend class WinConEmulation;
};
}
#endif /* WINCONSOLE_H */

/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PTY_H
#define PTY_H

// Qt
#include <QSize>
#include <QtContainerFwd>

// KDE
#include <KPtyProcess>

// Konsole
#include "konsoleprivate_export.h"

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
class KONSOLEPRIVATE_EXPORT Pty : public KPtyProcess
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
    explicit Pty(QObject *parent = nullptr);

    /**
     * Construct a process using an open pty master.
     * See KPtyProcess::KPtyProcess()
     */
    explicit Pty(int ptyMasterFd, QObject *parent = nullptr);

    ~Pty() override;

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
    int start(const QString &program, const QStringList &arguments, const QStringList &environment);

    /** Control whether the pty device is writeable by group members. */
    void setWriteable(bool writeable);

    /**
     * Enables or disables Xon/Xoff flow control.  The flow control setting
     * may be changed later by a terminal application, so flowControlEnabled()
     * may not equal the value of @p on in the previous call to setFlowControlEnabled()
     */
    void setFlowControlEnabled(bool on);

    /** Queries the terminal state and returns true if Xon/Xoff flow control is enabled. */
    bool flowControlEnabled() const;

    /**
     * Sets the size of the window (in columns and lines of characters,
     * and width and height in pixels) used by this teletype.
     */
    void setWindowSize(int columns, int lines, int width, int height);

    /** Returns the size of the window used by this teletype in characters.  See setWindowSize() */
    QSize windowSize() const;

    /** Returns the size of the window used by this teletype in pixels.  See setWindowSize() */
    QSize pixelSize() const;

    /**
     * Sets the special character for erasing previous not-yet-erased character.
     * See termios(3) for detailed description.
     */
    void setEraseChar(char eraseChar);

    /** */
    char eraseChar() const;

    /**
     * Sets the initial working directory.
     */
    void setInitialWorkingDirectory(const QString &dir);

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
     * Close the underlying pty master/slave pair.
     */
    void closePty();

public Q_SLOTS:
    /**
     * Put the pty into UTF-8 mode on systems which support it.
     */
    void setUtf8Mode(bool on);

    /**
     * Sends data to the process currently controlling the
     * teletype ( whose id is returned by foregroundProcessGroup() )
     *
     * @param data the data to send.
     */
    void sendData(const QByteArray &data);

Q_SIGNALS:
    /**
     * Emitted when a new block of data is received from
     * the teletype.
     *
     * @param buffer Pointer to the data received.
     * @param length Length of @p buffer
     */
    void receivedData(const char *buffer, int length);

protected:
    // TODO: remove this; the method is removed from QProcess in Qt6
    // instead use setChildProcessModifier() in the constructor
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void setupChildProcess() override;
#endif

private Q_SLOTS:
    // called when data is received from the terminal process
    void dataReceived();

private:
    void init();

    // takes a list of key=value pairs and adds them
    // to the environment for the process
    void addEnvironmentVariables(const QStringList &environment);

    int _windowColumns;
    int _windowLines;
    int _windowWidth;
    int _windowHeight;
    char _eraseChar;
    bool _xonXoff;
    bool _utf8;
};
}

#endif // PTY_H

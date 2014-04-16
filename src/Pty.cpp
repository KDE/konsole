/*
    This file is part of Konsole, an X terminal.
    Copyright 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

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

// Own
#include "Pty.h"

// System
#include <termios.h>
#include <signal.h>

// Qt
#include <QtCore/QStringList>

// KDE
#include <KDebug>
#include <KPtyDevice>
#include <kde_file.h>

using Konsole::Pty;

Pty::Pty(int masterFd, QObject* aParent)
    : KPtyProcess(masterFd, aParent)
{
    init();
}

Pty::Pty(QObject* aParent)
    : KPtyProcess(aParent)
{
    init();
}

void Pty::init()
{
    _windowColumns = 0;
    _windowLines   = 0;
    _eraseChar     = 0;
    _xonXoff       = true;
    _utf8          = true;

    setEraseChar(_eraseChar);
    setFlowControlEnabled(_xonXoff);
    setUtf8Mode(_utf8);

    setWindowSize(_windowColumns, _windowLines);

    setUseUtmp(true);
    setPtyChannels(KPtyProcess::AllChannels);

    connect(pty(), SIGNAL(readyRead()) , this , SLOT(dataReceived()));
}

Pty::~Pty()
{
}

void Pty::sendData(const char* data, int length)
{
    if (length == 0)
        return;

    if (!pty()->write(data, length)) {
        kWarning() << "Could not send input data to terminal process.";
        return;
    }
}

void Pty::dataReceived()
{
    QByteArray data = pty()->readAll();
    emit receivedData(data.constData(), data.count());
}

void Pty::setWindowSize(int columns, int lines)
{
    _windowColumns = columns;
    _windowLines = lines;

    if (pty()->masterFd() >= 0)
        pty()->setWinSize(lines, columns);
}

QSize Pty::windowSize() const
{
    return QSize(_windowColumns, _windowLines);
}

void Pty::setFlowControlEnabled(bool enable)
{
    _xonXoff = enable;

    if (pty()->masterFd() >= 0) {
        struct ::termios ttmode;
        pty()->tcGetAttr(&ttmode);
        if (enable)
            ttmode.c_iflag |= (IXOFF | IXON);
        else
            ttmode.c_iflag &= ~(IXOFF | IXON);

        if (!pty()->tcSetAttr(&ttmode))
            kWarning() << "Unable to set terminal attributes.";
    }
}

bool Pty::flowControlEnabled() const
{
    if (pty()->masterFd() >= 0) {
        struct ::termios ttmode;
        pty()->tcGetAttr(&ttmode);
        return ttmode.c_iflag & IXOFF &&
               ttmode.c_iflag & IXON;
    } else {
        kWarning() << "Unable to get flow control status, terminal not connected.";
        return _xonXoff;
    }
}

void Pty::setUtf8Mode(bool enable)
{
#if defined(IUTF8) // XXX not a reasonable place to check it.
    _utf8 = enable;

    if (pty()->masterFd() >= 0) {
        struct ::termios ttmode;
        pty()->tcGetAttr(&ttmode);
        if (enable)
            ttmode.c_iflag |= IUTF8;
        else
            ttmode.c_iflag &= ~IUTF8;

        if (!pty()->tcSetAttr(&ttmode))
            kWarning() << "Unable to set terminal attributes.";
    }
#else
    Q_UNUSED(enable);
#endif
}

void Pty::setEraseChar(char eChar)
{
    _eraseChar = eChar;

    if (pty()->masterFd() >= 0) {
        struct ::termios ttmode;
        pty()->tcGetAttr(&ttmode);
        ttmode.c_cc[VERASE] = eChar;

        if (!pty()->tcSetAttr(&ttmode))
            kWarning() << "Unable to set terminal attributes.";
    }
}

char Pty::eraseChar() const
{
    if (pty()->masterFd() >= 0) {
        struct ::termios ttyAttributes;
        pty()->tcGetAttr(&ttyAttributes);
        return ttyAttributes.c_cc[VERASE];
    } else {
        kWarning() << "Unable to get erase char attribute, terminal not connected.";
        return _eraseChar;
    }
}

void Pty::setInitialWorkingDirectory(const QString& dir)
{
    QString pwd = dir;

    // remove trailing slash in the path when appropriate
    // example: /usr/share/icons/ ==> /usr/share/icons
    if (pwd.length() > 1 && pwd.endsWith(QLatin1Char('/'))) {
        pwd.chop(1);
    }

    setWorkingDirectory(pwd);

    // setting PWD to "." will cause problem for bash & zsh
    if (pwd != ".")
        setEnv("PWD", pwd);
}

void Pty::addEnvironmentVariables(const QStringList& environmentVariables)
{
    bool isTermEnvAdded = false;

    foreach(const QString& pair, environmentVariables) {
        // split on the first '=' character
        const int separator = pair.indexOf('=');

        if (separator >= 0) {
            QString variable = pair.left(separator);
            QString value = pair.mid(separator + 1);

            setEnv(variable, value);

            if (variable == "TERM") {
                isTermEnvAdded = true;
            }
        }
    }

    // extra safeguard to make sure $TERM is always set
    if (!isTermEnvAdded) {
        setEnv("TERM", "xterm");
    }
}

int Pty::start(const QString& programName,
               const QStringList& programArguments,
               const QStringList& environmentList)
{
    clearProgram();

    // For historical reasons, the first argument in programArguments is the
    // name of the program to execute, so create a list consisting of all
    // but the first argument to pass to setProgram()
    Q_ASSERT(programArguments.count() >= 1);
    setProgram(programName, programArguments.mid(1));

    addEnvironmentVariables(environmentList);

    // unless the LANGUAGE environment variable has been set explicitly
    // set it to a null string
    // this fixes the problem where KCatalog sets the LANGUAGE environment
    // variable during the application's startup to something which
    // differs from LANG,LC_* etc. and causes programs run from
    // the terminal to display messages in the wrong language
    //
    // this can happen if LANG contains a language which KDE
    // does not have a translation for
    //
    // BR:149300
    setEnv("LANGUAGE", QString(), false /* do not overwrite existing value if any */);

    KProcess::start();

    if (waitForStarted()) {
        return 0;
    } else {
        return -1;
    }
}

void Pty::setWriteable(bool writeable)
{
    KDE_struct_stat sbuf;
    if (KDE::stat(pty()->ttyName(), &sbuf) == 0) {
        if (writeable) {
            if (KDE::chmod(pty()->ttyName(), sbuf.st_mode | S_IWGRP) < 0) {
                kWarning() << "Could not set writeable on "<<pty()->ttyName();
            }
        } else {
            if (KDE::chmod(pty()->ttyName(), sbuf.st_mode & ~(S_IWGRP | S_IWOTH)) < 0) {
                kWarning() << "Could not unset writeable on "<<pty()->ttyName();
            }
        }
    }
}

void Pty::closePty()
{
    pty()->close();
}

int Pty::foregroundProcessGroup() const
{
    int foregroundPid = tcgetpgrp(pty()->masterFd());

    if (foregroundPid != -1) {
        return foregroundPid;
    }

    return 0;
}

void Pty::setupChildProcess()
{
    KPtyProcess::setupChildProcess();

    // reset all signal handlers
    // this ensures that terminal applications respond to
    // signals generated via key sequences such as Ctrl+C
    // (which sends SIGINT)
    struct sigaction action;
    sigemptyset(&action.sa_mask);
    action.sa_handler = SIG_DFL;
    action.sa_flags = 0;
    for (int signal = 1; signal < NSIG; signal++)
        sigaction(signal, &action, 0);
}

#include "Pty.moc"

/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "Pty.h"

#include "konsoledebug.h"

// System
#include <csignal>
#if !defined(Q_OS_WIN)
#include <sys/ioctl.h> //ioctl() and TIOCSWINSZ
#endif
#include <termios.h>

// Qt
#include <QStringList>
#include <qplatformdefs.h>

// KDE
#include <KPtyDevice>

using Konsole::Pty;

Pty::Pty(int masterFd, QObject *aParent)
    : KPtyProcess(masterFd, aParent)
{
    init();
}

Pty::Pty(QObject *aParent)
    : KPtyProcess(aParent)
{
    init();
}

void Pty::init()
{
    _windowColumns = 0;
    _windowLines = 0;
    _eraseChar = 0;
    _xonXoff = true;
    _utf8 = true;

    setEraseChar(_eraseChar);
    setFlowControlEnabled(_xonXoff);
    setUtf8Mode(_utf8);

    setWindowSize(_windowColumns, _windowLines);

    setUseUtmp(true);
    setPtyChannels(KPtyProcess::AllChannels);

    connect(pty(), &KPtyDevice::readyRead, this, &Konsole::Pty::dataReceived);
}

Pty::~Pty() = default;

void Pty::sendData(const QByteArray &data)
{
    if (data.isEmpty()) {
        return;
    }

    if (pty()->write(data) == -1) {
        qCDebug(KonsoleDebug) << "Could not send input data to terminal process.";
        return;
    }
}

void Pty::dataReceived()
{
    QByteArray data = pty()->readAll();
    if (data.isEmpty()) {
        return;
    }

    Q_EMIT receivedData(data.constData(), data.count());
}

void Pty::setWindowSize(int columns, int lines)
{
    _windowColumns = columns;
    _windowLines = lines;

    if (pty()->masterFd() >= 0) {
        pty()->setWinSize(lines, columns);
#if !defined(Q_OS_WIN)
        struct winsize w;
        w.ws_xpixel = _windowWidth;
        w.ws_ypixel = _windowHeight;
        w.ws_col = _windowColumns;
        w.ws_row = _windowLines;
        ioctl(pty()->masterFd(), TIOCSWINSZ, &w);
#endif
    }
}

void Pty::setPixelSize(int width, int height)
{
    _windowWidth = width;
    _windowHeight = height;
}

QSize Pty::windowSize() const
{
    return {_windowColumns, _windowLines};
}

void Pty::setFlowControlEnabled(bool enable)
{
    _xonXoff = enable;

    if (pty()->masterFd() >= 0) {
        struct ::termios ttmode;
        pty()->tcGetAttr(&ttmode);
        if (enable) {
            ttmode.c_iflag |= (IXOFF | IXON);
        } else {
            ttmode.c_iflag &= ~(IXOFF | IXON);
        }

        if (!pty()->tcSetAttr(&ttmode)) {
            qCDebug(KonsoleDebug) << "Unable to set terminal attributes.";
        }
    }
}

bool Pty::flowControlEnabled() const
{
    if (pty()->masterFd() >= 0) {
        struct ::termios ttmode;
        pty()->tcGetAttr(&ttmode);
        return ((ttmode.c_iflag & IXOFF) != 0U) && ((ttmode.c_iflag & IXON) != 0U);
    } else {
        qCDebug(KonsoleDebug) << "Unable to get flow control status, terminal not connected.";
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
        if (enable) {
            ttmode.c_iflag |= IUTF8;
        } else {
            ttmode.c_iflag &= ~IUTF8;
        }

        if (!pty()->tcSetAttr(&ttmode)) {
            qCDebug(KonsoleDebug) << "Unable to set terminal attributes.";
        }
    }
#else
    Q_UNUSED(enable)
#endif
}

void Pty::setEraseChar(char eChar)
{
    _eraseChar = eChar;

    if (pty()->masterFd() >= 0) {
        struct ::termios ttmode;
        pty()->tcGetAttr(&ttmode);
        ttmode.c_cc[VERASE] = eChar;

        if (!pty()->tcSetAttr(&ttmode)) {
            qCDebug(KonsoleDebug) << "Unable to set terminal attributes.";
        }
    }
}

char Pty::eraseChar() const
{
    if (pty()->masterFd() >= 0) {
        struct ::termios ttyAttributes;
        pty()->tcGetAttr(&ttyAttributes);
        return ttyAttributes.c_cc[VERASE];
    } else {
        qCDebug(KonsoleDebug) << "Unable to get erase char attribute, terminal not connected.";
        return _eraseChar;
    }
}

void Pty::setInitialWorkingDirectory(const QString &dir)
{
    QString pwd = dir;

    // remove trailing slash in the path when appropriate
    // example: /usr/share/icons/ ==> /usr/share/icons
    if (pwd.length() > 1 && pwd.endsWith(QLatin1Char('/'))) {
        pwd.chop(1);
    }

    setWorkingDirectory(pwd);

    // setting PWD to "." will cause problem for bash & zsh
    if (pwd != QLatin1String(".")) {
        setEnv(QStringLiteral("PWD"), pwd);
    }
}

void Pty::addEnvironmentVariables(const QStringList &environmentVariables)
{
    bool isTermEnvAdded = false;

    for (const QString &pair : environmentVariables) {
        // split on the first '=' character
        const int separator = pair.indexOf(QLatin1Char('='));

        if (separator >= 0) {
            QString variable = pair.left(separator);
            QString value = pair.mid(separator + 1);

            setEnv(variable, value);

            if (variable == QLatin1String("TERM")) {
                isTermEnvAdded = true;
            }
        }
    }

    // extra safeguard to make sure $TERM is always set
    if (!isTermEnvAdded) {
        setEnv(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    }
}

int Pty::start(const QString &programName, const QStringList &programArguments, const QStringList &environmentList)
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
    setEnv(QStringLiteral("LANGUAGE"), QString(), false /* do not overwrite existing value if any */);

    KProcess::start();

    if (waitForStarted()) {
        return 0;
    } else {
        return -1;
    }
}

void Pty::setWriteable(bool writeable)
{
    QT_STATBUF sbuf;
    if (QT_STAT(pty()->ttyName(), &sbuf) == 0) {
        if (writeable) {
            if (::chmod(pty()->ttyName(), sbuf.st_mode | S_IWGRP) < 0) {
                qCDebug(KonsoleDebug) << "Could not set writeable on " << pty()->ttyName();
            }
        } else {
            if (::chmod(pty()->ttyName(), sbuf.st_mode & ~(S_IWGRP | S_IWOTH)) < 0) {
                qCDebug(KonsoleDebug) << "Could not unset writeable on " << pty()->ttyName();
            }
        }
    } else {
        qCDebug(KonsoleDebug) << "Could not stat " << pty()->ttyName();
    }
}

void Pty::closePty()
{
    pty()->close();
}

int Pty::foregroundProcessGroup() const
{
    const int master_fd = pty()->masterFd();

    if (master_fd >= 0) {
        int foregroundPid = tcgetpgrp(master_fd);

        if (foregroundPid != -1) {
            return foregroundPid;
        }
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
    for (int signal = 1; signal < NSIG; signal++) {
        sigaction(signal, &action, nullptr);
    }
}

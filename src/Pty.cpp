/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "Pty.h"

#include "konsoledebug.h"

// Qt
#include <QStringList>
#include <qplatformdefs.h>

#ifndef Q_OS_WIN

// System
#include <csignal>
#include <errno.h>
#include <sys/ioctl.h> //ioctl() and TIOCSWINSZ
#include <termios.h>

// KDE
#include <KPtyDevice>
#include <KSandbox>

using Konsole::Pty;

static int getShellProcessId(QLatin1String tty)
{
    if (!KSandbox::isFlatpak()) {
        qFatal("Shouldn't be called in non flatpak world");
    }
    QProcess proc;
    proc.setProgram(QStringLiteral("ps"));
    proc.setArguments({QStringLiteral("-o"), QStringLiteral("pid"), QStringLiteral("-t"), QStringLiteral("%1").arg(tty), QStringLiteral("--no-headers")});
    KSandbox::startHostProcess(proc, QProcess::ReadOnly);
    if (proc.waitForStarted() && proc.waitForFinished()) {
        proc.setReadChannel(QProcess::StandardOutput);
        char buffer[256];
        int readCount = proc.readLine(buffer, sizeof(buffer));
        auto line = readCount > 0 ? QByteArrayView(buffer, readCount).trimmed() : QByteArrayView();
        bool ok;
        auto pid = line.toInt(&ok);
        if (ok) {
            return pid;
        }
    }
    qWarning("Unable to get shell process id");
    return 0;
}

Pty::Pty(QObject *aParent)
    : Pty(-1, aParent)
{
}

Pty::Pty(int masterFd, QObject *aParent)
    : KPtyProcess(masterFd, aParent)
{
    // Must call parent class child process modifier, as it sets file descriptors ...etc
    auto parentChildProcModifier = KPtyProcess::childProcessModifier();
    setChildProcessModifier([parentChildProcModifier = std::move(parentChildProcModifier)]() {
        if (parentChildProcModifier) {
            parentChildProcModifier();
        }

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
    });

    _windowColumns = 0;
    _windowLines = 0;
    _windowWidth = 0;
    _windowHeight = 0;
    _eraseChar = 0;
    _xon = true;
    _utf8 = true;

    setEraseChar(_eraseChar);
    setFlowControlEnabled(_xon);
    setUtf8Mode(_utf8);

    setWindowSize(_windowColumns, _windowLines, _windowWidth, _windowHeight);

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

    Q_EMIT receivedData(data.constData(), data.length());
}

void Pty::setWindowSize(int columns, int lines, int width, int height)
{
    _windowColumns = columns;
    _windowLines = lines;
    _windowWidth = width;
    _windowHeight = height;

    if (pty()->masterFd() >= 0) {
        pty()->setWinSize(_windowLines, _windowColumns, _windowHeight, _windowWidth);
    }
}

QSize Pty::windowSize() const
{
    return {_windowColumns, _windowLines};
}

QSize Pty::pixelSize() const
{
    return {_windowWidth, _windowHeight};
}

void Pty::setFlowControlEnabled(bool enable)
{
    // Only set IXON flow control ^S ^Q capabilities on the master
    // side.  IXOFF flow control is not used by PTYs, that's for
    // hardware terminals such as those working over RS232.
    _xon = enable;

    if (pty()->masterFd() >= 0) {
        struct ::termios ttmode;
        pty()->tcGetAttr(&ttmode);
        if (enable) {
            ttmode.c_iflag |= IXON;
        } else {
            ttmode.c_iflag &= ~IXON;
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
        return ((ttmode.c_iflag & IXON) != 0U);
    } else {
        qCDebug(KonsoleDebug) << "Unable to get flow control status, terminal not connected.";
        return _xon;
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

    setProgram(programName, programArguments);

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
        if (KSandbox::isFlatpak()) {
            _shellProcessId = getShellProcessId(QLatin1String(pty()->ttyName()));
        }
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
    if (KSandbox::isFlatpak()) {
        QProcess proc;
        proc.setProgram(QStringLiteral("ps"));
        proc.setArguments({QStringLiteral("-o"),
                           QStringLiteral("%p\t"),
                           QStringLiteral("-o"),
                           QStringLiteral("stat"),
                           QStringLiteral("--tty"),
                           QString::fromLatin1(pty()->ttyName()),
                           QStringLiteral("--no-headers")});

        KSandbox::startHostProcess(proc, QProcess::ReadOnly);
        if (proc.waitForStarted() && proc.waitForFinished()) {
            while (proc.canReadLine()) {
                char buffer[256];
                int readCount = proc.readLine(buffer, sizeof(buffer));
                auto line = readCount > 0 ? QByteArrayView(buffer, readCount) : QByteArrayView();
                int i = line.indexOf('\t');
                if (i == -1) {
                    return 0;
                }
                QByteArrayView pid = line.mid(0, i);
                QByteArrayView stat = line.mid(i + 1);
                if (stat.contains("+")) {
                    return pid.trimmed().toInt();
                }
            }
        }
        return 0;
    }

    const int master_fd = pty()->masterFd();

    if (master_fd >= 0) {
        int foregroundPid = tcgetpgrp(master_fd);

        if (foregroundPid != -1) {
            return foregroundPid;
        } else {
            qCWarning(KonsoleDebug, "Failed to get foreground process group id for %d: %s", master_fd, strerror(errno));
            return 0;
        }
    }
    qWarning(KonsoleDebug, "foregroundProcessGroup master_fd < 0");

    return 0;
}

int Pty::shellProcessId() const
{
    return KSandbox::isFlatpak() ? _shellProcessId : processId();
}

int Pty::flatpakSpawnProcessId() const
{
    if (KSandbox::isFlatpak()) {
        return processId();
    }
    qFatal("Shouldn't be called, we are not in flatpak");
    return processId();
}

#else // Windows backend

#include "ptyqt/conptyprocess.h"

using Konsole::Pty;

Pty::Pty(QObject *aParent)
    : Pty(-1, aParent)
{
}

Pty::Pty(int masterFd, QObject *aParent)
    : QObject(aParent)
{
    Q_UNUSED(masterFd)

    m_proc = std::make_unique<ConPtyProcess>();
    if (!m_proc->isAvailable()) {
        m_proc.reset();
    }

    _windowColumns = 0;
    _windowLines = 0;
    _windowWidth = 0;
    _windowHeight = 0;
    _eraseChar = 0;
    _xon = true;
    _utf8 = true;

    setEraseChar(_eraseChar);
}

Pty::~Pty() = default;

void Pty::sendData(const QByteArray &data)
{
    if (m_proc) {
        m_proc->write(data.constData(), data.length());
    }
}

void Pty::dataReceived()
{
    if (m_proc) {
        auto data = m_proc->readAll();
        Q_EMIT receivedData(data.constData(), data.length());
    }
}

void Pty::setWindowSize(int columns, int lines, int, int)
{
    if (m_proc && isRunning())
        m_proc->resize(columns, lines);
}

QSize Pty::windowSize() const
{
    if (!m_proc) {
        return {};
    }
    auto s = m_proc->size();
    return QSize(s.first, s.second);
}

QSize Pty::pixelSize() const
{
    return QSize();
}

void Pty::setFlowControlEnabled(bool enable)
{
    _xon = enable;
}

bool Pty::flowControlEnabled() const
{
    return false;
}

void Pty::setUtf8Mode(bool)
{
}

void Pty::setEraseChar(char eChar)
{
    _eraseChar = eChar;
}

char Pty::eraseChar() const
{
    return _eraseChar;
}

void Pty::setInitialWorkingDirectory(const QString & /*dir*/)
{
}

void Pty::addEnvironmentVariables(const QStringList & /*environmentVariables*/)
{
}

int Pty::start(const QString &program, const QStringList &arguments, const QString &workingDir, const QStringList &environment, int cols, int lines)
{
    if (!m_proc || !m_proc->isAvailable()) {
        return -1;
    }
    bool res = m_proc->startProcess(program, arguments, workingDir, environment, cols, lines);
    if (!res) {
        return -1;
    } else {
        auto n = m_proc->notifier();
        connect(n, &QIODevice::readyRead, this, &Pty::dataReceived);
        connect(m_proc.get(), &IPtyProcess::exited, this, [this] {
            Q_EMIT finished(exitCode(), QProcess::NormalExit);
        });
        connect(n, &QIODevice::aboutToClose, this, [this] {
            Q_EMIT finished(exitCode(), QProcess::NormalExit);
        });
    }
    return 0;
}

void Pty::setWriteable(bool)
{
}

void Pty::closePty()
{
    if (m_proc) {
        m_proc->kill();
    }
}

int Pty::foregroundProcessGroup() const
{
    return 0;
}

int Pty::shellProcessId() const
{
    if (m_proc && m_proc->isAvailable()) {
        return m_proc->pid();
    }
    return 0;
}

#endif

#include "moc_Pty.cpp"

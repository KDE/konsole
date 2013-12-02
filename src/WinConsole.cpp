/*
    This file is part of Konsole, an X terminal.
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

// Own
#include "WinConsole.h"

// KcwSH
#include <kcwsh/terminal.h>
#include <kcwsh/outputwriter.h>
#include <kcwsh/inputreader.h>

// Qt
#include <QtCore/QStringList>

// KDE
#include <KDebug>

using Konsole::WinConsole;

WinConsole::WinConsole(QObject* aParent)
 : QProcess(aParent)
{
    setInputReader(new KcwSH::InputReader);
    setOutputWriter(new KcwSH::OutputWriter(this));
}

int WinConsole::pid() const
{
    return KcwSH::Terminal::pid();
}

int WinConsole::foregroundProcessGroup() const
{
    return foregroundPid();
}

void WinConsole::closePty()
{
    KcwSH::Terminal::quit();
}

QProcess::ProcessState WinConsole::state() const
{
    if(isSetup())
        return QProcess::Running;
    else
        return QProcess::NotRunning;
}


void WinConsole::setWindowSize(int columns, int lines)
{
    COORD c = { columns, lines };
    setTerminalSize(c);
}

QSize WinConsole::windowSize() const
{
    COORD c = terminalSize();
    return QSize(c.X, c.Y);
}

void WinConsole::setInitialWorkingDirectory(const QString& dir)
{
    KcwSH::Terminal::setInitialWorkingDirectory(reinterpret_cast<const WCHAR*>(dir.utf16()));
}

int WinConsole::start(const QString& program, const QStringList& arguments, const QStringList& environment)
{
    QString cmd = program + QLatin1Char(' ') + arguments.join(QLatin1String(" "));
    setCmd(reinterpret_cast<const WCHAR*>(cmd.utf16()));

    KcwProcess::KcwProcessEnvironment env = KcwProcess::KcwProcessEnvironment::getCurrentEnvironment();
    foreach(QString entry, environment) {
        const int l = entry.indexOf('=');
        const QString var = entry.left(l);
        const QString value = entry.mid(l + 1);
        env[std::wstring(reinterpret_cast<const WCHAR*>(var.utf16()))] = std::wstring(reinterpret_cast<const WCHAR*>(value.utf16()));
    }
    KcwSH::Terminal::setEnvironment(env);
    KcwSH::Terminal::start();
    return 0;
}

void WinConsole::setWriteable(bool)
{
}

bool WinConsole::waitForFinished(int msecs)
{
    // FIXME:Patrick implement me
    return true;
}

void WinConsole::setFlowControlEnabled(bool e)
{
}

bool WinConsole::flowControlEnabled() const
{
    return false;
}

void WinConsole::sizeChanged()
{
    OutputDebugStringA(__FUNCTION__);
}

void WinConsole::bufferChanged()
{
    emit outputChanged();
}

void WinConsole::cursorPositionChanged()
{
    COORD c = outputWriter()->cursorPosition();
    emit cursorChanged(c.X + 1, c.Y + 1);
}

void WinConsole::titleChanged()
{
    std::wstring ws = title();
    QString title = QString::fromUtf16(
        reinterpret_cast<const ushort*>(ws.c_str())
    );
    emit termTitleChanged(0, title);
}

void WinConsole::hasQuit()
{
    OutputDebugStringA(__FUNCTION__);
    Terminal::quit();
    emit finished(0, QProcess::NormalExit);
}


#include "WinConsole.moc"

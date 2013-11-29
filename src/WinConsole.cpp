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

// Konsole
#include "WinTerminal.h"

using Konsole::WinConsole;

WinConsole::WinConsole(QObject* aParent)
 : QProcess(aParent)
{
    _terminal = new WinTerminal;
    connect(_terminal, SIGNAL(cursorChanged(int, int)), this, SIGNAL(cursorChanged(int, int)));
    connect(_terminal, SIGNAL(termTitleChanged(int, QString)), this, SIGNAL(termTitleChanged(int, QString)));
    connect(_terminal, SIGNAL(outputChanged()), this, SIGNAL(outputChanged()));
    connect(_terminal, SIGNAL(finished(int, QProcess::ExitStatus)), this, SIGNAL(finished(int, QProcess::ExitStatus)));

    _terminal->setInputReader(new KcwSH::InputReader);
    _terminal->setOutputWriter(new KcwSH::OutputWriter(_terminal));
}

int WinConsole::pid() const
{
    return _terminal->pid();
}

int WinConsole::foregroundProcessGroup() const
{
    return _terminal->foregroundPid();
}

void WinConsole::closePty()
{
    _terminal->quit();
}

QProcess::ProcessState WinConsole::state() const
{
    if(_terminal->isSetup())
        return QProcess::Running;
    else
        return QProcess::NotRunning;
}


void WinConsole::setWindowSize(int columns, int lines)
{
    COORD c = { columns, lines };
    _terminal->setTerminalSize(c);
}

QSize WinConsole::windowSize() const
{
    COORD c = _terminal->terminalSize();
    return QSize(c.X, c.Y);
}

void WinConsole::setInitialWorkingDirectory(const QString& dir)
{
    _terminal->setInitialWorkingDirectory(reinterpret_cast<const WCHAR*>(dir.utf16()));
}

int WinConsole::start(const QString& program, const QStringList& arguments, const QStringList& environment)
{
    QString cmd = program + QLatin1Char(' ') + arguments.join(QLatin1String(" "));
    _terminal->setCmd(reinterpret_cast<const WCHAR*>(cmd.utf16()));

    KcwProcess::KcwProcessEnvironment env = KcwProcess::KcwProcessEnvironment::getCurrentEnvironment();
    foreach(QString entry, environment) {
        const int l = entry.indexOf('=');
        const QString var = entry.left(l);
        const QString value = entry.mid(l + 1);
        env[std::wstring(reinterpret_cast<const WCHAR*>(var.utf16()))] = std::wstring(reinterpret_cast<const WCHAR*>(value.utf16()));
    }
    _terminal->setEnvironment(env);
    _terminal->start();
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

#include "WinConsole.moc"

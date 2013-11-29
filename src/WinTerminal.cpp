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
#include "WinTerminal.h"

// KcwSH
#include <kcwsh/terminal.h>
#include <kcwsh/outputwriter.h>
#include <kcwsh/inputreader.h>

// Qt
#include <QtCore/QStringList>

// KDE
#include <KDebug>

using Konsole::WinTerminal;

WinTerminal::WinTerminal()
{
}

void WinTerminal::sizeChanged()
{
    OutputDebugStringA(__FUNCTION__);
}

void WinTerminal::bufferChanged()
{
    emit outputChanged();
}

void WinTerminal::cursorPositionChanged()
{
    COORD c = outputWriter()->cursorPosition();
    emit cursorChanged(c.X + 1, c.Y + 1);
}

void WinTerminal::titleChanged()
{
    std::wstring ws = title();
    QString title = QString::fromUtf16(
        reinterpret_cast<const ushort*>(ws.c_str())
    );
    emit termTitleChanged(0, title);
}

void WinTerminal::hasQuit()
{
    OutputDebugStringA(__FUNCTION__);
    Terminal::quit();
    emit finished(0, QProcess::NormalExit);
}


#include "WinTerminal.moc"

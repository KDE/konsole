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

#ifndef WINTERMINAL_H
#define WINTERMINAL_H

// Qt
#include <QtCore/QObject>
#include <QtCore/QProcess>

// KcwSH
#include <kcwsh/terminal.h>

// Konsole
#include "konsole_export.h"

namespace Konsole
{
class Screen;

class KONSOLEPRIVATE_EXPORT WinTerminal : public QObject, public KcwSH::Terminal
{
    Q_OBJECT
    public:
        WinTerminal();

        void sizeChanged();
        void bufferChanged();
        void cursorPositionChanged();

        void titleChanged();

        void hasQuit();

//     public slots:
//         void setScreen(Screen* s);
    signals:
        void cursorChanged(int x, int y);
        void termTitleChanged(int i, QString title);
        void outputChanged();
        void finished(int, QProcess::ExitStatus);
    private:
        Screen *_screen;
};
}
#endif /* WINTERMINAL_H */
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

#ifndef WINCONEMULATION_H
#define WINCONEMULATION_H

// Qt
#include <QtCore/QHash>

// Konsole
#include "Emulation.h"
#include "Screen.h"

class QTimer;
class QKeyEvent;

namespace Konsole
{

class WinConsole;
/**
 * Provides an xterm compatible terminal emulation based on the DEC VT102 terminal.
 * A full description of this terminal can be found at http://vt100.net/docs/vt102-ug/
 *
 * In addition, various additional xterm escape sequences are supported to provide
 * features such as mouse input handling.
 * See http://rtfm.etla.org/xterm/ctlseq.html for a description of xterm's escape
 * sequences.
 *
 */
class WinConEmulation : public Emulation
{
    Q_OBJECT

public:
    /** Constructs a new emulation */
    WinConEmulation();
    ~WinConEmulation();

    // reimplemented from Emulation
    virtual void clearEntireScreen();
    virtual void reset();
    virtual char eraseChar() const;

    void setConsole(WinConsole* w);

public slots:
    // reimplemented from Emulation
    virtual void sendString(const char*, int length = -1);
    virtual void sendText(const QString& text);
    virtual void sendKeyEvent(QKeyEvent*);
    virtual void sendMouseEvent(int buttons, int column, int line, int eventType);
    void updateCursorPosition(int x, int y);
    void updateBuffer();

protected:
    // reimplemented from Emulation
    virtual void setMode(int mode);
    virtual void resetMode(int mode);
    virtual void receiveChar(int cc);

private:
    WinConsole* _console;
};
}

#endif // WINCONEMULATION_H

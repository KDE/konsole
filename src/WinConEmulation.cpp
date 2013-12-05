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
#include "WinConEmulation.h"

// Qt
#include <QtCore/QEvent>
#include <QtGui/QKeyEvent>

// KDE
#include <KLocalizedString>
#include <KDebug>

// KcwSH
#include <kcwsh/inputreader.h>
#include <kcwsh/outputwriter.h>

// Konsole
#include "Screen.h"
#include "Character.h"
#include "TerminalDisplay.h"
#include "WinConsole.h"

using namespace Konsole;

WinConEmulation::WinConEmulation()
    : Emulation()
{
    usesMouseChanged(true);
    reset();
}

WinConEmulation::~WinConEmulation()
{
}

void WinConEmulation::clearEntireScreen()
{
    _currentScreen->clearEntireScreen();
    bufferedUpdate();
}

void WinConEmulation::reset()
{
    bufferedUpdate();
}

// process an incoming unicode character
void WinConEmulation::receiveChar(int cc)
{
}

void WinConEmulation::setConsole(WinConsole* w)
{
    _console = w;
}

void WinConEmulation::updateCursorPosition(int x, int y)
{
    _currentScreen->setCursorYX(y, x);
    emit outputChanged();
}

void WinConEmulation::sendString(const char* s, int length)
{
    sendText(QString::fromLatin1(s));
}

/*!
    `cx',`cy' are 1-based.
    `cb' indicates the button pressed or released (0-2) or scroll event (4-5).

    eventType represents the kind of mouse action that occurred:
        0 = Mouse button press
        1 = Mouse drag
        2 = Mouse button release
*/

void WinConEmulation::sendMouseEvent(int cb, int cx, int cy, int eventType)
{
    kDebug() << cb << ":" << cx << "x" << cy << eventType;
}

void WinConEmulation::sendText(const QString& text)
{
    _console->sendText(reinterpret_cast<const wchar_t*>(text.utf16()));
}

void WinConEmulation::sendKeyEvent(QKeyEvent* event)
{
    if(event->key() == Qt::Key_C && event->modifiers() == Qt::ControlModifier)
    {
        _console->inputReader()->sendCtrlC();
        return;
    }

    bool keyDown = true;
    INPUT_RECORD ir;
    ZeroMemory(&ir, sizeof(INPUT_RECORD));
    ir.EventType = KEY_EVENT;
    ir.Event.KeyEvent.bKeyDown = keyDown;
    ir.Event.KeyEvent.wRepeatCount = event->count();
    ir.Event.KeyEvent.wVirtualKeyCode = event->nativeVirtualKey();
    ir.Event.KeyEvent.wVirtualScanCode = event->nativeScanCode();
    ir.Event.KeyEvent.uChar.UnicodeChar = static_cast<WCHAR>(event->text().utf16()[0]);
    ir.Event.KeyEvent.dwControlKeyState = event->nativeModifiers();
    _console->inputReader()->sendKeyboardEvents(&ir, 1);

}

void WinConEmulation::setMode(int m)
{
}

void WinConEmulation::resetMode(int m)
{
}

char WinConEmulation::eraseChar() const
{
    return 0;
}

void WinConEmulation::updateBuffer()
{
    KcwSH::OutputWriter *ow = _console->outputWriter();
    for(int i = 0; i < _currentScreen->_lines; i++) {
        Screen::ImageLine &v = _currentScreen->_screenLines[i];
        if(v.size() < _currentScreen->_columns) {
            v.resize(_currentScreen->_columns);
        }
        for(int j = 0; j < _currentScreen->_columns; j++) {
            COORD c;
            c.X = i; c.Y = j;
            WORD attr = ow->attributesAt(c);
            v[j] = Character(ow->at(c),
                             CharacterColor(COLOR_SPACE_SYSTEM, attr & 0xf),
                             CharacterColor(COLOR_SPACE_SYSTEM, (attr >> 4) & 0xf));
        }
    }
    emit outputChanged();
}

#include "WinConEmulation.moc"


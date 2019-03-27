/*
    This file is part of Konsole, an X terminal.

    Copyright 2015 by René J.V. Bertin <rjvbertin@gmail.com>

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

#ifndef SHORTCUT_H
#define SHORTCUT_H

#include <qnamespace.h>

namespace Konsole {
/**
 * Platform-specific main shortcut "opcode":
 */
enum Modifier {
#ifdef Q_OS_MACOS
    ACCEL = Qt::META,
    LEFT = Qt::Key_BracketLeft,
    RIGHT = Qt::Key_BracketRight
#else
    ACCEL = Qt::CTRL,
    LEFT = Qt::Key_Left,
    RIGHT = Qt::Key_Right
#endif
};
}

#endif //SHORTCUT_H

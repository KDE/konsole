/*
    SPDX-FileCopyrightText: 2015 René J.V. Bertin <rjvbertin@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SHORTCUT_H
#define SHORTCUT_H

#include <qnamespace.h>

namespace Konsole
{
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
    RIGHT = Qt::Key_Right,
#endif
};
}

#endif // SHORTCUT_H

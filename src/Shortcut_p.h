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
    // Use plain Command key for shortcuts
    ACCEL = Qt::CTRL,
#else
    // Use Ctrl+Shift for shortcuts
    ACCEL = Qt::CTRL | Qt::SHIFT,
#endif
};
}

#endif // SHORTCUT_H

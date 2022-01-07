/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>
    SPDX-FileCopyrightText: 2021 Jonah Brüchert <jbb@kaidan.im>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "TerminalBell.h"

#include <QTimer>
#include <QWidget>

#include <KNotification>

#include <chrono>

using namespace std::literals::chrono_literals;

namespace Konsole
{
constexpr auto MASK_TIMEOUT = 500ms;

TerminalBell::TerminalBell(Enum::BellModeEnum bellMode)
    : _bellMode(bellMode)
{
}

void TerminalBell::bell(QWidget *terminalDisplay, const QString &message, bool terminalHasFocus)
{
    switch (_bellMode) {
    case Enum::SystemBeepBell:
        KNotification::beep();
        break;
    case Enum::NotifyBell:
        // STABLE API:
        //     Please note that these event names, "BellVisible" and "BellInvisible",
        //     should not change and should be kept stable, because other applications
        //     that use this code via KPart rely on these names for notifications.
        KNotification::event(terminalHasFocus ? QStringLiteral("BellVisible") : QStringLiteral("BellInvisible"), message, QPixmap(), terminalDisplay);
        break;
    case Enum::VisualBell:
        Q_EMIT visualBell();
        break;
    default:
        break;
    }

    // limit the rate at which bells can occur.
    // ...mainly for sound effects where rapid bells in sequence
    // produce a horrible noise.
    _bellMasked = true;
    QTimer::singleShot(MASK_TIMEOUT, this, [this]() {
        _bellMasked = false;
    });
}

void TerminalBell::setBellMode(Enum::BellModeEnum mode)
{
    _bellMode = mode;
}

}

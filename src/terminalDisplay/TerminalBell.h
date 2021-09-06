/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>
    SPDX-FileCopyrightText: 2021 Jonah Brüchert <jbb@kaidan.im>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

#include <Enumeration.h>

namespace Konsole
{
class TerminalBell : public QObject
{
    Q_OBJECT

public:
    TerminalBell(Enum::BellModeEnum bellMode);
    void bell(const QString &message, bool terminalHasFocus);

    /**
     * Sets the type of effect used to alert the user when a 'bell' occurs in the
     * terminal session.
     *
     * The terminal session can trigger the bell effect by calling bell() with
     * the alert message.
     */
    void setBellMode(Enum::BellModeEnum mode);

Q_SIGNALS:
    void visualBell();

private:
    Enum::BellModeEnum _bellMode;
    bool _bellMasked = false;
};

}

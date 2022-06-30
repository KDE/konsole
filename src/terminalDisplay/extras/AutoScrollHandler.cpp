/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "AutoScrollHandler.h"
#include "../TerminalDisplay.h"
#include <QAccessible>
#include <QApplication>

using namespace Konsole;

AutoScrollHandler::AutoScrollHandler(QWidget *parent)
    : QObject(parent)
{
    parent->installEventFilter(this);
}

void AutoScrollHandler::timerEvent(QTimerEvent *event)
{
    if (event->timerId() != _timerId) {
        return;
    }

    auto *terminalDisplay = static_cast<TerminalDisplay *>(parent());
    QMouseEvent mouseEvent(QEvent::MouseMove,
                           widget()->mapFromGlobal(QCursor::pos()),
                           Qt::NoButton,
                           Qt::LeftButton,
                           terminalDisplay->usesMouseTracking() ? Qt::ShiftModifier : Qt::NoModifier);

    QApplication::sendEvent(widget(), &mouseEvent);
}

bool AutoScrollHandler::eventFilter(QObject *watched, QEvent *event)
{
    Q_ASSERT(watched == parent());
    Q_UNUSED(watched)

    switch (event->type()) {
    case QEvent::MouseMove: {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        bool mouseInWidget = widget()->rect().contains(mouseEvent->pos());
        auto *terminalDisplay = static_cast<TerminalDisplay *>(parent());
        if (mouseInWidget) {
            if (_timerId != 0) {
                killTimer(_timerId);
            }

            _timerId = 0;
        } else {
            if ((_timerId == 0) && terminalDisplay->selectionState() != 0 && ((mouseEvent->buttons() & Qt::LeftButton) != 0U)) {
                _timerId = startTimer(100);
            }
        }

        break;
    }
    case QEvent::MouseButtonRelease: {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if ((_timerId != 0) && ((mouseEvent->buttons() & ~Qt::LeftButton) != 0U)) {
            killTimer(_timerId);
            _timerId = 0;
        }
        break;
    }
    default:
        break;
    }

    return false;
}

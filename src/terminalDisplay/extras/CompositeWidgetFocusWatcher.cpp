/*
    This file is part of Konsole, a terminal emulator for KDE.

    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

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

#include "CompositeWidgetFocusWatcher.h"

#include <QFocusEvent>

using namespace Konsole;

CompositeWidgetFocusWatcher::CompositeWidgetFocusWatcher(QWidget *compositeWidget)
    : QObject(compositeWidget)
    , _compositeWidget(compositeWidget)
{
    registerWidgetAndChildren(compositeWidget);
}

bool CompositeWidgetFocusWatcher::eventFilter(QObject *watched, QEvent *event)
{
    switch(event->type()) {
    case QEvent::Close:
    case QEvent::DeferredDelete:
    case QEvent::Destroy:
        removeEventFilter(watched);
        disconnect(this, &CompositeWidgetFocusWatcher::compositeFocusChanged, watched, nullptr);
        break;
    case QEvent::FocusIn:
        emit compositeFocusChanged(true);
        break;
    case QEvent::FocusOut:
        if(static_cast<QFocusEvent *>(event)->reason() != Qt::PopupFocusReason) {
            emit compositeFocusChanged(false);
        }
        break;
    default:
        break;
    }
    return false;
}

void CompositeWidgetFocusWatcher::registerWidgetAndChildren(QWidget *widget)
{
    Q_ASSERT(widget != nullptr);

    if (widget->focusPolicy() != Qt::NoFocus) {
        widget->installEventFilter(this);
    }
    for (auto *child: widget->children()) {
        auto *childWidget = qobject_cast<QWidget *>(child);
        if (childWidget != nullptr) {
            registerWidgetAndChildren(childWidget);
        }
    }
}

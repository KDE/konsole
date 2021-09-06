/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
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
    switch (event->type()) {
    case QEvent::Close:
    case QEvent::DeferredDelete:
    case QEvent::Destroy:
        removeEventFilter(watched);
        disconnect(this, &CompositeWidgetFocusWatcher::compositeFocusChanged, watched, nullptr);
        break;
    case QEvent::FocusIn:
        Q_EMIT compositeFocusChanged(true);
        break;
    case QEvent::FocusOut:
        if (static_cast<QFocusEvent *>(event)->reason() != Qt::PopupFocusReason) {
            Q_EMIT compositeFocusChanged(false);
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
    for (auto *child : widget->children()) {
        auto *childWidget = qobject_cast<QWidget *>(child);
        if (childWidget != nullptr) {
            registerWidgetAndChildren(childWidget);
        }
    }
}

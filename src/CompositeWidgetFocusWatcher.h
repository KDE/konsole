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

#ifndef COMPOSITEWIDGET_H
#define COMPOSITEWIDGET_H

#include <QWidget>

namespace Konsole
{
// Watches compositeWidget and all its focusable children,
// and emits focusChanged() signal when either compositeWidget's
// or a child's focus changed.
// Limitation: children added after the object was created
// will not be registered.
class CompositeWidgetFocusWatcher : public QObject
{
    Q_OBJECT

public:
    explicit CompositeWidgetFocusWatcher(QWidget *compositeWidget);
    bool eventFilter(QObject *watched, QEvent *event) Q_DECL_OVERRIDE;

Q_SIGNALS:
    void compositeFocusChanged(bool focused);

private:
    void registerWidgetAndChildren(QWidget *widget);

    QWidget *_compositeWidget;
};

} // namespace Konsole

#endif // COMPOSITEWIDGET_H

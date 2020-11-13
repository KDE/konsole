/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
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

/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef AUTOSCROLLHANDLER_H
#define AUTOSCROLLHANDLER_H

#include <QWidget>

namespace Konsole
{
class AutoScrollHandler : public QObject
{
    Q_OBJECT

public:
    explicit AutoScrollHandler(QWidget *parent);

protected:
    void timerEvent(QTimerEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *widget() const
    {
        return static_cast<QWidget *>(parent());
    }

    int _timerId = 0;
};

}

#endif

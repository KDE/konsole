/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>
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

    int _timerId;
};

}

#endif

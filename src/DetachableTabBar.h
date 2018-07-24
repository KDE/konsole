/*
    Copyright 2018 by Tomaz Canabrava <tcanabrava@kde.org>

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

#ifndef DETACHABLETABBAR_H
#define DETACHABLETABBAR_H

#include <QTabBar>
#include <QCursor>

class DetachableTabBar : public QTabBar {
    Q_OBJECT
public:
    explicit DetachableTabBar(QWidget *parent = nullptr);
Q_SIGNALS:
    void detachTab(int idx);
protected:
    void mouseMoveEvent(QMouseEvent*event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
private:
    bool _draggingOutside;
    QCursor _originalCursor;
};

#endif

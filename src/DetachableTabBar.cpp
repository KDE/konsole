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

#include "DetachableTabBar.h"
#include <QMouseEvent>

DetachableTabBar::DetachableTabBar(QWidget *parent) :
    QTabBar(parent),
    _draggingOutside(false) {}

void DetachableTabBar::mouseMoveEvent(QMouseEvent *event)
{
    QTabBar::mouseMoveEvent(event);
    if (!contentsRect().adjusted(-30,-30,30,30).contains(event->pos())) {
        if (!_draggingOutside) {
            _draggingOutside = true;
            _originalCursor = cursor();
            setCursor(QCursor(Qt::DragCopyCursor));
        }
    } else if (_draggingOutside) {
        _draggingOutside = false;
        setCursor(_originalCursor);
    }
}

void DetachableTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    QTabBar::mouseReleaseEvent(event);
    if (!contentsRect().adjusted(-30,-30,30,30).contains(event->pos())) {
        setCursor(_originalCursor);
        emit detachTab(currentIndex());
    }
}

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
#include "KonsoleSettings.h"
#include "ViewContainer.h"

#include <QMouseEvent>
#include <QApplication>

namespace Konsole {

DetachableTabBar::DetachableTabBar(QWidget *parent) :
    QTabBar(parent),
    dragType(DragType::NONE),
    _originalCursor(cursor()),
    tabId(-1)
{
    setUsesScrollButtons(false);
    setElideMode(Qt::TextElideMode::ElideMiddle);
}

void DetachableTabBar::middleMouseButtonClickAt(const QPoint& pos)
{
    tabId = tabAt(pos);

    if (tabId != -1) {
        emit closeTab(tabId);
    }
}

void DetachableTabBar::mousePressEvent(QMouseEvent *event)
{
    QTabBar::mousePressEvent(event);
    _containers = window()->findChildren<Konsole::TabbedViewContainer*>();
}

void DetachableTabBar::mouseMoveEvent(QMouseEvent *event)
{
    QTabBar::mouseMoveEvent(event);
    auto widgetAtPos = qApp->topLevelAt(event->globalPos());
    if (widgetAtPos != nullptr) {
        if (window() == widgetAtPos->window()) {
            if (dragType != DragType::NONE) {
                dragType = DragType::NONE;
                setCursor(_originalCursor);
            }
        } else {
            if (dragType != DragType::WINDOW) {
                dragType = DragType::WINDOW;
                setCursor(QCursor(Qt::DragMoveCursor));
            }
        }
    } else if (!contentsRect().adjusted(-30,-30,30,30).contains(event->pos())) {
        // Don't let it detach the last tab.
        if (count() == 1) {
            return;
        }
        if (dragType != DragType::OUTSIDE) {
            dragType = DragType::OUTSIDE;
            setCursor(QCursor(Qt::DragCopyCursor));
        }
    }
}

void DetachableTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    QTabBar::mouseReleaseEvent(event);

    switch(event->button()) {
        case Qt::MiddleButton : if (KonsoleSettings::closeTabOnMiddleMouseButton()) {
                                    middleMouseButtonClickAt(event->pos());
                                }

                                tabId = tabAt(event->pos());
                                if (tabId == -1) {
                                    emit newTabRequest();
                                }
                                break;
        case Qt::LeftButton: _containers = window()->findChildren<Konsole::TabbedViewContainer*>(); break;
        default: break;
    }

    setCursor(_originalCursor);

    if (contentsRect().adjusted(-30,-30,30,30).contains(event->pos())) {
        return;
    }

    auto widgetAtPos = qApp->topLevelAt(event->globalPos());
    if (widgetAtPos == nullptr) {
        if (count() != 1) {
            emit detachTab(currentIndex());
        }
    } else if (window() != widgetAtPos->window()) {
        if (_containers.size() == 1 || count() > 1) {
            emit moveTabToWindow(currentIndex(), widgetAtPos);
        }
    }
}

}

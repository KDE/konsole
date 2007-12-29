/*
    Copyright (C) 2006-2007 by Robert Knight <robertknight@gmail.com>

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

// Own
#include "ProfileListWidget.h"

// Qt
#include <KDebug>
#include <QtGui/QDrag>
#include <QtGui/QKeyEvent>
#include <QtCore/QMimeData>

static const char* konsoleSessionMimeFormat = "konsole/session";

ProfileListWidget::ProfileListWidget(QWidget* parent)
    : QListWidget(parent)
{
    // use large icons so that there is a big area for the user to click
    // on to switch between sessions
    setIconSize( QSize(32,32) );

    // turn the frame off
    setFrameStyle( QFrame::NoFrame );

    QPalette p = palette();
    p.setBrush( QPalette::Base , QColor(220,220,220) );
    setPalette(p);
}

void ProfileListWidget::startDrag(Qt::DropActions /*supportedActions*/)
{
    kDebug() << "drag and drop started in session list widget";

    QMimeData* mimeData = new QMimeData();

    QByteArray data;
    data.setNum(42);
    mimeData->setData(konsoleSessionMimeFormat,data);

    QDrag* drag = new QDrag(this);
    drag->setMimeData(mimeData);

    Qt::DropAction action = drag->start( Qt::MoveAction );

    if ( action & Qt::MoveAction )
    {
        emit takeSessionEvent(currentRow());
    }
}

void ProfileListWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if ( event->mimeData()->hasFormat(konsoleSessionMimeFormat) )
    {
        event->accept();
    }
}

void ProfileListWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if ( event->mimeData()->hasFormat(konsoleSessionMimeFormat) )
    {
        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
}

void ProfileListWidget::dropEvent(QDropEvent* event)
{
    if ( event->mimeData()->hasFormat(konsoleSessionMimeFormat) )
    {
        event->setDropAction(Qt::MoveAction);
        event->accept();

        emit dropSessionEvent( event->mimeData()->data(konsoleSessionMimeFormat).toInt() ); 
    }
}


#include "ProfileListWidget.moc"

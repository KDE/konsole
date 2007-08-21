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

#ifndef PROFILELISTWIDGET_H
#define PROFILELISTWIDGET_H

// Qt
#include <QtGui/QListWidget>

class ProfileListWidget : public QListWidget
{
Q_OBJECT

public:
    ProfileListWidget(QWidget* parent);

signals:
    void takeSessionEvent(int itemIndex);
    void dropSessionEvent(int id); 

protected:
    virtual void startDrag(Qt::DropActions supportedActions);
    virtual void dropEvent(QDropEvent* event);
    virtual void dragEnterEvent(QDragEnterEvent* event);
    virtual void dragMoveEvent(QDragMoveEvent* event);

};

/*class SessionListDelegate : public QAbstractItemDelegate
{
Q_OBJECT

public:
    virtual void paint(QPainter* painter , const QStyleOptionViewItem& option,
                       const QModelIndex& index) const;

    virtual QSize sizeHint( const QStyleOptionViewItem& option,
                            const QModelIndex& index) const;
};*/

#endif // PROFILELISTWIDGET_H

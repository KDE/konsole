/*
    This file is part of the Konsole Terminal.

    Copyright 2006-2008 Robert Knight <robertknight@gmail.com>

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

#ifndef VIEWCONTAINERTABBAR_H
#define VIEWCONTAINERTABBAR_H

#include <QElapsedTimer>
#include <QTabBar>

class QLabel;

namespace Konsole {
class TabbedViewContainer;

class ViewContainerTabBar : public QTabBar
{
    Q_OBJECT

public:
    ViewContainerTabBar(QWidget *parent, TabbedViewContainer *container);
    ~ViewContainerTabBar() Q_DECL_OVERRIDE;

    // returns a pixmap image of a tab for use with QDrag
    QPixmap dragDropPixmap(int tab);

    // set the mimetype of which the tabbar support d&d
    void setSupportedMimeType(const QString &mimeType);

    // return associated tabbed container
    TabbedViewContainer *connectedTabbedViewContainer();

Q_SIGNALS:
    void initiateDrag(int index);
    void querySourceIndex(const QDropEvent *event, int &sourceIndex) const;
    void moveViewRequest(int index, const QDropEvent *event, bool &success,
                         TabbedViewContainer *sourceTabbedContainer);

protected:
    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void dragEnterEvent(QDragEnterEvent *event) Q_DECL_OVERRIDE;
    void dragLeaveEvent(QDragLeaveEvent *event) Q_DECL_OVERRIDE;
    void dragMoveEvent(QDragMoveEvent *event) Q_DECL_OVERRIDE;
    void dropEvent(QDropEvent *event) Q_DECL_OVERRIDE;

private:
    Q_DISABLE_COPY(ViewContainerTabBar)

    // show the indicator arrow which shows where a dropped tab will
    // be inserted at 'index'
    void setDropIndicator(int index, bool drawDisabled = false);

    // returns the index at which a tab will be inserted if the mouse
    // in a drag-drop operation is released at 'pos'
    int dropIndex(const QPoint &pos) const;

    // returns true if the tab to be dropped in a drag-drop operation
    // is the same as the tab at the drop location
    bool proposedDropIsSameTab(const QDropEvent *event) const;

    QLabel *_dropIndicator;
    int _dropIndicatorIndex;
    bool _drawIndicatorDisabled;
    QString _supportedMimeType;
    TabbedViewContainer *_connectedContainer;
    QPoint _dragStart;
    QElapsedTimer *_mousePressTimer;
};
}

#endif  // VIEWCONTAINERTABBAR_H

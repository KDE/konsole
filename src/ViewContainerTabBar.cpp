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

// Own
#include "ViewContainerTabBar.h"
#include "ViewContainer.h"

// Qt
#include <QMimeData>
#include <QLabel>
#include <QPainter>
#include <QDragMoveEvent>
#include <QIcon>
#include <QTabBar>
#include <QApplication>

// KDE
#include <KLocalizedString>

using Konsole::ViewContainerTabBar;
using Konsole::TabbedViewContainer;

ViewContainerTabBar::ViewContainerTabBar(QWidget *parent, TabbedViewContainer *container) :
    QTabBar(parent),
    _dropIndicator(nullptr),
    _dropIndicatorIndex(-1),
    _drawIndicatorDisabled(false),
    _connectedContainer(container)
{
    setDrawBase(true);
    setDocumentMode(true);
    setFocusPolicy(Qt::NoFocus);
    setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    setElideMode(Qt::ElideLeft);

    setAcceptDrops(true);
    setMouseTracking(true);

    _mousePressTimer = new QElapsedTimer();

    setWhatsThis(xi18nc("@info:whatsthis",
                        "<title>Tab Bar</title>"
                        "<para>The tab bar allows you to switch and move tabs. You can double-click a tab to change its name.</para>"));
}

ViewContainerTabBar::~ViewContainerTabBar()
{
    delete _mousePressTimer;
}

void ViewContainerTabBar::mousePressEvent(QMouseEvent *event)
{
    _mousePressTimer->start();

    if (event->buttons() == Qt::LeftButton) {
        _dragStart = event->pos();
    }
    QTabBar::mousePressEvent(event);
}

void ViewContainerTabBar::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() == Qt::LeftButton) {
        QPoint dragPos = _dragStart - event->pos();
        if (dragPos.manhattanLength() > QApplication::startDragDistance()
            || _mousePressTimer->elapsed() > QApplication::startDragTime()) {
            int tab = tabAt(_dragStart);
            if (tab != -1) {
                emit initiateDrag(tab);
            }
        }
    }
    QTabBar::mouseMoveEvent(event);
}

void ViewContainerTabBar::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(_supportedMimeType)
        && event->source() != nullptr) {
        event->acceptProposedAction();
    }
}

void ViewContainerTabBar::dragLeaveEvent(QDragLeaveEvent *)
{
    setDropIndicator(-1);
}

void ViewContainerTabBar::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat(_supportedMimeType)
        && event->source() != nullptr) {
        int index = dropIndex(event->pos());
        if (index == -1) {
            index = count();
        }

        setDropIndicator(index, proposedDropIsSameTab(event));

        event->acceptProposedAction();
    }
}

void ViewContainerTabBar::dropEvent(QDropEvent *event)
{
    setDropIndicator(-1);

    if (!event->mimeData()->hasFormat(_supportedMimeType)) {
        event->ignore();
        return;
    }

    if (proposedDropIsSameTab(event)) {
        event->setDropAction(Qt::IgnoreAction);
        event->accept();
        return;
    }

    const int index = dropIndex(event->pos());
    bool success = false;

    ViewContainerTabBar *sourceContainerTabBar
        = qobject_cast<ViewContainerTabBar *>(event->source());

    // check if the moved tab is the last of source view.
    if (sourceContainerTabBar->count() == 1) {
        TabbedViewContainer *sourceTabbedContainer
            = sourceContainerTabBar->connectedTabbedViewContainer();
        emit moveViewRequest(index, event, success, sourceTabbedContainer);
    } else {
        emit moveViewRequest(index, event, success, nullptr);
    }

    if (success) {
        event->accept();
    } else {
        event->ignore();
    }
}

TabbedViewContainer *ViewContainerTabBar::connectedTabbedViewContainer()
{
    return _connectedContainer;
}

void ViewContainerTabBar::setDropIndicator(int index, bool drawDisabled)
{
    if ((parentWidget() == nullptr) || _dropIndicatorIndex == index) {
        return;
    }

    _dropIndicatorIndex = index;
    const int ARROW_SIZE = 32;
    const bool north = shape() == QTabBar::RoundedNorth || shape() == QTabBar::TriangularNorth;

    if ((_dropIndicator == nullptr) || _drawIndicatorDisabled != drawDisabled) {
        if (_dropIndicator == nullptr) {
            _dropIndicator = new QLabel(parentWidget());
            _dropIndicator->resize(ARROW_SIZE, ARROW_SIZE);
        }

        QIcon::Mode drawMode = drawDisabled ? QIcon::Disabled : QIcon::Normal;
        const QString iconName = north ? QStringLiteral("arrow-up") : QStringLiteral("arrow-down");
        _dropIndicator->setPixmap(QIcon::fromTheme(iconName).pixmap(ARROW_SIZE, ARROW_SIZE,
                                                                    drawMode));
        _drawIndicatorDisabled = drawDisabled;
    }

    if (index < 0) {
        _dropIndicator->hide();
        return;
    }

    const QRect rect = tabRect(index < count() ? index : index - 1);

    QPoint pos;
    if (index < count()) {
        pos = rect.topLeft();
    } else {
        pos = rect.topRight();
    }

    if (north) {
        pos.ry() += ARROW_SIZE;
    } else {
        pos.ry() -= ARROW_SIZE;
    }

    pos.rx() -= ARROW_SIZE / 2;

    _dropIndicator->move(mapTo(parentWidget(), pos));
    _dropIndicator->show();
}

void ViewContainerTabBar::setSupportedMimeType(const QString &mimeType)
{
    _supportedMimeType = mimeType;
}

int ViewContainerTabBar::dropIndex(const QPoint &pos) const
{
    int tab = tabAt(pos);
    if (tab < 0) {
        return tab;
    }

    // pick the closest tab boundary
    QRect rect = tabRect(tab);
    if ((pos.x() - rect.left()) > (rect.width() / 2)) {
        tab++;
    }

    if (tab == count()) {
        return -1;
    }

    return tab;
}

bool ViewContainerTabBar::proposedDropIsSameTab(const QDropEvent *event) const
{
    const bool sameTabBar = event->source() == this;
    if (!sameTabBar) {
        return false;
    }

    const int index = dropIndex(event->pos());
    int sourceIndex = -1;
    emit querySourceIndex(event, sourceIndex);

    const bool sourceAndDropAreLast = sourceIndex == count() - 1 && index == -1;
    return sourceIndex == index || sourceIndex == index - 1 || sourceAndDropAreLast;
}

QPixmap ViewContainerTabBar::dragDropPixmap(int tab)
{
    Q_ASSERT(tab >= 0 && tab < count());

    // QWidget::grab() (depreciated QPixmap::grabWidget())
    // does not show right and bottom edges on certain themes
    // (curved corners) so increase size of rectangle.
    const QRect rect = tabRect(tab);
    const int borderWidth = 1;

    QPixmap tabPixmap(rect.width() + borderWidth,
                      rect.height() + borderWidth);
    QPainter painter(&tabPixmap);
    painter.drawPixmap(0, 0, this->grab(rect));
    QPen borderPen;
    borderPen.setBrush(palette().dark());
    borderPen.setWidth(borderWidth);
    painter.setPen(borderPen);
    painter.drawRect(0, 0, rect.width(), rect.height());
    painter.end();

    return tabPixmap;
}

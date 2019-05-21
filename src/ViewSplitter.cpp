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
#include "ViewSplitter.h"

// Qt
#include <QDebug>
#include <QChildEvent>
#include <QScrollBar>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QPaintEvent>
#include <QMimeData>
#include <QDebug>
#include <QCursor>
#include <QPainter>

// Konsole
#include "ViewContainer.h"
#include "TerminalDisplay.h"

using Konsole::ViewSplitter;
using Konsole::TerminalDisplay;

//TODO: Connect the TerminalDisplay destroyed signal here.

ViewSplitter::ViewSplitter(QWidget *parent) :
    QSplitter(parent)
{
    setAcceptDrops(true);
}

void ViewSplitter::adjustActiveTerminalDisplaySize(int percentage)
{
    const int containerIndex = indexOf(activeTerminalDisplay());
    Q_ASSERT(containerIndex != -1);

    QList<int> containerSizes = sizes();

    const int oldSize = containerSizes[containerIndex];
    const int newSize = static_cast<int>(oldSize * (1.0 + percentage / 100.0));
    const int perContainerDelta = (count() == 1) ? 0 : ((newSize - oldSize) / (count() - 1)) * (-1);

    for (int& size : containerSizes) {
        size += perContainerDelta;
    }
    containerSizes[containerIndex] = newSize;

    setSizes(containerSizes);
}

// Get the first splitter that's a parent of the current focused widget.
ViewSplitter *ViewSplitter::activeSplitter()
{
    QWidget *widget = focusWidget() != nullptr ? focusWidget() : this;

    ViewSplitter *splitter = nullptr;

    while ((splitter == nullptr) && (widget != nullptr)) {
        splitter = qobject_cast<ViewSplitter *>(widget);
        widget = widget->parentWidget();
    }

    Q_ASSERT(splitter);
    return splitter;
}

void ViewSplitter::updateSizes()
{
    const int space = (orientation() == Qt::Horizontal ? width() : height()) / count();
    setSizes(QVector<int>(count(), space).toList());
}

void ViewSplitter::addTerminalDisplay(TerminalDisplay *terminalDisplay, Qt::Orientation containerOrientation)
{
    ViewSplitter *splitter = activeSplitter();
    if (splitter->count() < 2) {
        splitter->addWidget(terminalDisplay);
        splitter->setOrientation(containerOrientation);
    } else if (containerOrientation == splitter->orientation()) {
        auto activeDisplay = splitter->activeTerminalDisplay();
        if (!activeDisplay) {
            splitter->addWidget(terminalDisplay);
        } else {
            const int currentIndex = splitter->indexOf(activeDisplay);
            splitter->insertWidget(currentIndex, terminalDisplay);
        }
    } else {
        auto newSplitter = new ViewSplitter();

        TerminalDisplay *oldTerminalDisplay = splitter->activeTerminalDisplay();
        const int oldContainerIndex = splitter->indexOf(oldTerminalDisplay);
        newSplitter->addWidget(oldTerminalDisplay);
        newSplitter->addWidget(terminalDisplay);
        newSplitter->setOrientation(containerOrientation);
        newSplitter->updateSizes();
        newSplitter->show();

        splitter->insertWidget(oldContainerIndex, newSplitter);
    }
    splitter->updateSizes();
}

void ViewSplitter::childEvent(QChildEvent *event)
{
    QSplitter::childEvent(event);

    if (event->removed()) {
        if (count() == 0) {
            deleteLater();
        }
        if (!findChild<TerminalDisplay*>()) {
            deleteLater();
        }
    }

    auto terminals = getToplevelSplitter()->findChildren<TerminalDisplay*>();
    if (terminals.size() == 1) {
        terminals.at(0)->headerBar()->setVisible(false);
    } else {
        for(auto terminal : terminals) {
            terminal->headerBar()->setVisible(true);
        }
    }
}

void ViewSplitter::handleFocusDirection(Qt::Orientation orientation, int direction)
{
    auto terminalDisplay = activeTerminalDisplay();
    auto parentSplitter = qobject_cast<ViewSplitter*>(terminalDisplay->parentWidget());
    auto topSplitter = parentSplitter->getToplevelSplitter();

    const auto handleWidth = parentSplitter->handleWidth() <= 1 ? 4 : parentSplitter->handleWidth();

    const auto start = QPoint(terminalDisplay->x(), terminalDisplay->y());
    const auto startMapped = parentSplitter->mapTo(topSplitter, start);

    const int newX = orientation != Qt::Horizontal ? startMapped.x() + handleWidth
             : direction == 1 ? startMapped.x() + terminalDisplay->width() + handleWidth
             : startMapped.x() - handleWidth;

    const int newY = orientation != Qt::Vertical ? startMapped.y() + handleWidth
                    : direction == 1 ? startMapped.y() + terminalDisplay->height() + handleWidth
                    : startMapped.y() - handleWidth;

    const auto newPoint = QPoint(newX, newY);
    auto child = topSplitter->childAt(newPoint);

    if (TerminalDisplay* terminal = qobject_cast<TerminalDisplay*>(child)) {
        terminal->setFocus(Qt::OtherFocusReason);
    } else if (qobject_cast<QSplitterHandle*>(child)) {
        auto targetSplitter = qobject_cast<QSplitter*>(child->parent());
        auto splitterTerminal = qobject_cast<TerminalDisplay*>(targetSplitter->widget(0));
        splitterTerminal->setFocus(Qt::OtherFocusReason);
    } else if (qobject_cast<QWidget*>(child)) {
        TerminalDisplay *terminalParent = nullptr;
        while(!terminalParent) {
            terminalParent = qobject_cast<TerminalDisplay*>(child->parentWidget());
            child = child->parentWidget();
        }
        terminalParent->setFocus(Qt::OtherFocusReason);
    }
}

void ViewSplitter::focusUp()
{
    handleFocusDirection(Qt::Vertical, -1);
}

void ViewSplitter::focusDown()
{
    handleFocusDirection(Qt::Vertical, +1);
}

void ViewSplitter::focusLeft()
{
    handleFocusDirection(Qt::Horizontal, -1);
}

void ViewSplitter::focusRight()
{
    handleFocusDirection(Qt::Horizontal, +1);
}

TerminalDisplay *ViewSplitter::activeTerminalDisplay() const
{
    auto focusedWidget = qobject_cast<TerminalDisplay*>(focusWidget());
    return focusedWidget ? focusedWidget : findChild<TerminalDisplay*>();
}

void ViewSplitter::maximizeCurrentTerminal()
{
    handleMinimizeMaximize(true);
}

void ViewSplitter::restoreOtherTerminals()
{
    handleMinimizeMaximize(false);
}

void ViewSplitter::handleMinimizeMaximize(bool maximize)
{
    auto viewSplitter = getToplevelSplitter();
    auto terminalDisplays = viewSplitter->findChildren<TerminalDisplay*>();
    auto currentActiveTerminal = viewSplitter->activeTerminalDisplay();
    auto method = maximize ? &QWidget::hide : &QWidget::show;
    for(auto terminal : terminalDisplays) {
        if (Q_LIKELY(currentActiveTerminal != terminal)) {
            (terminal->*method)();
        }
    }
}

ViewSplitter *ViewSplitter::getToplevelSplitter()
{
    ViewSplitter *current = this;
    while(qobject_cast<ViewSplitter*>(current->parentWidget())) {
        current = qobject_cast<ViewSplitter*>(current->parentWidget());
    }
    return current;
}

/* this variable is shared across all splitter instances */
namespace  {
    TerminalDisplay *lastDragged = nullptr;
}

void ViewSplitter::dragEnterEvent(QDragEnterEvent *ev)
{
    auto mime = ev->mimeData();
    qDebug() << mime;
    if (mime->hasFormat(QStringLiteral("konsole/terminal_display"))) {
        auto terminalWidget = qobject_cast<TerminalDisplay*>(ev->source()->parent());
        if (!terminalWidget) {
            ev->ignore();
        } else if (terminalWidget->rect().contains(terminalWidget->mapFromGlobal(QCursor::pos()))) {
            ev->ignore();
        } else {
            qDebug() << terminalWidget->geometry() << terminalWidget->mapFromGlobal(QCursor::pos());
            ev->accept();
        }
    }
    QSplitter::dragEnterEvent(ev);
}

void ViewSplitter::dragMoveEvent(QDragMoveEvent *ev)
{
    auto mime = ev->mimeData();
    if (mime->hasFormat(QStringLiteral("konsole/terminal_display"))) {
        ev->accept();
        auto childWidget = childAt(mapFromGlobal(QCursor::pos()));
        TerminalDisplay *displayAt = qobject_cast<TerminalDisplay*>(childWidget);
        while(displayAt == nullptr) {
            if (!childWidget->parent()) {
                break;
            }
            childWidget = childWidget->parentWidget();
            displayAt = qobject_cast<TerminalDisplay*>(childWidget);
        }

        if (displayAt) {
            if (lastDragged != displayAt) {
                if (lastDragged) {
                    lastDragged->disableDropOverlay();
                }
                lastDragged = displayAt;
            }
            lastDragged->enableDropOverlay();
        } else {
            if (lastDragged) {
                lastDragged->disableDropOverlay();
                lastDragged = nullptr;
            }
        }
    }
    QSplitter::dragMoveEvent(ev);
}

void ViewSplitter::dragLeaveEvent(QDragLeaveEvent *ev)
{
    Q_UNUSED(ev)
    update();
}

void ViewSplitter::dropEvent(QDropEvent *ev)
{
    Q_UNUSED(ev)
    if (!lastDragged) {
        return;
    }
    lastDragged->disableDropOverlay();

    const auto width = lastDragged->width();
    const auto height = lastDragged->height();
    const auto midPoint = QPoint(width/2, height / 2);
    const auto topTriangle = QPolygon({{0,0}, midPoint, {width, 0}, {0,0}});
    const auto leftTriangle = QPolygon({{0,0}, midPoint, {0, height}, {0,0}});
    const auto bottomTriangle = QPolygon({{0, height}, midPoint, {width, height}, {0, height}});
    const auto rightTriangle = QPolygon({{width, 0}, midPoint, {width, height}, {width, 0}});

    const auto mousePos = mapFromGlobal(QCursor::pos());
    const auto addLocation = (topTriangle.contains(mousePos)) ? Qt::TopEdge
                           : (leftTriangle.contains(mousePos)) ? Qt::LeftEdge
                           : (bottomTriangle.contains(mousePos)) ? Qt::BottomEdge
                           : (rightTriangle.contains(mousePos)) ? Qt::RightEdge
                           : /* in case of error */ Qt::Edge::RightEdge;

    auto *movingTerminal = qobject_cast<TerminalDisplay*>(ev->source()->parent());
    ev->acceptProposedAction();
    ev->accept();

    movingTerminal->hide();
    movingTerminal->setParent(nullptr);

    //TODO: Still need to do the 'add before' or 'add after'. right now is just 'after'.
    addTerminalDisplay(movingTerminal, addLocation == Qt::TopEdge || addLocation == Qt::BottomEdge ?
                           Qt::Orientation::Vertical : Qt::Orientation::Horizontal);

    movingTerminal->show();
    lastDragged = nullptr;
}

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

// Konsole
#include "ViewContainer.h"
#include "TerminalDisplay.h"

using Konsole::ViewSplitter;
using Konsole::TerminalDisplay;

//TODO: Connect the TerminalDisplay destroyed signal here.

ViewSplitter::ViewSplitter(QWidget *parent) :
    QSplitter(parent)
{
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
    } else if (qobject_cast<QScrollBar*>(child)) {
        auto scrollbarTerminal = qobject_cast<TerminalDisplay*>(child->parent());
        scrollbarTerminal->setFocus(Qt::OtherFocusReason);
    } else if (qobject_cast<QSplitterHandle*>(child)) {
        auto targetSplitter = qobject_cast<QSplitter*>(child->parent());
        auto splitterTerminal = qobject_cast<TerminalDisplay*>(targetSplitter->widget(0));
        splitterTerminal->setFocus(Qt::OtherFocusReason);
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
    auto splitters = viewSplitter->findChildren<ViewSplitter*>();

    if (maximize) {
        for(auto splitter : splitters) {
            bool collapseSplitter = true;
            for (auto child : splitter->findChildren<TerminalDisplay*>()) {
                if (child == currentActiveTerminal) {
                    collapseSplitter = false;
                    break;
                }
            }
            if (collapseSplitter) {
                splitter->setVisible(false);
            } else {
                for (int i = 0, end = splitter->count(); i < end; i++) {
                    QWidget *widgetAt = splitter->widget(i);
                    if (splitter->widget(i) != currentActiveTerminal && widgetAt->isVisible()) {
                        splitter->setVisible(false);
                    }
                }
            }
        }
    } else {
        for (auto splitter : splitters) {
            splitter->setVisible(true);
        }
        for (auto terminalDisplay : terminalDisplays) {
            terminalDisplay->setVisible(true);
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

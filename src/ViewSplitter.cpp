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

void ViewSplitter::adjustTerminalDisplaySize(TerminalDisplay *container, int percentage)
{
    int containerIndex = indexOf(container);

    Q_ASSERT(containerIndex != -1);

    QList<int> containerSizes = sizes();

    const int oldSize = containerSizes[containerIndex];
    const auto newSize = static_cast<int>(oldSize * (1.0 + percentage / 100.0));

    const int perContainerDelta = (count() == 1) ? 0 : ((newSize - oldSize) / (count() - 1)) * (-1);

    for (int i = 0; i < containerSizes.count(); i++) {
        if (i == containerIndex) {
            containerSizes[i] = newSize;
        } else {
            containerSizes[i] = containerSizes[i] + perContainerDelta;
        }
    }

    setSizes(containerSizes);
}

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
    int space;

    if (orientation() == Qt::Horizontal) {
        space = width() / count();
    } else {
        space = height() / count();
    }

    QList<int> widgetSizes;
    const int widgetCount = count();
    widgetSizes.reserve(widgetCount);
    for (int i = 0; i < widgetCount; i++) {
        widgetSizes << space;
    }

    setSizes(widgetSizes);
}

void ViewSplitter::addTerminalDisplay(TerminalDisplay *terminalDisplay, Qt::Orientation containerOrientation)
{
    ViewSplitter *splitter = activeSplitter();

    if (splitter->count() < 2
        || containerOrientation == splitter->orientation()) {
        splitter->addWidget(terminalDisplay);

        if (splitter->orientation() != containerOrientation) {
            splitter->setOrientation(containerOrientation);
        }
        connect(terminalDisplay, &QObject::destroyed, this, &ViewSplitter::childDestroyed);
        splitter->updateSizes();
    } else {
        auto newSplitter = new ViewSplitter();
        connect(newSplitter, &QObject::destroyed, this, &ViewSplitter::childDestroyed);
        connect(terminalDisplay, &QObject::destroyed, newSplitter, &ViewSplitter::childDestroyed);

        TerminalDisplay *oldTerminalDisplay = splitter->activeTerminalDisplay();
        disconnect(oldTerminalDisplay, &QObject::destroyed, nullptr, nullptr);
        connect(oldTerminalDisplay, &QObject::destroyed, newSplitter, &ViewSplitter::childDestroyed);

        const int oldContainerIndex = splitter->indexOf(oldTerminalDisplay);
        newSplitter->addWidget(oldTerminalDisplay);
        newSplitter->addWidget(terminalDisplay);
        newSplitter->setOrientation(containerOrientation);
        newSplitter->updateSizes();
        newSplitter->show();

        splitter->insertWidget(oldContainerIndex, newSplitter);
    }
}

void ViewSplitter::childDestroyed(QObject *childWidget)
{
    // remove the parent so count() has the correct value.
    childWidget->setParent(nullptr);
    if (count() == 0) {
        deleteLater();
    }
}

void ViewSplitter::handleFocusDirection(Qt::Orientation orientation, int direction)
{
    auto terminalDisplay = activeTerminalDisplay();
    auto activeViewSplitter = qobject_cast<ViewSplitter*>(terminalDisplay->parentWidget());
    auto idx = activeViewSplitter->indexOf(terminalDisplay);

    // Easy, we are in a vertical spliter and we want to focus up.
    if (activeViewSplitter->orientation() == orientation &&
            ((direction < 0 && idx > 0) || (direction > 0 && idx < activeViewSplitter->count() - 1))) {
        auto nextPossibleTerminal = qobject_cast<TerminalDisplay*>(activeViewSplitter->widget(idx + direction));
        if (nextPossibleTerminal) {
            nextPossibleTerminal->setFocus(Qt::OtherFocusReason);
            return;
        }

        // element is not a terminal display but a splitter with perhaps more splitters and terminals.
        // choose one element and focus it.
        auto nextPossibleSplitter = qobject_cast<ViewSplitter*>(activeViewSplitter->widget(idx + direction));
        if (nextPossibleSplitter) {
            nextPossibleTerminal = nextPossibleSplitter->findChild<TerminalDisplay*>();
            if (nextPossibleTerminal) {
                nextPossibleTerminal->setFocus(Qt::OtherFocusReason);
            }
            return;
        }
    }

    // Hard, we are in a horizontal splitter or we are in the top of the vertical splitter.
    else {
        ViewSplitter *parentTerminalWidget = nullptr;
        ViewSplitter *oldParent = activeViewSplitter;
        do {
            parentTerminalWidget = qobject_cast<ViewSplitter*>(
                        parentTerminalWidget ? parentTerminalWidget->parentWidget()
                                             : activeViewSplitter->parentWidget());

            if (parentTerminalWidget) {
                idx = parentTerminalWidget->indexOf(oldParent);
            }
            oldParent = parentTerminalWidget;
        } while (parentTerminalWidget && parentTerminalWidget->orientation() != orientation);

        if (!parentTerminalWidget) {
            return;
        }

        if ((direction < 0 && idx > 0) || (direction > 0 && idx < parentTerminalWidget->count() - 1)) {
            parentTerminalWidget->widget(idx + direction)->setFocus(Qt::OtherFocusReason);
        }
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

void ViewSplitter::activateNextTerminalDisplay()
{
    TerminalDisplay *active = activeTerminalDisplay();

    int index = indexOf(active);

    if (index == -1) {
        return;
    }

    if (index == count() - 1) {
        index = 0;
    } else {
        index++;
    }
    widget(index)->setFocus(Qt::OtherFocusReason);
}

void ViewSplitter::activatePreviousTerminalDisplay()
{
    TerminalDisplay *active = activeTerminalDisplay();

    int index = indexOf(active);

    if (index == 0) {
        index = count() - 1;
    } else {
        index--;
    }
    widget(index)->setFocus(Qt::OtherFocusReason);
}

TerminalDisplay *ViewSplitter::activeTerminalDisplay() const
{
    auto focusedWidget = qobject_cast<TerminalDisplay*>(focusWidget());
    return focusedWidget ? focusedWidget : findChild<TerminalDisplay*>();
}

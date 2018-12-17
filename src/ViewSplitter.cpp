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

void ViewSplitter::registerTerminalDisplay(TerminalDisplay *container)
{
    _terminalDisplays.append(container);
}

void ViewSplitter::unregisterTerminalDisplay(TerminalDisplay *container)
{
    _terminalDisplays.removeOne(container);
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

void ViewSplitter::removeTerminalDisplay(TerminalDisplay *terminalDisplay)
{
    Q_ASSERT(terminalDisplays().contains(terminalDisplay));
    unregisterTerminalDisplay(terminalDisplay);
}

void ViewSplitter::addTerminalDisplay(TerminalDisplay *terminalDisplay, Qt::Orientation containerOrientation)
{
    ViewSplitter *splitter = activeSplitter();
    connect(terminalDisplay, &QWidget::destroyed, this, &ViewSplitter::terminalDisplayDestroyed);

    if (splitter->count() < 2
        || containerOrientation == splitter->orientation()) {
        splitter->registerTerminalDisplay(terminalDisplay);
        splitter->addWidget(terminalDisplay);

        if (splitter->orientation() != containerOrientation) {
            splitter->setOrientation(containerOrientation);
        }

        splitter->updateSizes();
    } else {
        auto newSplitter = new ViewSplitter();

        TerminalDisplay *oldTerminalDisplay = splitter->activeTerminalDisplay();

        const int oldContainerIndex = splitter->indexOf(oldTerminalDisplay);

        splitter->unregisterTerminalDisplay(oldTerminalDisplay);

        newSplitter->registerTerminalDisplay(oldTerminalDisplay);
        newSplitter->registerTerminalDisplay(terminalDisplay);

        newSplitter->addWidget(oldTerminalDisplay);
        newSplitter->addWidget(terminalDisplay);
        newSplitter->setOrientation(containerOrientation);
        newSplitter->updateSizes();
        newSplitter->show();

        splitter->insertWidget(oldContainerIndex, newSplitter);
    }
}

void ViewSplitter::terminalDisplayDestroyed(QObject *terminalDisplay)
{
    terminalDisplay->setParent(nullptr);
    if (count() == 0) {
        deleteLater();
    }
}

//TODO: Maybe move this to the TabbedViewContainer ?
#if 0
void ViewSplitter::containerEmpty(TerminalDisplay * myContainer)
{
    _containers.removeAll(myContainer);
    if (count() == 0) {
        emit empty(this);
    }

    int children = 0;
    foreach (auto container, _containers) {
        children += container->count();
    }

    if (children == 0) {
        emit allContainersEmpty();
    }

    // This container is no more, try to find another container to focus.
    ViewSplitter *currentSplitter = activeSplitter();
    while(qobject_cast<ViewSplitter*>(currentSplitter->parent())) {
        currentSplitter = qobject_cast<ViewSplitter*>(currentSplitter->parent());
    }

    for(auto tabWidget : currentSplitter->findChildren<TerminalDisplay*>()) {
        if (tabWidget != myContainer && tabWidget->count()) {
            tabWidget->setCurrentIndex(0);
        }
    }
}
#endif

void ViewSplitter::activateNextTerminalDisplay()
{
    TerminalDisplay *active = activeTerminalDisplay();

    int index = _terminalDisplays.indexOf(active);

    if (index == -1) {
        return;
    }

    if (index == _terminalDisplays.count() - 1) {
        index = 0;
    } else {
        index++;
    }

    setActiveTerminalDisplay(_terminalDisplays.at(index));
}

void ViewSplitter::activatePreviousTerminalDisplay()
{
    TerminalDisplay *active = activeTerminalDisplay();

    int index = _terminalDisplays.indexOf(active);

    if (index == 0) {
        index = _terminalDisplays.count() - 1;
    } else {
        index--;
    }

    setActiveTerminalDisplay(_terminalDisplays.at(index));
}

void ViewSplitter::setActiveTerminalDisplay(TerminalDisplay *terminalDisplay)
{
    if (terminalDisplay != nullptr) {
        terminalDisplay->setFocus(Qt::OtherFocusReason);
    }
}

TerminalDisplay *ViewSplitter::activeTerminalDisplay() const
{
    return qobject_cast<TerminalDisplay*>(focusWidget());
}

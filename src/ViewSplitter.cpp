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

using Konsole::ViewSplitter;
using Konsole::TabbedViewContainer;

ViewSplitter::ViewSplitter(QWidget *parent) :
    QSplitter(parent),
    _containers(QList<TabbedViewContainer *>()),
    _recursiveSplitting(true)
{
}

void ViewSplitter::childEmpty(ViewSplitter *splitter)
{
    delete splitter;

    if (count() == 0) {
        emit empty(this);
    }
}

void ViewSplitter::adjustContainerSize(TabbedViewContainer *container, int percentage)
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

void ViewSplitter::registerContainer(TabbedViewContainer *container)
{
    _containers << container;
    connect(container, SIGNAL(empty(TabbedViewContainer*)), this, SLOT(containerEmpty(TabbedViewContainer*)));
}

void ViewSplitter::unregisterContainer(TabbedViewContainer *container)
{
    _containers.removeAll(container);
    disconnect(container, nullptr, this, nullptr);
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

void ViewSplitter::setRecursiveSplitting(bool recursive)
{
    _recursiveSplitting = recursive;
}

bool ViewSplitter::recursiveSplitting() const
{
    return _recursiveSplitting;
}

void ViewSplitter::removeContainer(TabbedViewContainer *container)
{
    Q_ASSERT(containers().contains(container));

    unregisterContainer(container);
}

void ViewSplitter::addContainer(TabbedViewContainer *container, Qt::Orientation containerOrientation)
{
    ViewSplitter *splitter = activeSplitter();

    if (splitter->count() < 2
        || containerOrientation == splitter->orientation()
        || !_recursiveSplitting) {
        splitter->registerContainer(container);
        splitter->addWidget(container);

        if (splitter->orientation() != containerOrientation) {
            splitter->setOrientation(containerOrientation);
        }

        splitter->updateSizes();
    } else {
        auto newSplitter = new ViewSplitter(this);
        connect(newSplitter, &Konsole::ViewSplitter::empty, splitter,
                &Konsole::ViewSplitter::childEmpty);

        TabbedViewContainer *oldContainer = splitter->activeContainer();
        const int oldContainerIndex = splitter->indexOf(oldContainer);

        splitter->unregisterContainer(oldContainer);

        newSplitter->registerContainer(oldContainer);
        newSplitter->registerContainer(container);

        newSplitter->addWidget(oldContainer);
        newSplitter->addWidget(container);
        newSplitter->setOrientation(containerOrientation);
        newSplitter->updateSizes();
        newSplitter->show();

        splitter->insertWidget(oldContainerIndex, newSplitter);
    }
}

void ViewSplitter::containerEmpty(TabbedViewContainer * myContainer)
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
}

void ViewSplitter::activateNextContainer()
{
    TabbedViewContainer *active = activeContainer();

    int index = _containers.indexOf(active);

    if (index == -1) {
        return;
    }

    if (index == _containers.count() - 1) {
        index = 0;
    } else {
        index++;
    }

    setActiveContainer(_containers.at(index));
}

void ViewSplitter::activatePreviousContainer()
{
    TabbedViewContainer *active = activeContainer();

    int index = _containers.indexOf(active);

    if (index == 0) {
        index = _containers.count() - 1;
    } else {
        index--;
    }

    setActiveContainer(_containers.at(index));
}

void ViewSplitter::setActiveContainer(TabbedViewContainer *container)
{
    QWidget *activeView = container->currentWidget();

    if (activeView != nullptr) {
        activeView->setFocus(Qt::OtherFocusReason);
    }
}

TabbedViewContainer *ViewSplitter::activeContainer() const
{
    if (QWidget *focusW = focusWidget()) {
        TabbedViewContainer *focusContainer = nullptr;

        while (focusW != nullptr) {
            foreach (TabbedViewContainer *container, _containers) {
                if (container == focusW) {
                    focusContainer = container;
                    break;
                }
            }
            focusW = focusW->parentWidget();
        }

        if (focusContainer != nullptr) {
            return focusContainer;
        }
    }

    QList<ViewSplitter *> splitters = findChildren<ViewSplitter *>();

    if (splitters.count() > 0) {
        return splitters.last()->activeContainer();
    } else {
        if (_containers.count() > 0) {
            return _containers.last();
        } else {
            return nullptr;
        }
    }
}

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

// Konsole
#include "ViewContainer.h"

using Konsole::ViewSplitter;
using Konsole::ViewContainer;

ViewSplitter::ViewSplitter(QWidget* parent)
    : QSplitter(parent)
    , _recursiveSplitting(true)
{
}

void ViewSplitter::childEmpty(ViewSplitter* splitter)
{
    delete splitter;

    if (count() == 0)
        emit empty(this);
}

void ViewSplitter::adjustContainerSize(ViewContainer* container , int percentage)
{
    int containerIndex = indexOf(container->containerWidget());

    Q_ASSERT(containerIndex != -1);

    QList<int> containerSizes = sizes();

    const int oldSize = containerSizes[containerIndex];
    const int newSize = static_cast<int>(oldSize * (1.0 + percentage / 100.0));

    const int perContainerDelta = (count() == 1) ? 0 : ((newSize - oldSize) / (count() - 1)) * (-1);

    for (int i = 0 ; i < containerSizes.count() ; i++) {
        if (i == containerIndex)
            containerSizes[i] = newSize;
        else
            containerSizes[i] = containerSizes[i] + perContainerDelta;
    }

    setSizes(containerSizes);
}

ViewSplitter* ViewSplitter::activeSplitter()
{
    QWidget* widget = focusWidget() ? focusWidget() : this;

    ViewSplitter* splitter = 0;

    while (!splitter && widget) {
        splitter = qobject_cast<ViewSplitter*>(widget);
        widget = widget->parentWidget();
    }

    Q_ASSERT(splitter);
    return splitter;
}

void ViewSplitter::registerContainer(ViewContainer* container)
{
    _containers << container;
    // Connecting to container::destroyed() using the new-style connection
    // syntax causes a crash at exit. I don't know why. Using the old-style
    // syntax works.
    //connect(container , static_cast<void(ViewContainer::*)(ViewContainer*)>(&Konsole::ViewContainer::destroyed) , this , &Konsole::ViewSplitter::containerDestroyed);
    //connect(container , &Konsole::ViewContainer::empty , this , &Konsole::ViewSplitter::containerEmpty);
    connect(container , SIGNAL(destroyed(ViewContainer*)) , this , SLOT(containerDestroyed(ViewContainer*)));
    connect(container , SIGNAL(empty(ViewContainer*)) , this , SLOT(containerEmpty(ViewContainer*)));
}

void ViewSplitter::unregisterContainer(ViewContainer* container)
{
    _containers.removeAll(container);
    disconnect(container , 0 , this , 0);
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
    for (int i = 0; i < count(); i++)
        widgetSizes << space;

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

void ViewSplitter::removeContainer(ViewContainer* container)
{
    Q_ASSERT(containers().contains(container));

    unregisterContainer(container);
}

void ViewSplitter::addContainer(ViewContainer* container ,
                                Qt::Orientation containerOrientation)
{
    ViewSplitter* splitter = activeSplitter();

    if (splitter->count() < 2 ||
            containerOrientation == splitter->orientation() ||
            !_recursiveSplitting) {
        splitter->registerContainer(container);
        splitter->addWidget(container->containerWidget());

        if (splitter->orientation() != containerOrientation)
            splitter->setOrientation(containerOrientation);

        splitter->updateSizes();
    } else {
        ViewSplitter* newSplitter = new ViewSplitter(this);
        connect(newSplitter , &Konsole::ViewSplitter::empty , splitter , &Konsole::ViewSplitter::childEmpty);

        ViewContainer* oldContainer = splitter->activeContainer();
        const int oldContainerIndex = splitter->indexOf(oldContainer->containerWidget());

        splitter->unregisterContainer(oldContainer);

        newSplitter->registerContainer(oldContainer);
        newSplitter->registerContainer(container);

        newSplitter->addWidget(oldContainer->containerWidget());
        newSplitter->addWidget(container->containerWidget());
        newSplitter->setOrientation(containerOrientation);
        newSplitter->updateSizes();
        newSplitter->show();

        splitter->insertWidget(oldContainerIndex, newSplitter);
    }
}

void ViewSplitter::containerEmpty(ViewContainer* /*container*/)
{
    int children = 0;
    foreach(ViewContainer* container, _containers) {
        children += container->views().count();
    }

    if (children == 0)
        emit allContainersEmpty();
}

void ViewSplitter::containerDestroyed(ViewContainer* container)
{
    Q_ASSERT(_containers.contains(container));

    _containers.removeAll(container);

    if (count() == 0) {
        emit empty(this);
    }
}

void ViewSplitter::activateNextContainer()
{
    ViewContainer* active = activeContainer();

    int index = _containers.indexOf(active);

    if (index == -1)
        return;

    if (index == _containers.count() - 1)
        index = 0;
    else
        index++;

    setActiveContainer(_containers.at(index));
}

void ViewSplitter::activatePreviousContainer()
{
    ViewContainer* active = activeContainer();

    int index = _containers.indexOf(active);

    if (index == 0)
        index = _containers.count() - 1;
    else
        index--;

    setActiveContainer(_containers.at(index));
}

void ViewSplitter::setActiveContainer(ViewContainer* container)
{
    QWidget* activeView = container->activeView();

    if (activeView)
        activeView->setFocus(Qt::OtherFocusReason);
}

ViewContainer* ViewSplitter::activeContainer() const
{
    if (QWidget* focusW = focusWidget()) {
        ViewContainer* focusContainer = 0;

        while (focusW != 0) {
            foreach(ViewContainer* container, _containers) {
                if (container->containerWidget() == focusW) {
                    focusContainer = container;
                    break;
                }
            }
            focusW = focusW->parentWidget();
        }

        if (focusContainer)
            return focusContainer;
    }

    QList<ViewSplitter*> splitters = findChildren<ViewSplitter*>();

    if (splitters.count() > 0) {
        return splitters.last()->activeContainer();
    } else {
        if (_containers.count() > 0)
            return _containers.last();
        else
            return 0;
    }
}


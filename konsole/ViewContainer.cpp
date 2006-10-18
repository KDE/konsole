/*
    This file is part of the Konsole Terminal.
    
    Copyright (C) 2006 Robert Knight <robertknight@gmail.com>

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

// Qt
#include <QLineEdit>
#include <QTabWidget>

// Konsole
#include "ViewContainer.h"

void ViewContainer::addView(QWidget* view)
{
    _views << view;
    viewAdded(view);
}
void ViewContainer::removeView(QWidget* view)
{
    _views.removeAll(view);
    viewRemoved(view);
}

const QList<QWidget*> ViewContainer::views()
{
    return _views;
}

int TabbedViewContainer::debug = 0;

TabbedViewContainer::TabbedViewContainer()
{
    _tabWidget = new QTabWidget();
}

TabbedViewContainer::~TabbedViewContainer()
{
    delete _tabWidget;
}

void TabbedViewContainer::viewAdded( QWidget* view )
{
    _tabWidget->addTab( view , "Tab " + QString::number(++debug) );
}
void TabbedViewContainer::viewRemoved( QWidget* view )
{
    Q_ASSERT( _tabWidget->indexOf(view) != -1 );

    _tabWidget->removeTab( _tabWidget->indexOf(view) );
}
QWidget* TabbedViewContainer::containerWidget() const
{
    return _tabWidget;
}

QWidget* TabbedViewContainer::activeView() const
{
    return _tabWidget->widget(_tabWidget->currentIndex());
}

#include "ViewContainer.moc"

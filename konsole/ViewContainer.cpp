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
#include <QHash>
#include <QLineEdit>
#include <QTabWidget>

// Konsole
#include "NavigationItem.h"
#include "ViewContainer.h"

void ViewContainer::addView(QWidget* view , NavigationItem* item)
{
    _views << view;
    _navigation[view] = item;

    viewAdded(view);
}
void ViewContainer::removeView(QWidget* view)
{
    _views.removeAll(view);
    _navigation.remove(view);

    viewRemoved(view);
}

const QList<QWidget*> ViewContainer::views()
{
    return _views;
}

NavigationItem* ViewContainer::navigationItem( QWidget* widget )
{
    Q_ASSERT( _navigation.contains(widget) );

    return _navigation[widget];    
}

QList<QWidget*> ViewContainer::widgetsForItem(NavigationItem* item) const
{
    return _navigation.keys(item);
}

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
    NavigationItem* item = navigationItem(view);
    connect( item , SIGNAL( titleChanged(NavigationItem*) ) , this , SLOT( updateTitle(NavigationItem*) ) );

    _tabWidget->addTab( view , item->icon() , item->title() );
}
void TabbedViewContainer::viewRemoved( QWidget* view )
{
    Q_ASSERT( _tabWidget->indexOf(view) != -1 );

    _tabWidget->removeTab( _tabWidget->indexOf(view) );
}

void TabbedViewContainer::updateTitle(NavigationItem* item) 
{
    QList<QWidget*> items = widgetsForItem(item);
    QListIterator<QWidget*> itemIter(items);

    while ( itemIter.hasNext() )
    {
        int index = _tabWidget->indexOf( itemIter.next() );
        _tabWidget->setTabText( index , item->title() );
    }

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

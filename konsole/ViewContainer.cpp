/*
    This file is part of the Konsole Terminal.
    
    Copyright (C) 2006-2007 Robert Knight <robertknight@gmail.com>

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
#include <QStackedWidget>
#include <QTabWidget>
#include <QWidgetAction>

// KDE 
#include <kcolordialog.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kmenu.h>
#include <kpalette.h>
#include <ktabwidget.h>

// Konsole
#include "ViewContainer.h"
#include "ViewProperties.h"

void ViewContainer::addView(QWidget* view , ViewProperties* item)
{
    _views << view;
    _navigation[view] = item;

    connect( view , SIGNAL(destroyed(QObject*)) , this , SLOT( viewDestroyed(QObject*) ) );

    viewAdded(view);
}
void ViewContainer::viewDestroyed(QObject* object)
{
    QWidget* widget = static_cast<QWidget*>(object);

    _views.removeAll(widget);
    _navigation.remove(widget);

    if (_views.count() == 0)
        emit empty(this);
}
void ViewContainer::removeView(QWidget* view)
{
    _views.removeAll(view);
    _navigation.remove(view);

    viewRemoved(view);

    if (_views.count() == 0)
        emit empty(this);
}

const QList<QWidget*> ViewContainer::views()
{
    return _views;
}

ViewProperties* ViewContainer::viewProperties( QWidget* widget )
{
    Q_ASSERT( _navigation.contains(widget) );

    return _navigation[widget];    
}

QList<QWidget*> ViewContainer::widgetsForItem(ViewProperties* item) const
{
    return _navigation.keys(item);
}

TabbedViewContainer::TabbedViewContainer(QObject* parent) :
    ViewContainer(parent)  
   ,_tabContextMenu(0) 
   ,_tabSelectColorMenu(0)
   ,_tabColorSelector(0)
   ,_tabColorCells(0)
   ,_contextMenuTab(0) 
{
    _tabWidget = new KTabWidget();
    _tabContextMenu = new KMenu(_tabWidget);   
    
     //Create a colour selection palette and fill it with a range of suitable colours
     QString paletteName;
     QStringList availablePalettes = KPalette::getPaletteList();

     if (availablePalettes.contains("40.colors"))
        paletteName = "40.colors";

    KPalette palette(paletteName);

    //If the palette of colours was found, create a palette menu displaying those colors
    //which the user chooses from when they activate the "Select Tab Color" sub-menu.
    //
    //If the palette is empty, default back to the old behaviour where the user is shown
    //a color dialog when they click the "Select Tab Color" menu item.
    if ( palette.nrColors() > 0 )
    {
        _tabColorCells = new KColorCells(_tabWidget,palette.nrColors()/8,8);

        for (int i=0;i<palette.nrColors();i++)
            _tabColorCells->setColor(i,palette.color(i));


        _tabSelectColorMenu = new KMenu(_tabWidget);
        connect( _tabSelectColorMenu, SIGNAL(aboutToShow()) , this, SLOT(prepareColorCells()) );
        _tabColorSelector = new QWidgetAction(_tabSelectColorMenu);
        _tabColorSelector->setDefaultWidget(_tabColorCells);

        _tabSelectColorMenu->addAction( _tabColorSelector );

        connect(_tabColorCells,SIGNAL(colorSelected(int)),this,SLOT(selectTabColor()));
        connect(_tabColorCells,SIGNAL(colorSelected(int)),_tabContextMenu,SLOT(hide()));
        
        QAction* action = _tabSelectColorMenu->menuAction(); 
            //_tabPopupMenu->addMenu(_tabSelectColorMenu);
        action->setIcon( KIcon("colors") );
        action->setText( i18n("Select &Tab Color") );

        _viewActions << action;
    }
    else
    {
      //  _viewActions << new KAction( KIcon("colors"),i18n("Select &Tab Color..."),this,
      //                  SLOT(slotTabSelectColor()));
    }


   connect( _tabWidget , SIGNAL(contextMenu(QWidget*,const QPoint&)),
                         SLOT(showContextMenu(QWidget*,const QPoint&))); 
}

TabbedViewContainer::~TabbedViewContainer()
{
    delete _tabContextMenu;
    delete _tabWidget;
}

void TabbedViewContainer::showContextMenu(QWidget* widget , const QPoint& position)
{
    //TODO - Use the tab under the mouse, not just the active tab
    _contextMenuTab = _tabWidget->indexOf(widget);
    //NavigationItem* item = navigationItem( widget );
   
    _tabContextMenu->clear();
//    _tabContextMenu->addActions( item->contextMenuActions(_viewActions) );
    _tabContextMenu->popup( position );
}

void TabbedViewContainer::prepareColorCells()
{
 //set selected color in palette widget to color of active tab

    QColor activeTabColor = _tabWidget->tabTextColor( _contextMenuTab );

    for (int i=0;i<_tabColorCells->numCells();i++)
        if ( activeTabColor == _tabColorCells->color(i) )
        {
            _tabColorCells->setSelected(i);
            break;
        } 
}

void TabbedViewContainer::viewAdded( QWidget* view )
{
    ViewProperties* item = viewProperties(view);
    connect( item , SIGNAL(titleChanged(ViewProperties*)) , this , SLOT(updateTitle(ViewProperties*))); 
    connect( item , SIGNAL(iconChanged(ViewProperties*) ) , this ,SLOT(updateIcon(ViewProperties*)));          
    _tabWidget->addTab( view , item->icon() , item->title() );
}
void TabbedViewContainer::viewRemoved( QWidget* view )
{
    Q_ASSERT( _tabWidget->indexOf(view) != -1 );

    _tabWidget->removeTab( _tabWidget->indexOf(view) );
}

void TabbedViewContainer::updateIcon(ViewProperties* item)
{
    kDebug() << __FUNCTION__ << ": icon changed." << endl;

    QList<QWidget*> items = widgetsForItem(item);
    QListIterator<QWidget*> itemIter(items);
    
    while ( itemIter.hasNext() )
    {
        int index = _tabWidget->indexOf( itemIter.next() );
        _tabWidget->setTabIcon( index , item->icon() );
    }
}
void TabbedViewContainer::updateTitle(ViewProperties* item) 
{
    kDebug() << __FUNCTION__ << ": title changed." << endl;

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

void TabbedViewContainer::setActiveView(QWidget* view)
{
    _tabWidget->setCurrentWidget(view);
}

void TabbedViewContainer::selectTabColor()
{
  QColor color;
  
  //If the color palette is available apply the current selected color to the tab, otherwise
  //default back to showing KDE's color dialog instead.
  if ( _tabColorCells )
  {
    color = _tabColorCells->color(_tabColorCells->getSelected());

    if (!color.isValid())
            return;
  }
  else
  {
    QColor defaultColor = _tabWidget->palette().color( QPalette::Foreground );
    QColor tempColor = _tabWidget->tabTextColor( _contextMenuTab );

    if ( KColorDialog::getColor(tempColor,defaultColor,_tabWidget) == KColorDialog::Accepted )
        color = tempColor;
    else
        return;
  }

  _tabWidget->setTabTextColor( _contextMenuTab , color );
}

StackedViewContainer::StackedViewContainer(QObject* parent) : ViewContainer(parent)
{
    _stackWidget = new QStackedWidget;
}
StackedViewContainer::~StackedViewContainer()
{
    delete _stackWidget;
}
QWidget* StackedViewContainer::containerWidget() const
{
    return _stackWidget;
}
QWidget* StackedViewContainer::activeView() const
{
    return _stackWidget->currentWidget();
}
void StackedViewContainer::setActiveView(QWidget* view)
{
   _stackWidget->setCurrentWidget(view); 
}
void StackedViewContainer::viewAdded( QWidget* view )
{
    _stackWidget->addWidget(view);
}
void StackedViewContainer::viewRemoved( QWidget* view )
{
    _stackWidget->removeWidget(view);
}

#include "ViewContainer.moc"

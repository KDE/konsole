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
#include <QLinearGradient>
#include <QListWidget>
#include <QSplitter>
#include <QStackedWidget>
#include <QToolButton>
#include <QWidgetAction>

// KDE 
#include <KColorDialog>
#include <KIconLoader>
#include <KLocale>
#include <KMenu>
#include <KPalette>
#include <KTabWidget>

// Konsole
#include "ViewContainer.h"
#include "ViewProperties.h"

// TODO Perhaps move everything which is Konsole-specific into different files
#include "SessionListWidget.h"

using namespace Konsole;

ViewContainer::~ViewContainer()
{
    emit destroyed(this);

}
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

    viewRemoved(widget);

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

void ViewContainer::activateNextView()
{
    QWidget* active = activeView();

    int index = _views.indexOf(active);

    if ( index == -1 )
        return;

    if ( index == _views.count() - 1 )
        index = 0;
    else
        index++;

    setActiveView( _views.at(index) );
}

void ViewContainer::activatePreviousView()
{
    QWidget* active = activeView();

    int index = _views.indexOf(active);

    if ( index == -1 ) 
        return;

    if ( index == 0 )
        index = _views.count() - 1;
    else
        index--;

    setActiveView( _views.at(index) );
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
   ,_newSessionButton(0) 
   ,_tabContextMenu(0) 
   ,_tabSelectColorMenu(0)
   ,_tabColorSelector(0)
   ,_tabColorCells(0)
   ,_contextMenuTab(0) 
{
    _tabWidget = new KTabWidget();
    _tabWidget->setDrawTabFrame(false);
    _tabContextMenu = new KMenu(_tabWidget);   

    _newSessionButton = new QToolButton(_tabWidget);
    _newSessionButton->setAutoRaise(true);
    _newSessionButton->setIcon( KIcon("tab-new") );
    _newSessionButton->setText( i18n("New") );
    _newSessionButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    _newSessionButton->setPopupMode(QToolButton::MenuButtonPopup);

    QToolButton* closeButton = new QToolButton(_tabWidget);
    closeButton->setIcon( KIcon("tab-remove") );
    closeButton->setAutoRaise(true);
    connect( closeButton , SIGNAL(clicked()) , this , SLOT(closeTabClicked()) );

    _tabWidget->setCornerWidget(_newSessionButton,Qt::TopLeftCorner);
    _tabWidget->setCornerWidget(closeButton,Qt::TopRightCorner);

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


   connect( _tabWidget , SIGNAL(currentChanged(int)) , this , SLOT(currentTabChanged(int)) );
   connect( _tabWidget , SIGNAL(contextMenu(QWidget*,const QPoint&)),
                         SLOT(showContextMenu(QWidget*,const QPoint&))); 
}

void TabbedViewContainer::currentTabChanged(int tab)
{
    if ( tab >= 0 )
    {
        emit activeViewChanged( _tabWidget->widget(tab) );
    }
}

void TabbedViewContainer::closeTabClicked()
{
    emit closeRequest(_tabWidget->currentWidget());
}

TabbedViewContainer::~TabbedViewContainer()
{
    _tabContextMenu->deleteLater();
    _tabWidget->deleteLater();
}

void TabbedViewContainer::setNewSessionMenu(QMenu* menu)
{
    _newSessionButton->setMenu(menu);
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

    for (int i=0;i<_tabColorCells->count();i++)
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
    const int index = _tabWidget->indexOf(view);

    if ( index != -1 )
        _tabWidget->removeTab( index );
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
    color = _tabColorCells->color(_tabColorCells->selectedIndex());

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

ListViewContainer::ListViewContainer(QObject* parent)
    : ViewContainer(parent)
{
    _splitter = new QSplitter;
    _stackWidget = new QStackedWidget(_splitter);
    _listWidget = new SessionListWidget(_splitter);

    // elide left is used because the most informative part of the session name is often
    // the rightmost part
    //
    // this means you get entries looking like:
    //
    // ...dirA ...dirB ...dirC  ( helpful )
    //
    // instead of
    //
    // johnSmith@comput... johnSmith@comput...  ( not so helpful )
    //

    _listWidget->setTextElideMode( Qt::ElideLeft );
    _listWidget->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    _listWidget->setDragDropMode(QAbstractItemView::DragDrop);
    _splitter->addWidget(_listWidget);
    _splitter->addWidget(_stackWidget);
        
    connect( _listWidget , SIGNAL(currentRowChanged(int)) , this , SLOT(rowChanged(int)) ); 
}

ListViewContainer::~ListViewContainer()
{
    _splitter->deleteLater();
}

QWidget* ListViewContainer::containerWidget() const
{
    return _splitter;
}

QWidget* ListViewContainer::activeView() const
{
    return _stackWidget->currentWidget();
}

QBrush ListViewContainer::randomItemBackground(int r)
{
    int i = r%6;

    //and now for something truly unpleasant:
    static const int r1[] = {255,190,190,255,190,255};
    static const int r2[] = {255,180,180,255,180,255};
    static const int b1[] = {190,255,190,255,255,190};
    static const int b2[] = {180,255,180,255,255,180};
    static const int g1[] = {190,190,255,190,255,255};
    static const int g2[] = {180,180,255,180,255,255};

    // hardcoded assumes item height is 32px
    QLinearGradient gradient( QPoint(0,0) , QPoint(0,32) );
    gradient.setColorAt(0,QColor(r1[i],g1[i],b1[i],100));
    gradient.setColorAt(0.5,QColor(r2[i],g2[i],b2[i],100));
    gradient.setColorAt(1,QColor(r1[i],g1[i],b1[i],100));
    return QBrush(gradient);
}

void ListViewContainer::viewAdded( QWidget* view )
{
    _stackWidget->addWidget(view);

    ViewProperties* properties = viewProperties(view);

    QListWidgetItem* item = new QListWidgetItem(_listWidget);
    item->setText( properties->title() );
    item->setIcon( properties->icon() );

    const int randomIndex = _listWidget->count();
    item->setData( Qt::BackgroundRole , randomItemBackground(randomIndex) );
   
    connect( properties , SIGNAL(titleChanged(ViewProperties*)) , this , SLOT(updateTitle(ViewProperties*)));
    connect( properties , SIGNAL(iconChanged(ViewProperties*)) , this , SLOT(updateIcon(ViewProperties*)));
}

void ListViewContainer::viewRemoved( QWidget* view )
{
    int index = _stackWidget->indexOf(view);
    _stackWidget->removeWidget(view);
    delete _listWidget->takeItem( index );
}

void ListViewContainer::setActiveView( QWidget* view )
{
    _stackWidget->setCurrentWidget(view);
    _listWidget->setCurrentRow(_stackWidget->indexOf(view));
}

void ListViewContainer::rowChanged( int row )
{
    // row may be -1 if the last row has been removed from the model
    if ( row >= 0 )
    {
        _stackWidget->setCurrentIndex( row );

        emit activeViewChanged( _stackWidget->currentWidget() );
    }
}

void ListViewContainer::updateTitle( ViewProperties* properties )
{
    QList<QWidget*> items = widgetsForItem(properties);
    QListIterator<QWidget*> itemIter(items);

    while ( itemIter.hasNext() )
    {
        int index = _stackWidget->indexOf( itemIter.next() );
        _listWidget->item( index )->setText( properties->title() );
    }
}

void ListViewContainer::updateIcon( ViewProperties* properties )
{
    QList<QWidget*> items = widgetsForItem(properties);
    QListIterator<QWidget*> itemIter(items);

    while ( itemIter.hasNext() )
    {
        int index = _stackWidget->indexOf( itemIter.next() );
        _listWidget->item( index )->setIcon( properties->icon() );
    }
}

#include "ViewContainer.moc"

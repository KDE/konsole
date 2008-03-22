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

// Own
#include "ViewContainer.h"

// Qt
#include <QtCore/QHash>
#include <QtGui/QLineEdit>
#include <QtGui/QBrush>
#include <QtGui/QListWidget>
#include <QtGui/QPushButton>
#include <QtGui/QSplitter>
#include <QtGui/QStackedWidget>
#include <QtGui/QTabBar>
#include <QtGui/QToolButton>
#include <QtGui/QWidgetAction>

// KDE 
#include <KColorDialog>
#include <kcolorscheme.h>
#include <kcolorutils.h>
#include <kdebug.h>
#include <kconfiggroup.h>
#include <KLocale>
#include <KMenu>
#include <KColorCollection>
#include <KTabWidget>

// Konsole
#include "ViewProperties.h"

// TODO Perhaps move everything which is Konsole-specific into different files
#include "ProfileListWidget.h"

using namespace Konsole;

ViewContainer::ViewContainer(NavigationPosition position , QObject* parent)
    : QObject(parent)
    , _navigationDisplayMode(AlwaysShowNavigation)
    , _navigationPosition(position)
{
}
ViewContainer::~ViewContainer()
{
    foreach( QWidget* view , _views ) 
    {
        disconnect(view,SIGNAL(destroyed(QObject*)),this,SLOT(viewDestroyed(QObject*)));
    }

    emit destroyed(this);
}
void ViewContainer::moveViewWidget( int , int ) {}

void ViewContainer::moveActiveView( MoveDirection direction )
{
    const int currentIndex = _views.indexOf( activeView() ) ;
    int newIndex = -1; 

    switch ( direction )
    {
        case MoveViewLeft:
            newIndex = qMax( currentIndex-1 , 0 );
            break;
        case MoveViewRight:
            newIndex = qMin( currentIndex+1 , _views.count() -1 );
            break;
    }

    Q_ASSERT( newIndex != -1 );

    moveViewWidget( currentIndex , newIndex );   

    _views.swap(currentIndex,newIndex);

    setActiveView( _views[newIndex] );
}

void ViewContainer::setNavigationDisplayMode(NavigationDisplayMode mode)
{
    _navigationDisplayMode = mode;

#if 0
    if ( mode == AlwaysShowNavigation )
        kDebug() << "Always show nav";
    else if ( mode == AlwaysHideNavigation )
        kDebug() << "Always hide nav";
    else if ( mode == ShowNavigationAsNeeded )
        kDebug() << "Show nav as needed";
#endif

    navigationDisplayModeChanged(mode);
}
ViewContainer::NavigationPosition ViewContainer::navigationPosition() const
{
    return _navigationPosition;
}
void ViewContainer::setNavigationPosition(NavigationPosition position)
{
    // assert that this position is supported
    Q_ASSERT( supportedNavigationPositions().contains(position) );

    _navigationPosition = position;

    navigationPositionChanged(position);
}
QList<ViewContainer::NavigationPosition> ViewContainer::supportedNavigationPositions() const
{
    return QList<NavigationPosition>() << NavigationPositionTop;
}
ViewContainer::NavigationDisplayMode ViewContainer::navigationDisplayMode() const
{
    return _navigationDisplayMode;
}
void ViewContainer::addView(QWidget* view , ViewProperties* item)
{
    _views << view;
    _navigation[view] = item;

    connect( view , SIGNAL(destroyed(QObject*)) , this , SLOT( viewDestroyed(QObject*) ) );

    addViewWidget(view);

    emit viewAdded(view,item);
}
void ViewContainer::viewDestroyed(QObject* object)
{
    QWidget* widget = static_cast<QWidget*>(object);

    _views.removeAll(widget);
    _navigation.remove(widget);

    // FIXME This can result in ViewContainerSubClass::removeViewWidget() being 
    // called after the the widget's parent has been deleted or partially deleted
    // in the ViewContainerSubClass instance's destructor.
    //
    // Currently deleteLater() is used to remove child widgets in the subclass 
    // constructors to get around the problem, but this is a hack and needs
    // to be fixed. 
    removeViewWidget(widget);
    
    emit viewRemoved(widget);

    if (_views.count() == 0)
        emit empty(this);
}
void ViewContainer::removeView(QWidget* view)
{
    _views.removeAll(view);
    _navigation.remove(view);

    disconnect( view , SIGNAL(destroyed(QObject*)) , this , SLOT( viewDestroyed(QObject*) ) );

    removeViewWidget(view);
    
    emit viewRemoved(view);

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

TabbedViewContainer::TabbedViewContainer(NavigationPosition position , QObject* parent) :
    ViewContainer(position,parent)
   ,_newSessionButton(0) 
   ,_tabContextMenu(0) 
   ,_tabSelectColorMenu(0)
   ,_tabColorSelector(0)
   ,_tabColorCells(0)
   ,_contextMenuTab(0) 
{
    _tabWidget = new KTabWidget();
    _tabContextMenu = new KMenu(_tabWidget);   

    _newSessionButton = new QToolButton(_tabWidget);
    _newSessionButton->setAutoRaise(true);
    _newSessionButton->setIcon( KIcon("tab-new") );
    _newSessionButton->setText( i18n("New") );
    _newSessionButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    _newSessionButton->setPopupMode(QToolButton::MenuButtonPopup);

    QToolButton* closeButton = new QToolButton(_tabWidget);
    closeButton->setIcon( KIcon("tab-close") );
    closeButton->setAutoRaise(true);
    connect( closeButton , SIGNAL(clicked()) , this , SLOT(closeTabClicked()) );

    _tabWidget->setCornerWidget(_newSessionButton,Qt::TopLeftCorner);
    _tabWidget->setCornerWidget(closeButton,Qt::TopRightCorner);

     //Create a colour selection palette and fill it with a range of suitable colours
     QString paletteName;
     QStringList availablePalettes = KColorCollection::installedCollections();

     if (availablePalettes.contains("40.colors"))
        paletteName = "40.colors";

    KColorCollection palette(paletteName);

    //If the palette of colours was found, create a palette menu displaying those colors
    //which the user chooses from when they activate the "Select Tab Color" sub-menu.
    //
    //If the palette is empty, default back to the old behaviour where the user is shown
    //a color dialog when they click the "Select Tab Color" menu item.
    if ( palette.count() > 0 )
    {
        _tabColorCells = new KColorCells(_tabWidget,palette.count()/8,8);

        for (int i=0;i<palette.count();i++)
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

void TabbedViewContainer::addViewWidget( QWidget* view )
{
    ViewProperties* item = viewProperties(view);
    connect( item , SIGNAL(titleChanged(ViewProperties*)) , this , SLOT(updateTitle(ViewProperties*))); 
    connect( item , SIGNAL(iconChanged(ViewProperties*) ) , this ,SLOT(updateIcon(ViewProperties*)));          
    _tabWidget->addTab( view , item->icon() , item->title() );
}
void TabbedViewContainer::removeViewWidget( QWidget* view )
{
    const int index = _tabWidget->indexOf(view);

    if ( index != -1 )
        _tabWidget->removeTab( index );
}

void TabbedViewContainer::updateIcon(ViewProperties* item)
{
    kDebug() << k_funcinfo << ": icon changed.";

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
    kDebug() << k_funcinfo << ": title changed.";

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

ViewContainerTabBar::ViewContainerTabBar(QWidget* parent)
    : KTabBar(parent)
{
    //TODO  Make tab re-ordering possible, but the QStackedWidget
    // and tab widget will then have different indicies, in which
    // case either the view widget stack needs to be re-ordered
    // or mapping between the tab and stack widget indicies needs
    // to be added
    //
    //setTabReorderingEnabled(true);
}

QSize ViewContainerTabBar::tabSizeHint(int index) const
{
     return QTabBar::tabSizeHint(index);
}

TabbedViewContainerV2::TabbedViewContainerV2(NavigationPosition position , QObject* parent) 
: ViewContainer(position,parent)
{
    _containerWidget = new QWidget;
    _stackWidget = new QStackedWidget();
    _tabBar = new ViewContainerTabBar();
    _tabBar->setDrawBase(true);
   
    connect( _tabBar , SIGNAL(currentChanged(int)) , this , SLOT(currentTabChanged(int)) );
    connect( _tabBar , SIGNAL(tabDoubleClicked(int)) , this , SLOT(tabDoubleClicked(int)) );
    connect( _tabBar , SIGNAL(newTabRequest()) , this , SIGNAL(newViewRequest()) );
    connect( _tabBar , SIGNAL(wheelDelta(int)) , this , SLOT(wheelScrolled(int)) );
	connect( _tabBar , SIGNAL(mouseMiddleClick(int)) , this , SLOT(closeTab(int)) );

    _layout = new TabbedViewContainerV2Layout;
    _layout->setSpacing(0);
    _layout->setMargin(0);
   
    _tabBarSpacer = new QSpacerItem(0,TabBarSpace);

    _layout->addWidget(_stackWidget);
    
    if ( position == NavigationPositionTop )
    {
        _layout->insertWidget(0,_tabBar);
        _layout->insertItemAt(0,_tabBarSpacer);
        _tabBar->setShape(QTabBar::RoundedNorth);
    }
    else if ( position == NavigationPositionBottom )
    {
        _layout->insertWidget(-1,_tabBar);
        _layout->insertItemAt(-1,_tabBarSpacer);
        _tabBar->setShape(QTabBar::RoundedSouth);
    }
    else
        Q_ASSERT(false); // position not supported

    _containerWidget->setLayout(_layout);
}
void TabbedViewContainerV2::closeTab(int tab)
{
	Q_ASSERT(tab >= 0 && tab < _stackWidget->count());
	removeView(_stackWidget->widget(tab));
}
void TabbedViewContainerV2::setTabBarVisible(bool visible)
{
    _tabBar->setVisible(visible);
    if ( visible )
    {
        _tabBarSpacer->changeSize(0,TabBarSpace);
    }
    else
    {
        _tabBarSpacer->changeSize(0,0);
    } 
}
QList<ViewContainer::NavigationPosition> TabbedViewContainerV2::supportedNavigationPositions() const
{
    return QList<NavigationPosition>() << NavigationPositionTop << NavigationPositionBottom;
}
void TabbedViewContainerV2::navigationPositionChanged(NavigationPosition position)
{
    // this method assumes that there are only three items 
    // in the layout
    Q_ASSERT( _layout->count() == 3 );

    // index of stack widget in the layout when tab bar is at the bottom
    const int StackIndexWithTabBottom = 0;

    if ( position == NavigationPositionTop 
            && _layout->indexOf(_stackWidget) == StackIndexWithTabBottom )
    {
        _layout->removeWidget(_tabBar);
        _layout->removeItem(_tabBarSpacer);

        _layout->insertWidget(0,_tabBar);
        _layout->insertItemAt(0,_tabBarSpacer);
        _tabBar->setShape(QTabBar::RoundedNorth);
    }
    else if ( position == NavigationPositionBottom 
            && _layout->indexOf(_stackWidget) != StackIndexWithTabBottom )
    {
        _layout->removeWidget(_tabBar);
        _layout->removeItem(_tabBarSpacer);

        _layout->insertWidget(-1,_tabBar);
        _layout->insertItemAt(-1,_tabBarSpacer);
        _tabBar->setShape(QTabBar::RoundedSouth);
    }
}
void TabbedViewContainerV2::navigationDisplayModeChanged(NavigationDisplayMode mode)
{
    if ( mode == AlwaysShowNavigation && _tabBar->isHidden() )
        setTabBarVisible(true);

    if ( mode == AlwaysHideNavigation && !_tabBar->isHidden() )
        setTabBarVisible(false);

    if ( mode == ShowNavigationAsNeeded )
        dynamicTabBarVisibility();
}
void TabbedViewContainerV2::dynamicTabBarVisibility()
{
    kDebug() << "tab bar count:" << _tabBar->count();
    kDebug() << "tab var hidden:" << _tabBar->isHidden();

    if ( _tabBar->count() > 1 && _tabBar->isHidden() )
        setTabBarVisible(true);

    if ( _tabBar->count() < 2 && !_tabBar->isHidden() )
        setTabBarVisible(false);    
}
TabbedViewContainerV2::~TabbedViewContainerV2()
{
    _containerWidget->deleteLater();
}
void TabbedViewContainerV2::tabDoubleClicked(int tab)
{
    viewProperties( views()[tab] )->rename();
}
void TabbedViewContainerV2::moveViewWidget( int fromIndex , int toIndex )
{
    QString text = _tabBar->tabText(fromIndex);
    QIcon icon = _tabBar->tabIcon(fromIndex);
   
    // FIXME - This will lose properties of the tab other than
    // their text and icon when moving them
    
    _tabBar->removeTab(fromIndex);
    _tabBar->insertTab(toIndex,icon,text);

    QWidget* widget = _stackWidget->widget(fromIndex);
    _stackWidget->removeWidget(widget);
    _stackWidget->insertWidget(toIndex,widget);
}
void TabbedViewContainerV2::currentTabChanged(int index)
{
    _stackWidget->setCurrentIndex(index);
    emit activeViewChanged(_stackWidget->widget(index));

    // clear activity indicators
    setTabActivity(index,false);
}

void TabbedViewContainerV2::wheelScrolled(int delta)
{
    if ( delta < 0 )
	activateNextView();
    else
	activatePreviousView();
}

QWidget* TabbedViewContainerV2::containerWidget() const
{
    return _containerWidget;
}
QWidget* TabbedViewContainerV2::activeView() const
{
    return _stackWidget->currentWidget();
}
void TabbedViewContainerV2::setActiveView(QWidget* view)
{
    const int index = _stackWidget->indexOf(view);

    Q_ASSERT( index != -1 );

   _stackWidget->setCurrentWidget(view);
   _tabBar->setCurrentIndex(index); 
}
void TabbedViewContainerV2::addViewWidget( QWidget* view )
{
    _stackWidget->addWidget(view);
    _stackWidget->updateGeometry();

    ViewProperties* item = viewProperties(view);
    connect( item , SIGNAL(titleChanged(ViewProperties*)) , this , 
                    SLOT(updateTitle(ViewProperties*))); 
    connect( item , SIGNAL(iconChanged(ViewProperties*) ) , this , 
                    SLOT(updateIcon(ViewProperties*)));
    connect( item , SIGNAL(activity(ViewProperties*)) , this ,
                    SLOT(updateActivity(ViewProperties*)));

    _tabBar->addTab( item->icon() , item->title() );

    if ( navigationDisplayMode() == ShowNavigationAsNeeded )
        dynamicTabBarVisibility();
}
void TabbedViewContainerV2::removeViewWidget( QWidget* view )
{
    const int index = _stackWidget->indexOf(view);

    Q_ASSERT( index != -1 );

    _stackWidget->removeWidget(view);
    _tabBar->removeTab(index);

    if ( navigationDisplayMode() == ShowNavigationAsNeeded )
        dynamicTabBarVisibility();
}

void TabbedViewContainerV2::setTabActivity(int index , bool activity)
{
    const QPalette& palette = _tabBar->palette();
    KColorScheme colorScheme(palette.currentColorGroup());
    const QColor colorSchemeActive = colorScheme.foreground(KColorScheme::ActiveText).color();    
    
    const QColor normalColor = palette.text().color();
    const QColor activityColor = KColorUtils::mix(normalColor,colorSchemeActive); 


    QColor color = activity ? activityColor : normalColor;

    if ( color != _tabBar->tabTextColor(index) )
        _tabBar->setTabTextColor(index,color);
}

void TabbedViewContainerV2::updateActivity(ViewProperties* item)
{
    QListIterator<QWidget*> iter(widgetsForItem(item));
    while ( iter.hasNext() )
    {
        const int index = _stackWidget->indexOf(iter.next());

        if ( index != _stackWidget->currentIndex() )
        {
            setTabActivity(index,true);
        } 
    }
}

void TabbedViewContainerV2::updateTitle(ViewProperties* item)
{
	// prevent tab titles from becoming overly-long as this limits the number
	// of tabs which can fit in the tab bar.  
	//
	// if the view's title is overly long then trim it and select the 
	// right-most 20 characters (assuming they contain the most useful
	// information) and insert an elide at the front
	const int MAX_TAB_TEXT_LENGTH = 20;

    QListIterator<QWidget*> iter(widgetsForItem(item));
    while ( iter.hasNext() )
    {
        const int index = _stackWidget->indexOf( iter.next() );

		QString tabText = item->title();
		if (tabText.count() > MAX_TAB_TEXT_LENGTH)
			tabText = tabText.right(MAX_TAB_TEXT_LENGTH).prepend("...");

        _tabBar->setTabText( index , tabText );
    }
}
void TabbedViewContainerV2::updateIcon(ViewProperties* item)
{
    kDebug() << "Tab icon changed.";

    QListIterator<QWidget*> iter(widgetsForItem(item));
    while ( iter.hasNext() )
    {
        const int index = _stackWidget->indexOf( iter.next() );
        _tabBar->setTabIcon( index , item->icon() );
    }
}

StackedViewContainer::StackedViewContainer(QObject* parent) 
: ViewContainer(NavigationPositionTop,parent)
{
    _stackWidget = new QStackedWidget;
}
StackedViewContainer::~StackedViewContainer()
{
    _stackWidget->deleteLater();
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
void StackedViewContainer::addViewWidget( QWidget* view )
{
    _stackWidget->addWidget(view);
}
void StackedViewContainer::removeViewWidget( QWidget* view )
{
    const int index = _stackWidget->indexOf(view);

    Q_ASSERT( index != -1);

    _stackWidget->removeWidget(view);
}

ListViewContainer::ListViewContainer(NavigationPosition position,QObject* parent)
    : ViewContainer(position,parent)
{
    _splitter = new QSplitter;
    _stackWidget = new QStackedWidget(_splitter);
    _listWidget = new ProfileListWidget(_splitter);

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

void ListViewContainer::addViewWidget( QWidget* view )
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

void ListViewContainer::removeViewWidget( QWidget* view )
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

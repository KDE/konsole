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
#include "ViewContainer.h"

// Qt
#include <QtCore/QHash>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QBrush>
#include <QtGui/QListWidget>
#include <QtGui/QSplitter>
#include <QtGui/QStackedWidget>
#include <QtGui/QTabBar>
#include <QtGui/QToolButton>
#include <QtGui/QWidgetAction>

#include <QtGui/QDrag>
#include <QtGui/QDragMoveEvent>
#include <QMimeData>

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
#include "IncrementalSearchBar.h"
#include "ViewProperties.h"

// TODO Perhaps move everything which is Konsole-specific into different files
#include "ProfileListWidget.h"

using namespace Konsole;

ViewContainer::ViewContainer(NavigationPosition position , QObject* parent)
    : QObject(parent)
    , _navigationDisplayMode(AlwaysShowNavigation)
    , _navigationPosition(position)
    , _searchBar(0)
{
}

ViewContainer::~ViewContainer()
{
    foreach( QWidget* view , _views ) 
    {
        disconnect(view,SIGNAL(destroyed(QObject*)),this,SLOT(viewDestroyed(QObject*)));
    }

    if (_searchBar) {
        _searchBar->deleteLater();
    }

    emit destroyed(this);
}
void ViewContainer::moveViewWidget( int , int ) {}
void ViewContainer::setFeatures(Features features)
{ _features = features; }
ViewContainer::Features ViewContainer::features() const
{ return _features; }
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
void ViewContainer::addView(QWidget* view , ViewProperties* item, int index)
{
    if (index == -1)
        _views.append(view);
    else
        _views.insert(index,view);

    _navigation[view] = item;

    connect( view , SIGNAL(destroyed(QObject*)) , this , SLOT( viewDestroyed(QObject*) ) );

    addViewWidget(view,index);

    emit viewAdded(view,item);
}
void ViewContainer::viewDestroyed(QObject* object)
{
    QWidget* widget = static_cast<QWidget*>(object);

    _views.removeAll(widget);
    _navigation.remove(widget);

    // FIXME This can result in ViewContainerSubClass::removeViewWidget() being 
    // called after the widget's parent has been deleted or partially deleted
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

IncrementalSearchBar* ViewContainer::searchBar()
{
    if (!_searchBar) {
        _searchBar = new IncrementalSearchBar( IncrementalSearchBar::AllFeatures , 0);
        _searchBar->setVisible(false);
        connect(_searchBar, SIGNAL(destroyed(QObject*)), this, SLOT(searchBarDestroyed()));
    }
    return _searchBar;
}

void ViewContainer::searchBarDestroyed()
{
    _searchBar = 0;
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

ViewContainerTabBar::ViewContainerTabBar(QWidget* parent,TabbedViewContainer* container)
    : KTabBar(parent)
    , _container(container)
    , _dropIndicator(0)
    , _dropIndicatorIndex(-1)
    , _drawIndicatorDisabled(false)
{
}
void ViewContainerTabBar::setDropIndicator(int index, bool drawDisabled)
{
    if (!parentWidget() || _dropIndicatorIndex == index)
        return;

    _dropIndicatorIndex = index;
    const int ARROW_SIZE = 22;
    bool north = shape() == QTabBar::RoundedNorth || shape() == QTabBar::TriangularNorth;

    if (!_dropIndicator || _drawIndicatorDisabled != drawDisabled)
    {
        if (!_dropIndicator)
        {
            _dropIndicator = new QLabel(parentWidget());
            _dropIndicator->resize(ARROW_SIZE,ARROW_SIZE);
        }

        QIcon::Mode drawMode = drawDisabled ? QIcon::Disabled : QIcon::Normal;
        const QString iconName = north ? "arrow-up" : "arrow-down";
        _dropIndicator->setPixmap(KIcon(iconName).pixmap(ARROW_SIZE,ARROW_SIZE,drawMode));
        _drawIndicatorDisabled = drawDisabled;
    }

    if (index < 0)
    {
        _dropIndicator->hide();
        return;
    }

    const QRect rect = tabRect(index < count() ? index : index-1);

    QPoint pos;
    if (index < count())
        pos = rect.topLeft();
    else
        pos = rect.topRight();

    if (north)
        pos.ry() += ARROW_SIZE;
    else
        pos.ry() -= ARROW_SIZE; 

    pos.rx() -= ARROW_SIZE/2; 

    _dropIndicator->move(mapTo(parentWidget(),pos));
    _dropIndicator->show();

}
void ViewContainerTabBar::dragLeaveEvent(QDragLeaveEvent*)
{
    setDropIndicator(-1);
}
void ViewContainerTabBar::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat(ViewProperties::mimeType()) &&
        event->source() != 0)
        event->acceptProposedAction();    
}
void ViewContainerTabBar::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasFormat(ViewProperties::mimeType())
        && event->source() != 0)
    {
        int index = dropIndex(event->pos());
        if (index == -1)
            index = count();

        setDropIndicator(index,proposedDropIsSameTab(event));

        event->acceptProposedAction();
    }
}
int ViewContainerTabBar::dropIndex(const QPoint& pos) const
{
    int tab = tabAt(pos);
    if (tab < 0)
        return tab;

    // pick the closest tab boundary 
    QRect rect = tabRect(tab);
    if ( (pos.x()-rect.left()) > (rect.width()/2) )
        tab++;

    if (tab == count())
        return -1;

    return tab;
}
bool ViewContainerTabBar::proposedDropIsSameTab(const QDropEvent* event) const
{
    int index = dropIndex(event->pos());
    int droppedId = ViewProperties::decodeMimeData(event->mimeData());
    bool sameTabBar = event->source() == this;

    if (!sameTabBar)
        return false;

    const QList<QWidget*> viewList = _container->views();
    int sourceIndex = -1;
    for (int i=0;i<count();i++)
    {
        int idAtIndex = _container->viewProperties(viewList[i])->identifier();
        if (idAtIndex == droppedId)
            sourceIndex = i;
    }

    bool sourceAndDropAreLast = sourceIndex == count()-1 && index == -1;
    if (sourceIndex == index || sourceIndex == index-1 || sourceAndDropAreLast)
        return true;
    else
        return false;
}
void ViewContainerTabBar::dropEvent(QDropEvent* event)
{
    setDropIndicator(-1);

    if (    !event->mimeData()->hasFormat(ViewProperties::mimeType())
        ||  proposedDropIsSameTab(event) )
    {
        event->ignore();
        return;
    }

    int index = dropIndex(event->pos());
    int droppedId = ViewProperties::decodeMimeData(event->mimeData());
    bool result = false;
    emit _container->moveViewRequest(index,droppedId,result);
    
    if (result)
        event->accept();
    else
        event->ignore();
}

QSize ViewContainerTabBar::tabSizeHint(int index) const
{
     return QTabBar::tabSizeHint(index);
}
QPixmap ViewContainerTabBar::dragDropPixmap(int tab) 
{
    Q_ASSERT(tab >= 0 && tab < count());

    // TODO - grabWidget() works except that it includes part
    // of the tab bar outside the tab itself if the tab has 
    // curved corners
    const QRect rect = tabRect(tab);
    const int borderWidth = 1;

    QPixmap tabPixmap(rect.width()+borderWidth,
                      rect.height()+borderWidth);
    QPainter painter(&tabPixmap);
    painter.drawPixmap(0,0,QPixmap::grabWidget(this,rect));
    QPen borderPen;
    borderPen.setBrush(palette().dark());
    borderPen.setWidth(borderWidth);
    painter.setPen(borderPen);
    painter.drawRect(0,0,rect.width(),rect.height());
    painter.end();

    return tabPixmap;
}
TabbedViewContainer::TabbedViewContainer(NavigationPosition position , QObject* parent) 
: ViewContainer(position,parent)
{
    _containerWidget = new QWidget;
    _stackWidget = new QStackedWidget();
    _tabBar = new ViewContainerTabBar(_containerWidget,this);
    _tabBar->setDrawBase(true);
    _tabBar->setDocumentMode(true);
    _tabBar->setFocusPolicy(Qt::ClickFocus);

    _newTabButton = new QToolButton(_containerWidget);
    _newTabButton->setIcon(KIcon("tab-new"));
    _newTabButton->adjustSize();
    // new tab button is initially hidden, it will be shown when setFeatures() is called
    // with the QuickNewView flag enabled
    _newTabButton->setHidden(true);

    _closeTabButton = new QToolButton(_containerWidget);
    _closeTabButton->setIcon(KIcon("tab-close"));
    _closeTabButton->adjustSize();
    _closeTabButton->setHidden(true);

    connect( _tabBar , SIGNAL(currentChanged(int)) , this , SLOT(currentTabChanged(int)) );
    connect( _tabBar , SIGNAL(tabDoubleClicked(int)) , this , SLOT(tabDoubleClicked(int)) );
    connect( _tabBar , SIGNAL(newTabRequest()) , this , SIGNAL(newViewRequest()) );
    connect( _tabBar , SIGNAL(wheelDelta(int)) , this , SLOT(wheelScrolled(int)) );
    connect( _tabBar , SIGNAL(closeRequest(int)) , this , SLOT(closeTab(int)) );
    connect( _tabBar , SIGNAL(initiateDrag(int)) , this , SLOT(startTabDrag(int)) );

    connect( _newTabButton , SIGNAL(clicked()) , this , SIGNAL(newViewRequest()) );
    connect( _closeTabButton , SIGNAL(clicked()) , this , SLOT(closeCurrentTab()) );

    _layout = new TabbedViewContainerLayout;
    _layout->setSpacing(0);
    _layout->setMargin(0);
    _tabBarLayout = new QHBoxLayout;
    _tabBarLayout->setSpacing(0);
    _tabBarLayout->setMargin(0);
    _tabBarLayout->addWidget(_newTabButton);
    _tabBarLayout->addWidget(_tabBar);
    _tabBarLayout->addWidget(_closeTabButton); 
    _tabBarSpacer = new QSpacerItem(0,TabBarSpace);

    _layout->addWidget(_stackWidget);
    searchBar()->setParent(_containerWidget);
    if ( position == NavigationPositionTop )
    {
        _layout->insertLayout(0,_tabBarLayout);
        _layout->insertItemAt(0,_tabBarSpacer);
        _layout->insertWidget(-1,searchBar());
        _tabBar->setShape(QTabBar::RoundedNorth);
    }
    else if ( position == NavigationPositionBottom )
    {
        _layout->insertWidget(-1,searchBar());
        _layout->insertLayout(-1,_tabBarLayout);
        _layout->insertItemAt(-1,_tabBarSpacer);
        _tabBar->setShape(QTabBar::RoundedSouth);
    }
    else
        Q_ASSERT(false); // position not supported

    _containerWidget->setLayout(_layout);
}
void TabbedViewContainer::setNewViewMenu(QMenu* menu)
{
  _newTabButton->setMenu(menu);
}
ViewContainer::Features TabbedViewContainer::supportedFeatures() const
{ 
  return QuickNewView|QuickCloseView;
}
void TabbedViewContainer::setFeatures(Features features)
{
    ViewContainer::setFeatures(features);

    const bool tabBarHidden = _tabBar->isHidden();
    _newTabButton->setVisible(!tabBarHidden && (features & QuickNewView));
    _closeTabButton->setVisible(!tabBarHidden && (features & QuickCloseView));
}
void TabbedViewContainer::closeCurrentTab()
{
    if (_stackWidget->currentIndex() != -1)
    {
        closeTab(_stackWidget->currentIndex());
    }
}
void TabbedViewContainer::closeTab(int tab)
{
    Q_ASSERT(tab >= 0 && tab < _stackWidget->count());
    
    if (viewProperties(_stackWidget->widget(tab))->confirmClose())
        removeView(_stackWidget->widget(tab));
}
void TabbedViewContainer::setTabBarVisible(bool visible)
{
    _tabBar->setVisible(visible);
    _newTabButton->setVisible(visible && (features() & QuickNewView));
    _closeTabButton->setVisible(visible && (features() & QuickCloseView));
    if ( visible )
    {
        _tabBarSpacer->changeSize(0,TabBarSpace);
    }
    else
    {
        _tabBarSpacer->changeSize(0,0);
    } 
}
QList<ViewContainer::NavigationPosition> TabbedViewContainer::supportedNavigationPositions() const
{
    return QList<NavigationPosition>() << NavigationPositionTop << NavigationPositionBottom;
}
void TabbedViewContainer::navigationPositionChanged(NavigationPosition position)
{
    // this method assumes that there are only three items 
    // in the layout
    Q_ASSERT( _layout->count() == 4 );

    // index of stack widget in the layout when tab bar is at the bottom
    const int StackIndexWithTabBottom = 0;

    if ( position == NavigationPositionTop 
            && _layout->indexOf(_stackWidget) == StackIndexWithTabBottom )
    {
        _layout->removeItem(_tabBarLayout);
        _layout->removeItem(_tabBarSpacer);
        _layout->removeWidget(searchBar());

        _layout->insertLayout(0,_tabBarLayout);
        _layout->insertItemAt(0,_tabBarSpacer);
        _layout->insertWidget(-1,searchBar());
        _tabBar->setShape(QTabBar::RoundedNorth);
    }
    else if ( position == NavigationPositionBottom 
            && _layout->indexOf(_stackWidget) != StackIndexWithTabBottom )
    {
        _layout->removeItem(_tabBarLayout);
        _layout->removeItem(_tabBarSpacer);
        _layout->removeWidget(searchBar());

        _layout->insertWidget(-1,searchBar());
        _layout->insertLayout(-1,_tabBarLayout);
        _layout->insertItemAt(-1,_tabBarSpacer);
        _tabBar->setShape(QTabBar::RoundedSouth);
    }
}
void TabbedViewContainer::navigationDisplayModeChanged(NavigationDisplayMode mode)
{
    if ( mode == AlwaysShowNavigation && _tabBar->isHidden() )
        setTabBarVisible(true);

    if ( mode == AlwaysHideNavigation && !_tabBar->isHidden() )
        setTabBarVisible(false);

    if ( mode == ShowNavigationAsNeeded )
        dynamicTabBarVisibility();
}
void TabbedViewContainer::dynamicTabBarVisibility()
{
    if ( _tabBar->count() > 1 && _tabBar->isHidden() )
        setTabBarVisible(true);

    if ( _tabBar->count() < 2 && !_tabBar->isHidden() )
        setTabBarVisible(false);    
}
TabbedViewContainer::~TabbedViewContainer()
{
    if (!_containerWidget.isNull())
        _containerWidget->deleteLater();
}

void TabbedViewContainer::startTabDrag(int tab)
{
    QDrag* drag = new QDrag(_tabBar);
    const QRect tabRect = _tabBar->tabRect(tab);
    QPixmap tabPixmap = _tabBar->dragDropPixmap(tab); 

    drag->setPixmap(tabPixmap);
    
    int id = viewProperties(views()[tab])->identifier();
    QWidget* view = views()[tab];
    drag->setMimeData(ViewProperties::createMimeData(id));

    // start drag, if drag-and-drop is successful the view at 'tab' will be
    // deleted
    //
    // if the tab was dragged onto another application
    // which blindly accepted the drop then ignore it
    if (drag->exec() == Qt::MoveAction && drag->target() != 0)
    {
        // Deleting the view may cause the view container to be deleted, which
        // will also delete the QDrag object.
        // This can cause a crash if Qt's internal drag-and-drop handling
        // tries to delete it later.  
        //
        // For now set the QDrag's parent to 0 so that it won't be deleted if 
        // this view container is destroyed.
        //
        // FIXME: Resolve this properly
        drag->setParent(0);
        removeView(view);
    }
}
void TabbedViewContainer::tabDoubleClicked(int tab)
{
    viewProperties( views()[tab] )->rename();
}
void TabbedViewContainer::moveViewWidget( int fromIndex , int toIndex )
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
void TabbedViewContainer::currentTabChanged(int index)
{
    _stackWidget->setCurrentIndex(index);
    if (_stackWidget->widget(index))
        emit activeViewChanged(_stackWidget->widget(index));

    // clear activity indicators
    setTabActivity(index,false);
}

void TabbedViewContainer::wheelScrolled(int delta)
{
    if ( delta < 0 )
    activateNextView();
    else
    activatePreviousView();
}

QWidget* TabbedViewContainer::containerWidget() const
{
    return _containerWidget;
}
QWidget* TabbedViewContainer::activeView() const
{
    return _stackWidget->currentWidget();
}
void TabbedViewContainer::setActiveView(QWidget* view)
{
    const int index = _stackWidget->indexOf(view);

    Q_ASSERT( index != -1 );

   _stackWidget->setCurrentWidget(view);
   _tabBar->setCurrentIndex(index); 
}
void TabbedViewContainer::addViewWidget( QWidget* view , int index)
{
    _stackWidget->insertWidget(index,view);
    _stackWidget->updateGeometry();

    ViewProperties* item = viewProperties(view);
    connect( item , SIGNAL(titleChanged(ViewProperties*)) , this , 
                    SLOT(updateTitle(ViewProperties*))); 
    connect( item , SIGNAL(iconChanged(ViewProperties*) ) , this , 
                    SLOT(updateIcon(ViewProperties*)));
    connect( item , SIGNAL(activity(ViewProperties*)) , this ,
                    SLOT(updateActivity(ViewProperties*)));

    _tabBar->insertTab( index , item->icon() , item->title() );

    if ( navigationDisplayMode() == ShowNavigationAsNeeded )
        dynamicTabBarVisibility();
}
void TabbedViewContainer::removeViewWidget( QWidget* view )
{
    if (!_stackWidget)
        return;
    const int index = _stackWidget->indexOf(view);

    Q_ASSERT( index != -1 );

    _stackWidget->removeWidget(view);
    _tabBar->removeTab(index);

    if ( navigationDisplayMode() == ShowNavigationAsNeeded )
        dynamicTabBarVisibility();
}

void TabbedViewContainer::setTabActivity(int index , bool activity)
{
    const QPalette& palette = _tabBar->palette();
    KColorScheme colorScheme(palette.currentColorGroup());
    const QColor colorSchemeActive = colorScheme.foreground(KColorScheme::ActiveText).color();    
    
    const QColor normalColor = palette.text().color();
    const QColor activityColor = KColorUtils::mix(normalColor,colorSchemeActive); 
    
    QColor color = activity ? activityColor : QColor();

    if ( color != _tabBar->tabTextColor(index) )
        _tabBar->setTabTextColor(index,color);
}

void TabbedViewContainer::updateActivity(ViewProperties* item)
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

void TabbedViewContainer::updateTitle(ViewProperties* item)
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
void TabbedViewContainer::updateIcon(ViewProperties* item)
{
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
    _containerWidget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(_containerWidget);

    _stackWidget = new QStackedWidget(_containerWidget);

    searchBar()->setParent(_containerWidget);
    layout->addWidget(searchBar());
    layout->addWidget(_stackWidget);
    layout->setMargin(0);
}
StackedViewContainer::~StackedViewContainer()
{
    if (!_containerWidget.isNull())
        _containerWidget->deleteLater();
}
QWidget* StackedViewContainer::containerWidget() const
{
    return _containerWidget;
}
QWidget* StackedViewContainer::activeView() const
{
    return _stackWidget->currentWidget();
}
void StackedViewContainer::setActiveView(QWidget* view)
{
   _stackWidget->setCurrentWidget(view); 
}
void StackedViewContainer::addViewWidget( QWidget* view , int )
{
    _stackWidget->addWidget(view);
}
void StackedViewContainer::removeViewWidget( QWidget* view )
{
    if (!_stackWidget)
        return;
    const int index = _stackWidget->indexOf(view);

    Q_ASSERT( index != -1);

    _stackWidget->removeWidget(view);
}

ListViewContainer::ListViewContainer(NavigationPosition position,QObject* parent)
    : ViewContainer(position,parent)
{
    _splitter = new QSplitter;
    _listWidget = new ProfileListWidget(_splitter);

    QWidget *contentArea = new QWidget(_splitter);
    QVBoxLayout *layout = new QVBoxLayout(contentArea);
    _stackWidget = new QStackedWidget(contentArea);
    searchBar()->setParent(contentArea);
    layout->addWidget(_stackWidget);
    layout->addWidget(searchBar());
    layout->setMargin(0);

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
    _splitter->addWidget(contentArea);
        
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

void ListViewContainer::addViewWidget( QWidget* view , int )
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
    if (!_stackWidget)
        return;
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

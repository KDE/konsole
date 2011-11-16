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
//krazy:excludeall=qclasses

// Own
#include "ViewContainer.h"

// Qt
#include <QtCore/QHash>
#include <QtGui/QLabel>
#include <QtGui/QBrush>
#include <QtGui/QListWidget>
#include <QtGui/QSplitter>
#include <QtGui/QStackedWidget>
#include <QtGui/QTabBar>
#include <QtGui/QToolButton>
#include <QtGui/QWidgetAction>
#include <QtGui/QDrag>
#include <QtGui/QDragMoveEvent>
#include <QtCore/QMimeData>

// KDE
#include <KColorDialog>
#include <KColorScheme>
#include <KColorUtils>
#include <KDebug>
#include <KConfigGroup>
#include <KLocale>
#include <KMenu>
#include <KColorCollection>

// Konsole
#include "IncrementalSearchBar.h"
#include "ViewProperties.h"

// TODO Perhaps move everything which is Konsole-specific into different files

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

void ViewContainer::setNavigationTextMode(bool mode)
{
    navigationTextModeChanged(mode);
}

void ViewContainer::addView(QWidget* view , ViewProperties* item, int index)
{
    if (index == -1)
        _views.append(view);
    else
        _views.insert(index,view);

    _navigation[view] = item;

    connect( view , SIGNAL(destroyed(QObject*)) , this , SLOT(viewDestroyed(QObject*)) );

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

    disconnect( view , SIGNAL(destroyed(QObject*)) , this , SLOT(viewDestroyed(QObject*)) );

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
        _searchBar = new IncrementalSearchBar(0);
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

void ViewContainer::activateLastView()
{
    setActiveView(_views.at(_views.count()-1));
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
    setStyleSheet("QTabBar::tab { min-width: 2em; max-width: 25em }");
    setElideMode(Qt::ElideLeft);
}
void ViewContainerTabBar::setDropIndicator(int index, bool drawDisabled)
{
    if (!parentWidget() || _dropIndicatorIndex == index)
        return;

    _dropIndicatorIndex = index;
    const int ARROW_SIZE = 32;
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
   , _contextMenuTabIndex(0)
{
    _containerWidget = new QWidget;
    _stackWidget = new QStackedWidget();

    // The tab bar
    _tabBar = new ViewContainerTabBar(_containerWidget,this);
    _tabBar->setDrawBase(true);
    _tabBar->setDocumentMode(true);
    _tabBar->setFocusPolicy(Qt::NoFocus);
    _tabBar->setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);

    connect( _tabBar , SIGNAL(currentChanged(int)) , this , SLOT(currentTabChanged(int)) );
    connect( _tabBar , SIGNAL(tabDoubleClicked(int)) , this , SLOT(tabDoubleClicked(int)) );
    connect( _tabBar , SIGNAL(newTabRequest()) , this , SIGNAL(newViewRequest()) );
    connect( _tabBar , SIGNAL(wheelDelta(int)) , this , SLOT(wheelScrolled(int)) );
    connect( _tabBar , SIGNAL(initiateDrag(int)) , this , SLOT(startTabDrag(int)) );
    connect( _tabBar, SIGNAL(contextMenu(int,QPoint)), this,
            SLOT(openTabContextMenu(int,QPoint)) );

    // The context menu of tab bar
    _contextPopupMenu = new KMenu(_tabBar);

    _contextPopupMenu->addAction(KIcon("tab-detach"),
        i18nc("@action:inmenu", "&Detach Tab"), this,
        SLOT(tabContextMenuDetachTab()));

    _contextPopupMenu->addAction(KIcon(),
        i18nc("@action:inmenu", "&Rename Tab..."), this,
        SLOT(tabContextMenuRenameTab()));

    _contextPopupMenu->addAction(KIcon("tab-close"),
        i18nc("@action:inmenu", "&Close Tab"), this,
        SLOT(tabContextMenuCloseTab()));

    // The 'new tab' and 'close tab' button
    _newTabButton = new QToolButton(_containerWidget);
    _newTabButton->setIcon(KIcon("tab-new"));
    _newTabButton->adjustSize();

    _closeTabButton = new QToolButton(_containerWidget);
    _closeTabButton->setIcon(KIcon("tab-close"));
    _closeTabButton->adjustSize();

    // 'new tab' button is initially hidden. It will be shown when setFeatures()
    // is called with the QuickNewView flag enabled. The 'close tab' is the same.
    _newTabButton->setHidden(true);
    _closeTabButton->setHidden(true);

    connect( _newTabButton , SIGNAL(clicked()) , this , SIGNAL(newViewRequest()) );
    connect( _closeTabButton , SIGNAL(clicked()) , this , SLOT(closeCurrentTab()) );

    // Combin tab bar and 'new/close tab' buttons
    _tabBarLayout = new QHBoxLayout;
    _tabBarLayout->setSpacing(0);
    _tabBarLayout->setContentsMargins(0, 0, 0, 0);
    _tabBarLayout->addWidget(_newTabButton);
    _tabBarLayout->addWidget(_tabBar);
    _tabBarLayout->addWidget(_closeTabButton);

    _layout = new TabbedViewContainerLayout;
    _layout->setSpacing(0);
    _layout->setContentsMargins(0, 0, 0, 0);

    // The terminal area
    _layout->addWidget(_stackWidget);

    // The search bar
    searchBar()->setParent(_containerWidget);

    // The overall layout
    if ( position == NavigationPositionTop )
    {
        _layout->insertLayout(0,_tabBarLayout);
        _layout->insertWidget(-1,searchBar());
        _tabBar->setShape(QTabBar::RoundedNorth);
    }
    else if ( position == NavigationPositionBottom )
    {
        _layout->insertWidget(-1,searchBar());
        _layout->insertLayout(-1,_tabBarLayout);
        _tabBar->setShape(QTabBar::RoundedSouth);
    }
    else
    {
        Q_ASSERT(false); // position not supported
    }

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
    updateVisibilityOfQuickButtons();
}

void TabbedViewContainer::closeCurrentTab()
{
    if (_stackWidget->currentIndex() != -1)
    {
        emit closeTab(this, _stackWidget->widget(_stackWidget->currentIndex()));
    }
}

void TabbedViewContainer::updateVisibilityOfQuickButtons()
{
    const bool tabBarHidden = _tabBar->isHidden();
    _newTabButton->setVisible(!tabBarHidden && (features() & QuickNewView));
    _closeTabButton->setVisible(!tabBarHidden && (features() & QuickCloseView));
}

void TabbedViewContainer::setTabBarVisible(bool visible)
{
    _tabBar->setVisible(visible);
    updateVisibilityOfQuickButtons();
}
QList<ViewContainer::NavigationPosition> TabbedViewContainer::supportedNavigationPositions() const
{
    return QList<NavigationPosition>() << NavigationPositionTop << NavigationPositionBottom;
}
void TabbedViewContainer::navigationPositionChanged(NavigationPosition position)
{
    // this method assumes that there are only two items
    // in the layout
    Q_ASSERT( _layout->count() == 3 );

    // index of stack widget in the layout when tab bar is at the bottom
    const int StackIndexWithTabBottom = 0;

    if ( position == NavigationPositionTop 
            && _layout->indexOf(_stackWidget) == StackIndexWithTabBottom )
    {
        _layout->removeItem(_tabBarLayout);
        _layout->removeWidget(searchBar());

        _layout->insertLayout(0,_tabBarLayout);
        _layout->insertWidget(-1,searchBar());
        _tabBar->setShape(QTabBar::RoundedNorth);
    }
    else if ( position == NavigationPositionBottom 
            && _layout->indexOf(_stackWidget) != StackIndexWithTabBottom )
    {
        _layout->removeItem(_tabBarLayout);
        _layout->removeWidget(searchBar());

        _layout->insertWidget(-1,searchBar());
        _layout->insertLayout(-1,_tabBarLayout);
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

void TabbedViewContainer::navigationTextModeChanged(bool useTextWidth)
{
    if (useTextWidth)
    {
        _tabBar->setStyleSheet("QTabBar::tab { }");
        _tabBar->setExpanding(false);
        _tabBar->setElideMode(Qt::ElideNone);
    }
    else
    {
        _tabBar->setStyleSheet("QTabBar::tab { min-width: 2em; max-width: 25em }");
        _tabBar->setExpanding(true);
        _tabBar->setElideMode(Qt::ElideLeft);
    }
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

    // offset the tab position so the tab will follow the cursor exactly
    // where it was clicked (as opposed to centering on the origin of the pixmap)
    QPoint mappedPos = _tabBar->mapFromGlobal(QCursor::pos());
    mappedPos.rx() -= tabRect.x();

    drag->setHotSpot(mappedPos);

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

void TabbedViewContainer::tabDoubleClicked(int index)
{
    renameTab(index);
}

void TabbedViewContainer::renameTab(int index)
{
    viewProperties(views()[index])->rename();
}

void TabbedViewContainer::openTabContextMenu(int index, const QPoint& pos)
{
    _contextMenuTabIndex = index;

    // Enable 'Detach Tab' menu item only if there is more than 1 tab
    QList<QAction *> menuActions = _contextPopupMenu->actions();
    if (_tabBar->count() == 1)
        menuActions.first()->setEnabled(false);
    else
        menuActions.first()->setEnabled(true);

    _contextPopupMenu->exec(pos);
}

void TabbedViewContainer::tabContextMenuCloseTab()
{
    _tabBar->setCurrentIndex(_contextMenuTabIndex);// Required for this to work
    emit closeTab(this, _stackWidget->widget(_contextMenuTabIndex));
}

void TabbedViewContainer::tabContextMenuDetachTab()
{
    emit detachTab(this, _stackWidget->widget(_contextMenuTabIndex));
}

void TabbedViewContainer::tabContextMenuRenameTab()
{
    renameTab(_contextMenuTabIndex);
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
    connect( item , SIGNAL(iconChanged(ViewProperties*)) , this , 
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
    QListIterator<QWidget*> iter(widgetsForItem(item));
    while ( iter.hasNext() )
    {
        const int index = _stackWidget->indexOf( iter.next() );
        QString tabText = item->title();

        _tabBar->setTabToolTip( index , tabText );

        // To avoid having & replaced with _ (shortcut indicator)
        tabText.replace('&', "&&");
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
    QVBoxLayout* layout = new QVBoxLayout(_containerWidget);

    _stackWidget = new QStackedWidget(_containerWidget);

    searchBar()->setParent(_containerWidget);
    layout->addWidget(searchBar());
    layout->addWidget(_stackWidget);
    layout->setContentsMargins(0, 0, 0, 0);
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
    Q_UNUSED(index);

    _stackWidget->removeWidget(view);
}

#include "ViewContainer.moc"

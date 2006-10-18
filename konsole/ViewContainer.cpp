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


// Konsole
#include "NavigationItem.h"

// Terminal Session Navigation
#include "TESession.h"

NavigationItem::NavigationItem()
{
    _title = "monkey";
}

QString NavigationItem::title() const
{
    return _title;
}
QIcon NavigationItem::icon() const
{
    return _icon;
}
void NavigationItem::setTitle( const QString& title )
{
    _title = title;
}
void NavigationItem::setIcon( const QIcon& icon )
{
    _icon = icon;
}

QList<QAction*> NavigationItem::contextMenuActions( QList<QAction*> viewActions ) const
{
    return viewActions;
}

SessionNavigationItem::SessionNavigationItem(TESession* session)
    : _session(session)
{

    setTitle( _session->displayTitle() );   
    setIcon( QIcon(_session->IconName()) ); 

    connect( _session , SIGNAL( updateTitle() ) , this , SLOT( updateTitle() ) );
}

void SessionNavigationItem::updateTitle()
{
    setTitle( _session->displayTitle() );

    emit titleChanged(this);
}

#include "NavigationItem.moc"

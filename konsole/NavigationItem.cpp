

// Konsole
#include "NavigationItem.h"

// Terminal Session Navigation
    // KDE
    #include <kaction.h>
    #include <kdebug.h>
    #include <kinputdialog.h>
    #include <klocale.h>
    #include <kiconloader.h>
    
    // Konsole
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

QList<QAction*> NavigationItem::contextMenuActions( QList<QAction*> viewActions ) 
{
    return viewActions;
}

SessionNavigationItem::SessionNavigationItem(TESession* session)
    : _session(session)
{

    setTitle( _session->displayTitle() );   
    setIcon( SmallIconSet(_session->IconName()) ); 

    connect( _session , SIGNAL( updateTitle() ) , this , SLOT( updateTitle() ) );
}

QList<QAction*> SessionNavigationItem::contextMenuActions( QList<QAction*> viewActions ) 
{
    QList<QAction*> actionList;

    QAction* monitorSeparator = new QAction(this);
    monitorSeparator->setSeparator(true);
    QAction* viewSeparator = new QAction(this);
    viewSeparator->setSeparator(true);
    QAction* closeSeparator = new QAction(this);
    closeSeparator->setSeparator(true);

    KAction* renameAction =  new KAction(i18n("&Rename Session"),0/*parent*/,"rename_session");
    connect( renameAction , SIGNAL(triggered()) , this , SLOT( renameSession() ) );
   // actionList << new QAction("&Detach Session...",this);
    actionList << renameAction;
    actionList << monitorSeparator;
   // actionList << new KToggleAction( KIcon("activity") i18n("Monitor for &Activity"),this);
   // actionList << new KToggleAction("Monitor for &Silence",this);
   // actionList << new KToggleAction("Send &Input to All Sessions",this);
   // actionList << viewSeparator;
    actionList << viewActions;
   // actionList << closeSeparator;
   // actionList << new QAction("&Close Session",this);

    return actionList;
}

void SessionNavigationItem::renameSession()
{
    QString newTitle = _session->Title();

    bool success;

    newTitle = KInputDialog::getText( i18n("Rename Session"), i18n("Session Name:") , 
                                   newTitle , 
                                   &success );

    if ( success )
    {
        _session->setTitle(newTitle);

        updateTitle();
    }
}

void SessionNavigationItem::updateTitle()
{
    kDebug() << " session title update " << endl;
    kDebug() << " session icon update " << endl;

    setTitle( _session->displayTitle() );
    setIcon( SmallIconSet(_session->IconName()) );
    
    emit titleChanged(this);
    emit iconChanged(this);
}

#include "NavigationItem.moc"



// Konsole
#include "NavigationItem.h"

// Terminal Session Navigation
    // KDE
    #include <kaction.h>
    #include <kdebug.h>
    #include <kinputdialog.h>
    #include <klocale.h>
    #include <kicon.h>
    #include <kiconloader.h>
    #include <ktoggleaction.h>

    // Konsole
    #include "TESession.h"

NavigationItem::NavigationItem()
{
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
    , _collection(this)
{
    setTitle( _session->displayTitle() );   
    setIcon( KIcon(_session->iconName()) ); 

    connect( _session , SIGNAL( updateTitle() ) , this , SLOT( updateTitle() ) );
    connect( _session , SIGNAL( notifySessionState(TESession*,int) ) , this , 
            SLOT( sessionStateChange(TESession*,int) ) );

    //setup context menu
    buildContextMenuActions();
}


QList<QAction*> SessionNavigationItem::contextMenuActions( QList<QAction*> viewActions )
{
    _actionList.contains(_viewSeparator);

    QList<QAction*> actions = _actionList;

    int index = actions.indexOf(_viewSeparator);
    for (int i=0;i<viewActions.count();i++)
        actions.insert(index+i+1,viewActions[i]);
    
    return actions;
}

void  SessionNavigationItem::buildContextMenuActions() 
{
    QAction* monitorSeparator = new QAction(this);
    monitorSeparator->setSeparator(true);
    _viewSeparator = new QAction(this);
    _viewSeparator->setSeparator(true);
    QAction* closeSeparator = new QAction(this);
    closeSeparator->setSeparator(true);

    KAction* renameAction =  new KAction(i18n("&Rename Session"),&_collection,"rename_session");
    connect( renameAction , SIGNAL(triggered()) , this , SLOT( renameSession() ) );
   // _actionList << new QAction("&Detach Session...",this);
    _actionList << renameAction;
    _actionList << monitorSeparator;
    
    KToggleAction* monitorActivityAction = new KToggleAction( KIcon("activity") , 
                                                i18n("Monitor for &Activity"),
                                                &_collection , "monitor_activity");

    monitorActivityAction->setCheckedState( KGuiItem(i18n("Stop Monitoring for &Activity")));
    
    connect( monitorActivityAction , SIGNAL(toggled(bool)) , this , SLOT(toggleMonitorActivity(bool)));
    
    _actionList << monitorActivityAction;
    
    KToggleAction* monitorSilenceAction = new KToggleAction( KIcon("silence") , i18n("Monitor for &Silence"),
                                        &_collection , "monitor_silence" );
    monitorSilenceAction->setCheckedState( KGuiItem(i18n("Stop Monitoring for &Silence")));
    
    connect( monitorSilenceAction , SIGNAL(toggled(bool)) , this , SLOT(toggleMonitorSilence(bool)));
    _actionList << monitorSilenceAction;
    
   // _actionList << new KToggleAction("Send &Input to All Sessions",this);
    _actionList << _viewSeparator;
   // _actionList << viewActions;
    _actionList << closeSeparator;

    KAction* closeAction = new KAction( i18n("&Close Session") ,&_collection , "close_session");
    connect( closeAction , SIGNAL(triggered()) , this , SLOT( closeSession() ) );
    _actionList << closeAction;
}

void SessionNavigationItem::toggleMonitorActivity(bool monitor)
{
    _session->setMonitorActivity(monitor);    
}
void SessionNavigationItem::toggleMonitorSilence(bool monitor)
{
    _session->setMonitorSilence(monitor);
}
void SessionNavigationItem::closeSession()
{
    _session->closeSession();
}

void SessionNavigationItem::sessionStateChange(TESession* /* session */ , int state)
{
    QString stateIconName;

    switch ( state )
    {
        case NOTIFYNORMAL:
            if ( _session->isMasterMode() )
                stateIconName = "remote";
            else
                stateIconName = _session->iconName();
            break;
        case NOTIFYBELL:
            stateIconName = "bell";
            break;
        case NOTIFYACTIVITY:
            stateIconName = "activity";
            break;
        case NOTIFYSILENCE:
            stateIconName = "silence";
            break;
    }

    if ( stateIconName != _stateIconName )
    {
        _stateIconName = stateIconName;
        
        QPixmap iconPixmap = KGlobal::instance()->iconLoader()->loadIcon(stateIconName,K3Icon::Small,0,
                        K3Icon::DefaultState,0L,true);

        QIcon iconSet;
        iconSet.addPixmap(iconPixmap,QIcon::Normal);

        setIcon(iconSet);
        emit iconChanged(this);
    }
}

void SessionNavigationItem::renameSession()
{
    QString newTitle = _session->title();

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
    setIcon( KIcon(_session->iconName()) );
    
    emit titleChanged(this);
    emit iconChanged(this);
}

#include "NavigationItem.moc"

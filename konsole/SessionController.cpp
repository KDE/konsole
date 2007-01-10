
// KDE
#include <KAction>
#include <KIcon>
#include <KLocale>
#include <KToggleAction>
#include <kdebug.h>

// Konsole
#include "Filter.h"
#include "TESession.h"
#include "TEWidget.h"
#include "SessionController.h"

KIcon SessionController::_activityIcon;
KIcon SessionController::_silenceIcon;

SessionController::SessionController(TESession* session , TEWidget* view, QObject* parent)
    : ViewProperties(parent)
    , KXMLGUIClient()
    , _session(session)
    , _view(view)
    , _previousState(-1)
{
    setXMLFile("sessionui.rc");
    setupActions();

    sessionTitleChanged();
    
    view->installEventFilter(this);

    // destroy session controller if either the view or the session are destroyed
    connect( _session , SIGNAL(destroyed()) , this , SLOT(deleteLater()) );
    connect( _view , SIGNAL(destroyed()) , this , SLOT(deleteLater()) );

    // listen to activity / silence notifications from session
    connect( _session , SIGNAL(notifySessionState(TESession*,int)) , this ,
            SLOT(sessionStateChanged(TESession*,int) ));

    // list to title and icon changes
    connect( _session , SIGNAL(updateTitle()) , this , SLOT(sessionTitleChanged()) );

    // install filter on the view to highlight URLs
    _viewUrlFilter = new UrlFilter();
    view->filterChain()->addFilter( _viewUrlFilter );
}

SessionController::~SessionController()
{   
    // delete filters
    delete _viewUrlFilter;
}

bool SessionController::eventFilter(QObject* watched , QEvent* event)
{
    if ( watched == _view )
    {
        if ( event->type() == QEvent::FocusIn )
        {
            emit focused(this);
        }
    }

    return false;
}

void SessionController::setupActions()
{
    QAction* action = 0;
    KActionCollection* collection = actionCollection();

    // Close Session
    action = collection->addAction("close-session"); 
    action->setIcon( KIcon("fileclose") );
    action->setText( i18n("&Close Tab") );
    connect( action , SIGNAL(triggered()) , this , SLOT(closeSession()) );
    
    // Copy and Paste
    action = collection->addAction("copy");
    action->setIcon( KIcon("editcopy") );
    action->setText( i18n("&Copy") );
    connect( action , SIGNAL(triggered()) , this , SLOT(copy()) );
    
    action = collection->addAction("paste");
    action->setIcon( KIcon("editpaste") );
    action->setText( i18n("&Paste") );
    connect( action , SIGNAL(triggered()) , this , SLOT(paste()) );

    // Clear and Clear+Reset
    action = collection->addAction("clear");
    action->setText( i18n("C&lear Display") );
    connect( action , SIGNAL(triggered()) , this , SLOT(clear()) );
    
    action = collection->addAction("clear-and-reset");
    action->setText( i18n("Clear and Reset") );
    connect( action , SIGNAL(triggered()) , this , SLOT(clearAndReset()) );

    // Monitor
    action = collection->addAction("monitor-activity");
    action->setText( i18n("Monitor for &Activity") );
    connect( action , SIGNAL(toggled(bool)) , this , SLOT(monitorActivity(bool)) );

    action = collection->addAction("monitor-silence");
    action->setText( i18n("Monitor for &Silence") );
    connect( action , SIGNAL(toggled(bool)) , this , SLOT(monitorSilence(bool)) );

    // History
    action = collection->addAction("search-history");
    action->setIcon( KIcon("find") );
    action->setText( i18n("Search History") );
    connect( action , SIGNAL(triggered()) , this , SLOT(searchHistory()) );
    
    action = collection->addAction("find-next");
    action->setIcon( KIcon("next") );
    action->setText( i18n("Find Next") );
    connect( action , SIGNAL(triggered()) , this , SLOT(findNextInHistory()) );
    
    action = collection->addAction("find-previous");
    action->setIcon( KIcon("previous") );
    action->setText( i18n("Find Previous") );
    connect( action , SIGNAL(triggered()) , this , SLOT(findPreviousInHistory()) );
    
    action = collection->addAction("save-history");
    action->setText( i18n("Save History") );
    connect( action , SIGNAL(triggered()) , this , SLOT(saveHistory()) );
    
    action = collection->addAction("clear-history");
    action->setText( i18n("Clear History") );
    connect( action , SIGNAL(triggered()) , this , SLOT(clearHistory()) );
}

void SessionController::closeSession()
{
    _session->closeSession();
}

void SessionController::copy()
{
    _view->copyClipboard(); 
}

void SessionController::paste()
{
    _view->pasteClipboard();
}

void SessionController::clear()
{
    TEmulation* emulation = _session->getEmulation();

    emulation->clearEntireScreen();
    emulation->clearSelection();
}
void SessionController::clearAndReset()
{
    TEmulation* emulation = _session->getEmulation();

    emulation->reset();
    emulation->clearSelection();
}
void SessionController::searchHistory()
{
}
void SessionController::findNextInHistory()
{
}
void SessionController::findPreviousInHistory()
{
}
void SessionController::saveHistory()
{
}
void SessionController::clearHistory()
{
    _session->clearHistory();
}
void SessionController::monitorActivity(bool monitor)
{
    _session->setMonitorActivity(monitor);
}
void SessionController::monitorSilence(bool monitor)
{
    _session->setMonitorSilence(monitor); 
}
void SessionController::sessionTitleChanged()
{
        if ( _sessionIconName != _session->iconName() )
        { 
            _sessionIconName = _session->iconName();
            _sessionIcon = KIcon( _sessionIconName );
            setIcon( _sessionIcon );
        }
           
       setTitle( _session->displayTitle() ); 
}
void SessionController::sessionStateChanged(TESession* /*session*/ , int state)
{
    //TODO - Share icons across sessions ( possible using a static QHash<QString,QIcon> variable 
    // to create a cache of icons mapped from icon names? )

    if ( state == _previousState )
        return;

    _previousState = state;

    if ( state == NOTIFYACTIVITY )
    {
        if (_activityIcon.isNull())
        {
            _activityIcon = KIcon("activity");
        }

        setIcon(_activityIcon);
    } 
    else if ( state == NOTIFYSILENCE )
    {
        if (_silenceIcon.isNull())
        {
            _silenceIcon = KIcon("silence");
        }

        setIcon(_silenceIcon);
    }
    else if ( state == NOTIFYNORMAL )
    {
        if ( _sessionIconName != _session->iconName() )
        { 
            _sessionIconName = _session->iconName();
            _sessionIcon = KIcon( _sessionIconName );
        }
            
        setIcon( _sessionIcon );
    }
}

#include "SessionController.moc"

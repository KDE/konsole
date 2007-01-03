
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
    KAction* action = 0;
    KActionCollection* collection = actionCollection();

    // Close Session
    action = new KAction( KIcon("fileclose"), i18n("&Close Tab") , collection , "close-session" );
    connect( action , SIGNAL(triggered()) , this , SLOT(closeSession()) );
    
    // Copy and Paste
    action = new KAction( KIcon("editcopy") , i18n("&Copy") , collection , "copy" );
    connect( action , SIGNAL(triggered()) , this , SLOT(copy()) );
    
    action = new KAction( KIcon("editpaste") , i18n("&Paste") , collection , "paste" );
    connect( action , SIGNAL(triggered()) , this , SLOT(paste()) );

    // Clear and Clear+Reset
    action = new KAction( i18n("C&lear Display") , collection , "clear" );
    connect( action , SIGNAL(triggered()) , this , SLOT(clear()) );
    
    action = new KAction( i18n("Clear and Reset") , collection , "clear-and-reset" );
    connect( action , SIGNAL(triggered()) , this , SLOT(clearAndReset()) );

    // Monitor
    action = new KToggleAction( i18n("Monitor for &Activity") , collection , "monitor-activity" );
    connect( action , SIGNAL(toggled(bool)) , this , SLOT(monitorActivity(bool)) );

    action = new KToggleAction( i18n("Monitor for &Silence") , collection , "monitor-silence" );
    connect( action , SIGNAL(toggled(bool)) , this , SLOT(monitorSilence(bool)) );

    // History
    action = new KAction( KIcon("find") , i18n("Search History") , collection , "search-history" );
    connect( action , SIGNAL(triggered()) , this , SLOT(searchHistory()) );
    
    action = new KAction( KIcon("next") , i18n("Find Next") , collection , "find-next" );
    connect( action , SIGNAL(triggered()) , this , SLOT(findNextInHistory()) );
    
    action = new KAction( KIcon("previous") , i18n("Find Previous") , collection , "find-previous" );
    connect( action , SIGNAL(triggered()) , this , SLOT(findPreviousInHistory()) );
    
    action = new KAction( i18n("Save History") , collection , "save-history" );
    connect( action , SIGNAL(triggered()) , this , SLOT(saveHistory()) );
    
    action = new KAction( i18n("Clear History") , collection , "clear-history" );
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


// KDE
#include <KAction>
#include <KLocale>
#include <kdebug.h>

// Konsole
#include "TESession.h"
#include "TEWidget.h"
#include "SessionController.h"

SessionController::SessionController(TESession* session , TEWidget* view, QObject* parent)
    : QObject(parent)
        /*ViewProperties(parent) */
    , KXMLGUIClient()
    , _session(session)
    , _view(view)
{
    setupActions();

    view->installEventFilter(this);
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

    setXMLFile("konsoleui.rc");

    // Close Session
    action = new KAction( i18n("&Close Tab") , actionCollection() , "close-session" );
    connect( action , SIGNAL(triggered()) , this , SLOT(closeSession()) );

    // Copy and Paste
    action = new KAction( i18n("&Copy") , actionCollection() , "copy" );
    connect( action , SIGNAL(triggered()) , this , SLOT(copy()) );
    
    action = new KAction( i18n("&Paste") , actionCollection() , "paste" );
    connect( action , SIGNAL(triggered()) , this , SLOT(paste()) );

    // Clear and Clear+Reset
    action = new KAction( i18n("C&lear Display") , actionCollection() , "clear" );
    connect( action , SIGNAL(triggered()) , this , SLOT(clear()) );
    
    action = new KAction( i18n("Clear and Reset") , actionCollection() , "clear-and-reset" );
    connect( action , SIGNAL(triggered()) , this , SLOT(clearAndReset()) );


    // History
    action = new KAction( i18n("Search History") , actionCollection() , "search-history" );
    connect( action , SIGNAL(triggered()) , this , SLOT(searchHistory()) );
    
    action = new KAction( i18n("Find Next") , actionCollection() , "find-next" );
    connect( action , SIGNAL(triggered()) , this , SLOT(findNextInHistory()) );
    
    action = new KAction( i18n("Find Previous") , actionCollection() , "find-previous" );
    connect( action , SIGNAL(triggered()) , this , SLOT(findPreviousInHistory()) );
    
    action = new KAction( i18n("Save History") , actionCollection() , "save-history" );
    connect( action , SIGNAL(triggered()) , this , SLOT(saveHistory()) );
    
    action = new KAction( i18n("Clear History") , actionCollection() , "clear-history" );
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

#include "SessionController.moc"


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
    KActionCollection* collection = actionCollection();

    setXMLFile("konsoleui.rc");
    
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

#include "SessionController.moc"

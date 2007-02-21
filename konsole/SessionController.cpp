
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
#include "ProcessInfo.h"

// for SaveHistoryTask
#include <KUrl>
#include <KFileDialog>
#include <KJob>
#include <KMessageBox>
#include "TerminalCharacterDecoder.h"

// used an old-style include below because #include <KIO/Job> does not work
// at the time of writing
#include <kio/job.h>


KIcon SessionController::_activityIcon;
KIcon SessionController::_silenceIcon;

SessionController::SessionController(TESession* session , TEWidget* view, QObject* parent)
    : ViewProperties(parent)
    , KXMLGUIClient()
    , _session(session)
    , _view(view)
    , _previousState(-1)
{
    // handle user interface related to session (menus etc.)
    setXMLFile("sessionui.rc");
    setupActions();

    setIdentifier(_session->sessionId());
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
    view->filterChain()->addFilter( new UrlFilter() );
}

SessionController::~SessionController()
{   
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
    KToggleAction* toggleAction = 0;
    toggleAction = new KToggleAction(i18n("Monitor for &Activity"),this);  
    action = collection->addAction("monitor-activity",toggleAction);
    connect( action , SIGNAL(toggled(bool)) , this , SLOT(monitorActivity(bool)) );

    toggleAction = new KToggleAction(i18n("Monitor for &Silence"),this);  
    action = collection->addAction("monitor-silence",toggleAction);
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

    // debugging tools
    action = collection->addAction("debug-process");
    action->setText( "Get Foreground Process" );
    connect( action , SIGNAL(triggered()) , this , SLOT(debugProcess()) );
}

void SessionController::debugProcess()
{
    // testing facility to retrieve process information about 
    // currently active process in the shell
    ProcessInfo* sessionProcess = ProcessInfo::newInstance(_session->sessionPid());
    sessionProcess->update();

    bool ok = false;
    int fpid = sessionProcess->foregroundPid(&ok);

    if ( ok )
    {
        ProcessInfo* fp = ProcessInfo::newInstance(fpid);
        fp->update();

        QString name = fp->name(&ok);

        if ( ok )
        {
            _session->setTitle(name);
            sessionTitleChanged();
        }

        QString currentDir = fp->currentDir(&ok);

        if ( ok )
        {
            qDebug() << currentDir;
        }
        else
        {
            qDebug() << "could not read current dir of foreground process";
        }

        delete fp;
    }
    delete sessionProcess;
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
    SessionTask* task = new SaveHistoryTask();
    task->setAutoDelete(true);
    task->addSession( _session );   
    task->execute();
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
         
       //TODO - use _session->displayTitle() here. 
       setTitle( _session->title() ); 
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

SessionTask::SessionTask()
    : _autoDelete(false)
{
}
void SessionTask::setAutoDelete(bool enable)
{
    _autoDelete = enable;
}
bool SessionTask::autoDelete() const
{
    return _autoDelete;
}
void SessionTask::addSession(TESession* session)
{
    _sessions << session;
}
QList<SessionPtr> SessionTask::sessions() const
{
    return _sessions;
}

SaveHistoryTask::SaveHistoryTask()
    : SessionTask()
{
}
SaveHistoryTask::~SaveHistoryTask()
{
}

void SaveHistoryTask::execute()
{
    QListIterator<SessionPtr> iter(sessions());

    // TODO - prompt the user if the file already exists, currently existing files
    //        are always overwritten

    // TODO - think about the UI when saving multiple history sessions, if there are more than two or
    //        three then providing a URL for each one will be tedious

    // TODO - show a warning ( preferably passive ) if saving the history output fails
    //
    
     KFileDialog* dialog = new KFileDialog( QString(":konsole") /* check this */, 
                                               QString() , 0 /* no parent widget */);
     QStringList mimeTypes;
     mimeTypes << "text/plain";
     mimeTypes << "text/html";
     dialog->setMimeFilter(mimeTypes,"text/plain");

     // iterate over each session in the task and display a dialog to allow the user to choose where
     // to save that session's history.
     // then start a KIO job to transfer the data from the history to the chosen URL
    while ( iter.hasNext() )
    {
        SessionPtr session = iter.next();

        dialog->setCaption( i18n("Save Output from %1",session->title()) );
            
        int result = dialog->exec();

        if ( result != QDialog::Accepted )
            continue;

        KUrl url = dialog->selectedUrl(); 

        if ( !url.isValid() )
        { // UI:  Can we make this friendlier?
            KMessageBox::sorry( 0 , i18n("%1 is an invalid URL, the output could not be saved.") );
            continue;
        }

        KIO::TransferJob* job = KIO::put( url, 
                                          -1,   // no special permissions
                                          true, // overwrite existing files
                                          false,// do not resume an existing transfer
                                          !url.isLocalFile() // show progress information only for remote
                                                             // URLs
                                                             //
                                                             // a better solution would be to show progress
                                                             // information after a certain period of time
                                                             // instead, since the overall speed of transfer
                                                             // depends on factors other than just the protocol
                                                             // used
                                        );


        SaveJob jobInfo;
        jobInfo.session = session;
        jobInfo.lastLineFetched = -1;  // when each request for data comes in from the KIO subsystem
                                       // lastLineFetched is used to keep track of how much of the history
                                       // has already been sent, and where the next request should continue
                                       // from

        if ( dialog->currentMimeFilter() == "text/html" )
           jobInfo.decoder = new HTMLDecoder();
        else
           jobInfo.decoder = new PlainTextDecoder();

        _jobSession.insert(job,jobInfo);

        connect( job , SIGNAL(dataReq(KIO::Job*,QByteArray&)),
                 this, SLOT(jobDataRequested(KIO::Job*,QByteArray&)) );
        connect( job , SIGNAL(result(KJob*)),
                 this, SLOT(jobResult(KJob*)) );
    }

    dialog->deleteLater();   
}
void SaveHistoryTask::jobDataRequested(KIO::Job* job , QByteArray& data)
{
    // TODO - Report progress information for the job

    // PERFORMANCE:  Do some tests and tweak this value to get faster saving
    const int LINES_PER_REQUEST = 500;

    SaveJob& info = _jobSession[job];   

    // transfer LINES_PER_REQUEST lines from the session's history to the save location
    if ( info.session )
    {
        int sessionLines = info.session->getEmulation()->lines();

        int copyUpToLine = qMin( info.lastLineFetched + LINES_PER_REQUEST , sessionLines );

        QTextStream stream(&data,QIODevice::ReadWrite);
        info.session->getEmulation()->writeToStream( &stream , info.decoder , info.lastLineFetched+1 , copyUpToLine );

        // if there are still more lines to process after this request then insert a new line character
        // to ensure that the next block of lines begins on a new line       
        //
        // FIXME - There is still an extra new-line at the end of the save data.   
        if ( copyUpToLine < sessionLines )
        {
            stream << '\n';
        }


        info.lastLineFetched = copyUpToLine;
    }
}
void SaveHistoryTask::jobResult(KJob* job)
{
    if ( job->error() )
    {
        KMessageBox::sorry( 0 , i18n("A problem occurred when saving the output.\n%1",job->errorString()) );
    }

    SaveJob& info = _jobSession[job];

    _jobSession.remove(job);
    
    delete info.decoder;
  
    // notify the world that the task is done 
    emit completed();

    if ( autoDelete() )
        deleteLater();
}

void SearchHistoryTask::execute()
{
    // not yet implemented
}

SearchHistoryThread::SearchHistoryThread(SessionPtr session , QObject* parent)
    : QThread(parent)
    , _session(session)
    , _lastLineFetched(-1)
    , _decoder( new PlainTextDecoder() )
{
}

SearchHistoryThread::~SearchHistoryThread()
{
    delete _decoder;
}

void SearchHistoryThread::setRegExp(const QRegExp& expression)
{
    _regExp = expression;
}
QRegExp SearchHistoryThread::regExp() const
{
    return _regExp;
}

void SearchHistoryThread::run() 
{
       
}

#include "SessionController.moc"

/*
    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>

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

// Own
#include "SessionController.h"

// Qt
#include <QtGui/QApplication>
#include <QMenu>

// KDE
#include <KAction>
#include <KDebug>
#include <KIcon>
#include <KInputDialog>
#include <KLocale>
#include <KMenu>
#include <KRun>
#include <kshell.h>
#include <KToggleAction>
#include <KUrl>
#include <KXMLGUIFactory>
#include <KXMLGUIBuilder>
#include <kdebug.h>
#include <kcodecaction.h>
#include <kdeversion.h>

// Konsole
#include "EditProfileDialog.h"
#include "CopyInputDialog.h"
#include "Emulation.h"
#include "Filter.h"
#include "History.h"
#include "IncrementalSearchBar.h"
#include "ScreenWindow.h"
#include "Session.h"
#include "ProfileList.h"
#include "TerminalDisplay.h"
#include "SessionManager.h"

// for SaveHistoryTask
#include <KFileDialog>
#include <KIO/Job>
#include <KJob>
#include <KMessageBox>
#include "TerminalCharacterDecoder.h"


using namespace Konsole;

KIcon SessionController::_activityIcon;
KIcon SessionController::_silenceIcon;
QSet<SessionController*> SessionController::_allControllers;
QPointer<SearchHistoryThread> SearchHistoryTask::_thread;
int SessionController::_lastControllerId;

SessionController::SessionController(Session* session , TerminalDisplay* view, QObject* parent)
    : ViewProperties(parent)
    , KXMLGUIClient()
    , _session(session)
    , _view(view)
    , _copyToGroup(0)
    , _profileList(0)
    , _previousState(-1)
    , _viewUrlFilter(0)
    , _searchFilter(0)
    , _searchToggleAction(0)
    , _findNextAction(0)
    , _findPreviousAction(0)
    , _urlFilterUpdateRequired(false)
    , _codecAction(0)
    , _changeProfileMenu(0)
    , _listenForScreenWindowUpdates(false)
    , _preventClose(false)
{
    _allControllers.insert(this);

    Q_ASSERT( session );
    Q_ASSERT( view );

    // handle user interface related to session (menus etc.)
    if (isKonsolePart())
        setXMLFile("konsole/partui.rc");
    else
        setXMLFile("konsole/sessionui.rc");

    setupActions();
    actionCollection()->addAssociatedWidget(view);
    foreach (QAction* action, actionCollection()->actions())
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    setIdentifier(++_lastControllerId);
    sessionTitleChanged();

    view->installEventFilter(this);

    // listen for session resize requests
    connect( _session , SIGNAL(resizeRequest(const QSize&)) , this ,
            SLOT(sessionResizeRequest(const QSize&)) );

    // listen for popup menu requests
    connect( _view , SIGNAL(configureRequest(QPoint)) , this,
            SLOT(showDisplayContextMenu(QPoint)) );

    // move view to newest output when keystrokes occur
    connect( _view , SIGNAL(keyPressedSignal(QKeyEvent*)) , this ,
            SLOT(trackOutput(QKeyEvent*)) );

    // listen to activity / silence notifications from session
    connect( _session , SIGNAL(stateChanged(int)) , this ,
            SLOT(sessionStateChanged(int) ));
    // listen to title and icon changes
    connect( _session , SIGNAL(titleChanged()) , this , SLOT(sessionTitleChanged()) );

    // listen for color changes
    connect( _session , SIGNAL(changeBackgroundColorRequest(QColor)) , _view , SLOT(setBackgroundColor(QColor)) );
    connect( _session , SIGNAL(changeForegroundColorRequest(QColor)) , _view , SLOT(setForegroundColor(QColor)) );

    // update the title when the session starts
    connect( _session , SIGNAL(started()) , this , SLOT(snapshot()) ); 

    // listen for output changes to set activity flag
    connect( _session->emulation() , SIGNAL(outputChanged()) , this ,
            SLOT(fireActivity()) );
    
    // listen for flow control status changes
    connect( _session , SIGNAL(flowControlEnabledChanged(bool)) , _view ,
        SLOT(setFlowControlWarningEnabled(bool)) );
    _view->setFlowControlWarningEnabled(_session->flowControlEnabled());

    // take a snapshot of the session state every so often when
    // user activity occurs
    //
    // the timer is owned by the session so that it will be destroyed along
    // with the session
    QTimer* activityTimer = new QTimer(_session);
    activityTimer->setSingleShot(true);
    activityTimer->setInterval(2000);
    connect( _view , SIGNAL(keyPressedSignal(QKeyEvent*)) , activityTimer , SLOT(start()) );
    connect( activityTimer , SIGNAL(timeout()) , this , SLOT(snapshot()) );
}

void SessionController::updateSearchFilter()
{
    if ( _searchFilter ) 
    {
        Q_ASSERT( searchBar() && searchBar()->isVisible() );

        _view->processFilters();
    }
}

SessionController::~SessionController()
{
   if ( _view )
      _view->setScreenWindow(0);

   _allControllers.remove(this);
}
void SessionController::trackOutput(QKeyEvent* event)
{
    Q_ASSERT( _view->screenWindow() );

    // jump to the end of the scrollback buffer unless the key pressed
    // is one of the three main modifiers, as these are used to select
    // the selection mode (eg. Ctrl+Alt+<Left Click> for column/block selection)
    switch (event->key())
    {
        case Qt::Key_Shift:
        case Qt::Key_Control:
        case Qt::Key_Alt:
            break;
        default:
            _view->screenWindow()->setTrackOutput(true);
    }
}
void SessionController::requireUrlFilterUpdate()
{
    // this method is called every time the screen window's output changes, so do not
    // do anything expensive here.

    _urlFilterUpdateRequired = true;
}
void SessionController::snapshot()
{
    Q_ASSERT( _session != 0 );

    QString title = _session->getDynamicTitle();    
    title         = title.simplified();

    // crude indicator when the session is broadcasting to others
    if (_copyToGroup && _copyToGroup->sessions().count() > 1)
        title.append('*');

    // apply new title
    if ( !title.isEmpty() )
        _session->setTitle(Session::DisplayedTitleRole,title);
    else
        _session->setTitle(Session::DisplayedTitleRole,_session->title(Session::NameRole));
}

QString SessionController::currentDir() const
{
    return _session->currentWorkingDirectory();
}

KUrl SessionController::url() const
{
    return _session->getUrl();
}

void SessionController::rename()
{
    renameSession();
}

void SessionController::openUrl( const KUrl& url )
{
    // handle local paths
    if ( url.isLocalFile() )
    {
        QString path = url.toLocalFile();
        _session->emulation()->sendText("cd " + KShell::quoteArg(path) + '\r');
    }
    else if ( url.protocol() == "ssh" )
    {
        _session->emulation()->sendText("ssh ");

        if ( url.port() > -1 )
            _session->emulation()->sendText("-p " + QString::number(url.port()) + ' ' );
        if ( url.hasUser() )
            _session->emulation()->sendText(url.user() + '@');
        if ( url.hasHost() )
            _session->emulation()->sendText(url.host() + '\r');
    }
    else if ( url.protocol() == "telnet" )
    {
        _session->emulation()->sendText("telnet ");

        if ( url.hasUser() )
            _session->emulation()->sendText("-l " + url.user() + ' ');
        if ( url.hasHost() )
            _session->emulation()->sendText(url.host() + ' ');
        if ( url.port() > -1 )
            _session->emulation()->sendText(QString::number(url.port()));
         _session->emulation()->sendText("\r");
    }
    else
    {
        //TODO Implement handling for other Url types
        kWarning(1211) << "Unable to open bookmark at url" << url << ", I do not know"
           << " how to handle the protocol " << url.protocol();
    }
}

bool SessionController::eventFilter(QObject* watched , QEvent* event)
{
    if ( watched == _view )
    {
        if ( event->type() == QEvent::FocusIn )
        {
            // notify the world that the view associated with this session has been focused
            // used by the view manager to update the title of the MainWindow widget containing the view
            emit focused(this);

            // when the view is focused, set bell events from the associated session to be delivered
            // by the focused view

            // first, disconnect any other views which are listening for bell signals from the session
            disconnect( _session , SIGNAL(bellRequest(const QString&)) , 0 , 0 );
            // second, connect the newly focused view to listen for the session's bell signal
            connect( _session , SIGNAL(bellRequest(const QString&)) ,
                    _view , SLOT(bell(const QString&)) );
        }
        // when a mouse move is received, create the URL filter and listen for output changes if
        // it has not already been created.  If it already exists, then update only if the output
        // has changed since the last update ( _urlFilterUpdateRequired == true )
        //
        // also check that no mouse buttons are pressed since the URL filter only applies when
        // the mouse is hovering over the view
        if ( event->type() == QEvent::MouseMove &&
            (!_viewUrlFilter || _urlFilterUpdateRequired) &&
            ((QMouseEvent*)event)->buttons() == Qt::NoButton )
        {
            if ( _view->screenWindow() && !_viewUrlFilter )
            {
                connect( _view->screenWindow() , SIGNAL(scrolled(int)) , this ,
                        SLOT(requireUrlFilterUpdate()) );
                connect( _view->screenWindow() , SIGNAL(outputChanged()) , this ,
                         SLOT(requireUrlFilterUpdate()) );

                // install filter on the view to highlight URLs
                _viewUrlFilter = new UrlFilter();
                _view->filterChain()->addFilter( _viewUrlFilter );
            }

            _view->processFilters();
            _urlFilterUpdateRequired = false;
        }
    }

    return false;
}

void SessionController::removeSearchFilter()
{
    if (!_searchFilter)
        return;

    _view->filterChain()->removeFilter(_searchFilter);
    delete _searchFilter;
    _searchFilter = 0;
}

void SessionController::setSearchBar(IncrementalSearchBar* searchBar)
{
    // disconnect the existing search bar
    if ( _searchBar )
    {
        disconnect( this , 0 , _searchBar , 0 );
        disconnect( _searchBar , 0 , this , 0 );
    }

    // remove any existing search filter
    removeSearchFilter();

    // connect new search bar
    _searchBar = searchBar;
    if ( _searchBar )
    {
        connect( _searchBar , SIGNAL(closeClicked()) , this , SLOT(searchClosed()) );
        connect( _searchBar , SIGNAL(findNextClicked()) , this , SLOT(findNextInHistory()) );
        connect( _searchBar , SIGNAL(findPreviousClicked()) , this , SLOT(findPreviousInHistory()) );
        connect( _searchBar , SIGNAL(highlightMatchesToggled(bool)) , this , SLOT(highlightMatches(bool)) );

        // if the search bar was previously active
        // then re-enter search mode
        searchHistory( _searchToggleAction->isChecked() );
    }
}
IncrementalSearchBar* SessionController::searchBar() const
{
    return _searchBar;
}

void SessionController::setShowMenuAction(QAction* action)
{
    actionCollection()->addAction("show-menubar",action);
}

void SessionController::setupActions()
{
    KAction* action = 0;
    KToggleAction* toggleAction = 0;
    KActionCollection* collection = actionCollection();

    // Close Session
    action = collection->addAction("close-session");
    action->setIcon( KIcon("tab-close") );
    action->setText( i18n("&Close Tab") );
    action->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_W) );
    connect( action , SIGNAL(triggered()) , this , SLOT(closeSession()) );

    // Open Browser
    action = collection->addAction("open-browser");
    action->setText( i18n("Open Browser Here") );
    action->setIcon( KIcon("system-file-manager") );
    connect( action, SIGNAL(triggered()), this, SLOT(openBrowser()) );

    // Copy and Paste
    action = collection->addAction("copy");
    action->setIcon( KIcon("edit-copy") );
    action->setText( i18n("&Copy") );
    action->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_C) );
    connect( action , SIGNAL(triggered()) , this , SLOT(copy()) );

    KAction* pasteAction = new KAction( i18n("&Paste") , this );
    pasteAction->setIcon( KIcon("edit-paste") );

    KShortcut pasteShortcut = pasteAction->shortcut();
    pasteShortcut.setPrimary( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_V) );
    pasteShortcut.setAlternate( QKeySequence(Qt::SHIFT+Qt::Key_Insert) );
    pasteAction->setShortcut(pasteShortcut);

    collection->addAction("paste",pasteAction);

    connect( pasteAction , SIGNAL(triggered()) , this , SLOT(paste()) );

    action = collection->addAction("paste-selection");
    action->setText( i18n("Paste Selection") );
    action->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_Insert) );
    connect( action , SIGNAL(triggered()) , this , SLOT(pasteSelection()) );

    // Rename Session
    action = collection->addAction("rename-session");
    action->setText( i18n("&Rename Tab...") );
    action->setShortcut( QKeySequence(Qt::CTRL+Qt::ALT+Qt::Key_S) );
    connect( action , SIGNAL(triggered()) , this , SLOT(renameSession()) );

    // Copy Input To
    action = collection->addAction("copy-input-to");
    action->setText(i18n("Copy Input To..."));
    connect( action , SIGNAL(triggered()) , this , SLOT(copyInputTo()) );

    // Clear and Clear+Reset
    action = collection->addAction("clear");
    action->setText( i18n("C&lear Display") );
    action->setIcon( KIcon("edit-clear") );
    connect( action , SIGNAL(triggered()) , this , SLOT(clear()) );

    action = collection->addAction("clear-and-reset");
    action->setText( i18n("Clear && Reset") );
    action->setIcon( KIcon("edit-clear-history") );
    connect( action , SIGNAL(triggered()) , this , SLOT(clearAndReset()) );

    // Monitor
    toggleAction = new KToggleAction(i18n("Monitor for &Activity"),this);
    toggleAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_A) );
    action = collection->addAction("monitor-activity",toggleAction);
    connect( action , SIGNAL(toggled(bool)) , this , SLOT(monitorActivity(bool)) );

    toggleAction = new KToggleAction(i18n("Monitor for &Silence"),this);
    toggleAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_I) );
    action = collection->addAction("monitor-silence",toggleAction);
    connect( action , SIGNAL(toggled(bool)) , this , SLOT(monitorSilence(bool)) );

    // Character Encoding
    _codecAction = new KCodecAction(i18n("Character Encoding"),this);
    collection->addAction("character-encoding",_codecAction);
    connect( _codecAction->menu() , SIGNAL(aboutToShow()) , this , SLOT(updateCodecAction()) );
    connect( _codecAction , SIGNAL(triggered(QTextCodec*)) , this , SLOT(changeCodec(QTextCodec*)) );

    // Text Size
    action = collection->addAction("increase-text-size");
    action->setText( i18n("Increase Text Size") );
    action->setIcon( KIcon("zoom-in") );
    action->setShortcut( QKeySequence(Qt::CTRL+Qt::Key_Plus) );
    connect( action , SIGNAL(triggered()) , this , SLOT(increaseTextSize()) );

    action = collection->addAction("decrease-text-size");
    action->setText( i18n("Decrease Text Size") );
    action->setIcon( KIcon("zoom-out") );
    action->setShortcut( QKeySequence(Qt::CTRL+Qt::Key_Minus) );
    connect( action , SIGNAL(triggered()) , this , SLOT(decreaseTextSize()) );

    // Scrollback
    _searchToggleAction = new KAction(i18n("Search Output..."),this);
    _searchToggleAction->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_F) );
    _searchToggleAction->setIcon( KIcon("edit-find") );
    _searchToggleAction->setCheckable(true);
    action = collection->addAction("search-history" , _searchToggleAction);
    connect( action , SIGNAL(toggled(bool)) , this , SLOT(searchHistory(bool)) );

    _findNextAction = collection->addAction("find-next");
    _findNextAction->setIcon( KIcon("go-down-search") );
    _findNextAction->setText( i18n("Find Next") );
    _findNextAction->setShortcut( QKeySequence(Qt::Key_F3) );
    _findNextAction->setEnabled(false);
    connect( _findNextAction , SIGNAL(triggered()) , this , SLOT(findNextInHistory()) );

    _findPreviousAction = collection->addAction("find-previous");
    _findPreviousAction->setIcon( KIcon("go-up-search") );
    _findPreviousAction->setText( i18n("Find Previous") );
    _findPreviousAction->setShortcut( QKeySequence(Qt::SHIFT + Qt::Key_F3) );
    _findPreviousAction->setEnabled(false);
    connect( _findPreviousAction , SIGNAL(triggered()) , this , SLOT(findPreviousInHistory()) );

    action = collection->addAction("save-history");
    action->setText( i18n("Save Output...") );
    action->setIcon( KIcon("document-save-as") );
    connect( action , SIGNAL(triggered()) , this , SLOT(saveHistory()) );

    action = collection->addAction("history-options");
    action->setText( i18n("Scrollback Options") );
    action->setIcon( KIcon("configure") );
    connect( action , SIGNAL(triggered()) , this , SLOT(showHistoryOptions()) );

    action = collection->addAction("clear-history");
    action->setText( i18n("Clear Scrollback") );
    connect( action , SIGNAL(triggered()) , this , SLOT(clearHistory()) );

    action = collection->addAction("clear-history-and-reset");
    action->setText( i18n("Clear Scrollback && Reset") );
    action->setShortcut( QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_X) );
    connect( action , SIGNAL(triggered()) , this , SLOT(clearHistoryAndReset()) );

    // Profile Options
    action = collection->addAction("edit-current-profile");
    action->setText( i18n("Edit Current Profile...") );
    action->setIcon( KIcon("document-properties") );
    connect( action , SIGNAL(triggered()) , this , SLOT(editCurrentProfile()) );

    _changeProfileMenu = new KMenu(i18n("Change Profile"),_view);
    collection->addAction("change-profile",_changeProfileMenu->menuAction());
    connect( _changeProfileMenu , SIGNAL(aboutToShow()) , this , SLOT(prepareChangeProfileMenu()) );
}
void SessionController::changeProfile(Profile::Ptr profile)
{
    SessionManager::instance()->setSessionProfile(_session,profile);    
}
void SessionController::prepareChangeProfileMenu()
{
    if ( _changeProfileMenu->isEmpty() )
    {
        _profileList = new ProfileList(false,this);
        connect( _profileList , SIGNAL(profileSelected(Profile::Ptr)) ,
                this , SLOT(changeProfile(Profile::Ptr)) );
    }

    _changeProfileMenu->clear();
    _changeProfileMenu->addActions(_profileList->actions());
}
void SessionController::updateCodecAction()
{
    _codecAction->setCurrentCodec( QString(_session->emulation()->codec()->name()) );
}
void SessionController::changeCodec(QTextCodec* codec)
{
    _session->setCodec(codec);
}

void SessionController::editCurrentProfile()
{
    EditProfileDialog* dialog = new EditProfileDialog( QApplication::activeWindow() );

    dialog->setProfile(SessionManager::instance()->sessionProfile(_session));
    dialog->show();
}
void SessionController::renameSession()
{
    QPointer<Session> guard(_session);
    bool ok = false;
    const QString& text = KInputDialog::getText( i18n("Rename Tab") ,
                                                 i18n("Enter new tab text:") ,
                                                 _session->tabTitleFormat(Session::LocalTabTitle) ,
                                                 &ok, QApplication::activeWindow() );
    if (!guard)
        return;

    if ( ok )
    {
        // renaming changes both the local and remote tab title formats, to save confusion over
        // the tab title not changing if renaming the tab whilst the remote tab title format is 
        // being displayed
        //
        // The downside of this approach is that after renaming a tab manually, the ability to 
        // have separate formats for local and remote activities is lost
        _session->setTabTitleFormat(Session::LocalTabTitle,text);
        _session->setTabTitleFormat(Session::RemoteTabTitle,text);

        // trigger an update of the tab text
        snapshot();
    }
}
void SessionController::saveSession()
{
    Q_ASSERT(0); // not implemented yet

    //SaveSessionDialog dialog(_view);
    //int result = dialog.exec();
}
bool SessionController::confirmClose() const
{
    if (_session->isForegroundProcessActive())
    {
        QString title = _session->foregroundProcessName();
      
        // hard coded for now.  In future make it possible for the user to specify which programs
        // are ignored when considering whether to display a confirmation
        QStringList ignoreList; 
        ignoreList << QString(qgetenv("SHELL")).section('/',-1);
        if (ignoreList.contains(title))
            return true;

        QString question;
        if (title.isEmpty())
            question = i18n("A program is currently running in this session."
                            "  Are you sure you want to close it?");
        else
            question = i18n("The program '%1' is currently running in this session."  
                            "  Are you sure you want to close it?",title);

        int result = KMessageBox::warningYesNo(_view->window(),question,i18n("Confirm Close"));
        return (result == KMessageBox::Yes) ? true : false; 
    }
    return true;
}
void SessionController::closeSession()
{
    if (_preventClose)
        return;

    if (confirmClose())
        _session->close();
}

void SessionController::openBrowser()
{
    new KRun(url(), QApplication::activeWindow());
}

void SessionController::copy()
{
    _view->copyClipboard();
}

void SessionController::paste()
{
    _view->pasteClipboard();
}
void SessionController::pasteSelection()
{
    _view->pasteSelection();
}
void SessionController::copyInputTo()
{
    if (!_copyToGroup)
    {
        _copyToGroup = new SessionGroup(this);
        _copyToGroup->addSession(_session);
        _copyToGroup->setMasterStatus(_session,true);
        _copyToGroup->setMasterMode(SessionGroup::CopyInputToAll);
    }

    CopyInputDialog* dialog = new CopyInputDialog(_view);
    dialog->setMasterSession(_session);
    
    QSet<Session*> currentGroup = QSet<Session*>::fromList(_copyToGroup->sessions());
    currentGroup.remove(_session);
    
    dialog->setChosenSessions(currentGroup);

    QPointer<Session> guard(_session);
    int result = dialog->exec();
    if (!guard)
        return;

    if (result)
    {
        QSet<Session*> newGroup = dialog->chosenSessions();
        newGroup.remove(_session);
    
        QSet<Session*> completeGroup = newGroup | currentGroup;
        foreach(Session* session, completeGroup)
        {
            if (newGroup.contains(session) && !currentGroup.contains(session))
                _copyToGroup->addSession(session);
            else if (!newGroup.contains(session) && currentGroup.contains(session))
                _copyToGroup->removeSession(session);
        }

        snapshot();
    }

    delete dialog;
}
void SessionController::clear()
{
    Emulation* emulation = _session->emulation();

    emulation->clearEntireScreen();
}
void SessionController::clearAndReset()
{
    Emulation* emulation = _session->emulation();

    emulation->reset();
    _session->refresh();
}
void SessionController::searchClosed()
{
    _searchToggleAction->toggle();
}

#if 0
void SessionController::searchHistory()
{
    searchHistory(true);
}
#endif

void SessionController::listenForScreenWindowUpdates()
{
    if (_listenForScreenWindowUpdates)
        return;

    connect( _view->screenWindow() , SIGNAL(outputChanged()) , this , 
            SLOT(updateSearchFilter()) );
    connect( _view->screenWindow() , SIGNAL(scrolled(int)) , this , 
            SLOT(updateSearchFilter()) );

    _listenForScreenWindowUpdates = true;
}

// searchHistory() may be called either as a result of clicking a menu item or
// as a result of changing the search bar widget
void SessionController::searchHistory(bool showSearchBar)
{
    if ( _searchBar )
    {
        _searchBar->setVisible(showSearchBar);

        if (showSearchBar)
        {
            removeSearchFilter();

            listenForScreenWindowUpdates();
            
            _searchFilter = new RegExpFilter();
            _view->filterChain()->addFilter(_searchFilter);
            connect( _searchBar , SIGNAL(searchChanged(const QString&)) , this ,
                    SLOT(searchTextChanged(const QString&)) );

            // invoke search for matches for the current search text
            const QString& currentSearchText = _searchBar->searchText();
            if (!currentSearchText.isEmpty())
            {
                searchTextChanged(currentSearchText);
            }

            setFindNextPrevEnabled(true);
        }
        else
        {
            setFindNextPrevEnabled(false);

            disconnect( _searchBar , SIGNAL(searchChanged(const QString&)) , this ,
                    SLOT(searchTextChanged(const QString&)) );

            removeSearchFilter();

            _view->setFocus( Qt::ActiveWindowFocusReason );
        }
    }
}
void SessionController::setFindNextPrevEnabled(bool enabled)
{
    _findNextAction->setEnabled(enabled);
    _findPreviousAction->setEnabled(enabled);
}
void SessionController::searchTextChanged(const QString& text)
{
    Q_ASSERT( _view->screenWindow() );

    if ( text.isEmpty() )
        _view->screenWindow()->clearSelection();

    // update search.  this is called even when the text is
    // empty to clear the view's filters
    beginSearch(text , SearchHistoryTask::ForwardsSearch);
}
void SessionController::searchCompleted(bool success)
{
    if ( _searchBar )
        _searchBar->setFoundMatch(success);
}

void SessionController::beginSearch(const QString& text , int direction)
{
    Q_ASSERT( _searchBar );
    Q_ASSERT( _searchFilter );

    Qt::CaseSensitivity caseHandling = _searchBar->matchCase() ? Qt::CaseSensitive : Qt::CaseInsensitive;
    QRegExp::PatternSyntax syntax = _searchBar->matchRegExp() ? QRegExp::RegExp : QRegExp::FixedString;

    QRegExp regExp( text.trimmed() ,  caseHandling , syntax );
    _searchFilter->setRegExp(regExp);

    if ( !regExp.isEmpty() )
    {
        SearchHistoryTask* task = new SearchHistoryTask(this);

        connect( task , SIGNAL(completed(bool)) , this , SLOT(searchCompleted(bool)) );

        task->setRegExp(regExp);
        task->setSearchDirection( (SearchHistoryTask::SearchDirection)direction );
        task->setAutoDelete(true);
        task->addScreenWindow( _session , _view->screenWindow() );
        task->execute();
    }

    _view->processFilters();
}
void SessionController::highlightMatches(bool highlight)
{
    if ( highlight )
    {
        _view->filterChain()->addFilter(_searchFilter);
        _view->processFilters();
    }
    else
    {
        _view->filterChain()->removeFilter(_searchFilter);
    }

    _view->update();
}
void SessionController::findNextInHistory()
{
    Q_ASSERT( _searchBar );
    Q_ASSERT( _searchFilter );

    beginSearch(_searchBar->searchText(),SearchHistoryTask::ForwardsSearch);
}
void SessionController::findPreviousInHistory()
{
    Q_ASSERT( _searchBar );
    Q_ASSERT( _searchFilter );

    beginSearch(_searchBar->searchText(),SearchHistoryTask::BackwardsSearch);
}
void SessionController::showHistoryOptions()
{
    HistorySizeDialog* dialog = new HistorySizeDialog( QApplication::activeWindow() );
    const HistoryType& currentHistory = _session->historyType();

    if ( currentHistory.isEnabled() )
    {
        if ( currentHistory.isUnlimited() )
            dialog->setMode( HistorySizeDialog::UnlimitedHistory );
        else
        {
            dialog->setMode( HistorySizeDialog::FixedSizeHistory );
            dialog->setLineCount( currentHistory.maximumLineCount() );
        }
    }
    else
        dialog->setMode( HistorySizeDialog::NoHistory );

    connect( dialog , SIGNAL(optionsChanged(int,int,bool)) ,
             this , SLOT(scrollBackOptionsChanged(int,int,bool)) );

    dialog->show();
}
void SessionController::sessionResizeRequest(const QSize& size)
{
    kDebug(1211) << "View resize requested to " << size;
    _view->setSize(size.width(),size.height());
}
void SessionController::scrollBackOptionsChanged(int mode, int lines, bool saveToCurrentProfile )
{
    switch (mode)
    {
        case HistorySizeDialog::NoHistory:
            _session->setHistoryType( HistoryTypeNone() );
            break;
         case HistorySizeDialog::FixedSizeHistory:
            _session->setHistoryType( HistoryTypeBuffer(lines) );
            break;
         case HistorySizeDialog::UnlimitedHistory:
             _session->setHistoryType( HistoryTypeFile() );
            break;
    }
    if (saveToCurrentProfile)
    {
        Profile::Ptr profile = SessionManager::instance()->sessionProfile(_session);

        switch (mode)
        {
            case HistorySizeDialog::NoHistory:
                profile->setProperty(Profile::HistoryMode , Profile::DisableHistory);
                break;
            case HistorySizeDialog::FixedSizeHistory:
                profile->setProperty(Profile::HistoryMode , Profile::FixedSizeHistory);
                profile->setProperty(Profile::HistorySize , lines);
                break;
            case HistorySizeDialog::UnlimitedHistory:
                profile->setProperty(Profile::HistoryMode , Profile::UnlimitedHistory);
                break;
        }
        SessionManager::instance()->changeProfile(profile, profile->setProperties());
    }
}

void SessionController::saveHistory()
{
    SessionTask* task = new SaveHistoryTask(this);
    task->setAutoDelete(true);
    task->addSession( _session );
    task->execute();
}
void SessionController::clearHistory()
{
    _session->clearHistory();
}
void SessionController::clearHistoryAndReset()
{
    clearAndReset();
    clearHistory();
}
void SessionController::increaseTextSize()
{
    QFont font = _view->getVTFont();
    font.setPointSize(font.pointSize()+1);
    _view->setVTFont(font);

    //TODO - Save this setting as a session default
}
void SessionController::decreaseTextSize()
{
    static const int MinimumFontSize = 6;

    QFont font = _view->getVTFont();
    font.setPointSize( qMax(font.pointSize()-1,MinimumFontSize) );
    _view->setVTFont(font);

    //TODO - Save this setting as a session default
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

        QString title = _session->title(Session::DisplayedTitleRole);

        // special handling for the "%w" marker which is replaced with the
        // window title set by the shell
        title.replace("%w",_session->userTitle());
        // special handling for the "%#" marker which is replaced with the 
        // number of the shell
        title.replace("%#",QString::number(_session->sessionId()));

       if ( title.isEmpty() )
          title = _session->title(Session::NameRole);

       setTitle( title );
}

void SessionController::showDisplayContextMenu(const QPoint& position)
{
    // needed to make sure the popup menu is available, even if a hosting
    // application did not merge our GUI.
    if (!factory()) 
    {
        if (!clientBuilder()) 
            setClientBuilder(new KXMLGUIBuilder(_view));
        
        KXMLGUIFactory* factory = new KXMLGUIFactory(clientBuilder(), this);
        factory->addClient(this);
    }

    QMenu* popup = qobject_cast<QMenu*>(factory()->container("session-popup-menu",this));
    if (popup)
    {
        // prepend content-specific actions such as "Open Link", "Copy Email Address" etc.
        QList<QAction*> contentActions = _view->filterActions(position);
        QAction* contentSeparator = new QAction(popup);
        contentSeparator->setSeparator(true);
        contentActions << contentSeparator;

        _preventClose = true;

        popup->insertActions(popup->actions().value(0,0),contentActions);
        QAction* chosen = popup->exec( _view->mapToGlobal(position) );

        // remove content-specific actions, unless the close action was chosen
        // in which case the popup menu will be partially destroyed at this point
           foreach(QAction* action,contentActions)
            popup->removeAction(action);
        delete contentSeparator;

        _preventClose = false;

        if (chosen && chosen->objectName() == "close-session")
            chosen->trigger();
    }
    else
    {
        kWarning() << "Unable to display popup menu for session"
                   << _session->title(Session::NameRole)
                   << ", no GUI factory available to build the popup.";
    }
}

void SessionController::sessionStateChanged(int state)
{
    if ( state == _previousState )
        return;

    _previousState = state;

    // TODO - Replace the icon choices below when suitable icons for silence and activity
    // are available
    if ( state == NOTIFYACTIVITY )
    {
        if (_activityIcon.isNull())
        {
            _activityIcon = KIcon("dialog-information");
        }

        setIcon(_activityIcon);
    }
    else if ( state == NOTIFYSILENCE )
    {
        if (_silenceIcon.isNull())
        {
            _silenceIcon = KIcon("dialog-information");
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

bool SessionController::isKonsolePart() const
{
    // Check to see if we are being called from Konsole or a KPart
    if (QString(kapp->metaObject()->className()) == "Konsole::Application")
        return false;
    else
        return true;
}

SessionTask::SessionTask(QObject* parent)
    :  QObject(parent)
    ,  _autoDelete(false)
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
void SessionTask::addSession(Session* session)
{
    _sessions << session;
}
QList<SessionPtr> SessionTask::sessions() const
{
    return _sessions;
}

SaveHistoryTask::SaveHistoryTask(QObject* parent)
    : SessionTask(parent)
{
}
SaveHistoryTask::~SaveHistoryTask()
{
}

void SaveHistoryTask::execute()
{
    QListIterator<SessionPtr> iter(sessions());

    // TODO - think about the UI when saving multiple history sessions, if there are more than two or
    //        three then providing a URL for each one will be tedious

    // TODO - show a warning ( preferably passive ) if saving the history output fails
    //

     KFileDialog* dialog = new KFileDialog( QString(":konsole") /* check this */,
                                               QString(), QApplication::activeWindow() );
     dialog->setOperationMode(KFileDialog::Saving);
     dialog->setConfirmOverwrite(true);

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

        dialog->setCaption( i18n("Save Output From %1",session->title(Session::NameRole)) );

        int result = dialog->exec();

        if ( result != QDialog::Accepted )
            continue;

        KUrl url = dialog->selectedUrl();

        if ( !url.isValid() )
        { // UI:  Can we make this friendlier?
            KMessageBox::sorry( 0 , i18n("%1 is an invalid URL, the output could not be saved.",url.url()) );
            continue;
        }

        KIO::TransferJob* job = KIO::put( url,
                                          -1,   // no special permissions
                                          // overwrite existing files
                                          // do not resume an existing transfer
                                          // show progress information only for remote
                                          // URLs
                                          KIO::Overwrite | (url.isLocalFile() ? KIO::HideProgressInfo : KIO::DefaultFlags)
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
                                       // from.
                                       // this is set to -1 to indicate the job has just been started

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

    // transfer LINES_PER_REQUEST lines from the session's history
    // to the save location
    if ( info.session )
    {
        // note:  when retrieving lines from the emulation,
        // the first line is at index 0.

        int sessionLines = info.session->emulation()->lineCount();

        if ( sessionLines-1 == info.lastLineFetched )
            return; // if there is no more data to transfer then stop the job

        int copyUpToLine = qMin( info.lastLineFetched + LINES_PER_REQUEST ,
                                 sessionLines-1 );

        QTextStream stream(&data,QIODevice::ReadWrite);
        info.decoder->begin(&stream);
        info.session->emulation()->writeToStream( info.decoder , info.lastLineFetched+1 , copyUpToLine );
        info.decoder->end();

        // if there are still more lines to process after this request
        // then insert a new line character
        // to ensure that the next block of lines begins on a new line
        //
        // FIXME - There is still an extra new-line at the end of the save data.
        if ( copyUpToLine <= sessionLines-1 )
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
    emit completed(true);

    if ( autoDelete() )
        deleteLater();
}
void SearchHistoryTask::addScreenWindow( Session* session , ScreenWindow* searchWindow )
{
   _windows.insert(session,searchWindow);
}
void SearchHistoryTask::execute()
{
    QMapIterator< SessionPtr , ScreenWindowPtr > iter(_windows);

    while ( iter.hasNext() )
    {
        iter.next();
        executeOnScreenWindow( iter.key() , iter.value() );
    }
}

void SearchHistoryTask::executeOnScreenWindow( SessionPtr session , ScreenWindowPtr window )
{
    Q_ASSERT( session );
    Q_ASSERT( window );

    Emulation* emulation = session->emulation();

    int selectionColumn = 0;
    int selectionLine = 0;

    window->getSelectionEnd(selectionColumn , selectionLine);

    if ( !_regExp.isEmpty() )
    {
        int pos = -1;
        const bool forwards = ( _direction == ForwardsSearch );
        int startLine = selectionLine + window->currentLine() + ( forwards ? 1 : -1 );
        // Temporary fix for #205495
        if (startLine < 0) startLine = 0;
        const int lastLine = window->lineCount() - 1;
        QString string;

        //text stream to read history into string for pattern or regular expression searching
        QTextStream searchStream(&string);

        PlainTextDecoder decoder;
        decoder.setRecordLinePositions(true);

        //setup first and last lines depending on search direction
        int line = startLine;

        //read through and search history in blocks of 10K lines.
        //this balances the need to retrieve lots of data from the history each time
        //(for efficient searching)
        //without using silly amounts of memory if the history is very large.
        const int maxDelta = qMin(window->lineCount(),10000);
        int delta = forwards ? maxDelta : -maxDelta;

        int endLine = line;
        bool hasWrapped = false;  // set to true when we reach the top/bottom
                                  // of the output and continue from the other
                                  // end

        //loop through history in blocks of <delta> lines.
        do
        {
            // ensure that application does not appear to hang
            // if searching through a lengthy output
            QApplication::processEvents();

            // calculate lines to search in this iteration
            if ( hasWrapped )
            {
                if ( endLine == lastLine )
                    line = 0;
                else if ( endLine == 0 )
                    line = lastLine;

                endLine += delta;

                if ( forwards )
                   endLine = qMin( startLine , endLine );
                else
                   endLine = qMax( startLine , endLine );
            }
            else
            {
                endLine += delta;

                if ( endLine > lastLine )
                {
                    hasWrapped = true;
                    endLine = lastLine;
                } else if ( endLine < 0 )
                {
                    hasWrapped = true;
                    endLine = 0;
                }
            }

            decoder.begin(&searchStream);
            emulation->writeToStream(&decoder, qMin(endLine,line) , qMax(endLine,line) );
            decoder.end();

            // line number search below assumes that the buffer ends with a new-line 
            string.append('\n');

            pos = -1;
            if (forwards)
                pos = string.indexOf(_regExp);
            else
                pos = string.lastIndexOf(_regExp);

            //if a match is found, position the cursor on that line and update the screen
            if ( pos != -1 )
            {
                int newLines = 0;
                QList<int> linePositions = decoder.linePositions();
                while (newLines < linePositions.count() && linePositions[newLines] <= pos)
                    newLines++;

                // ignore the new line at the start of the buffer
                newLines--;

                int findPos = qMin(line,endLine) + newLines;

                highlightResult(window,findPos);

                emit completed(true);

                return;
            }

            //clear the current block of text and move to the next one
            string.clear();
            line = endLine;

        } while ( startLine != endLine );

        // if no match was found, clear selection to indicate this
        window->clearSelection();
        window->notifyOutputChanged();
    }

    emit completed(false);
}
void SearchHistoryTask::highlightResult(ScreenWindowPtr window , int findPos)
{
     //work out how many lines into the current block of text the search result was found
     //- looks a little painful, but it only has to be done once per search.

     kDebug(1211) << "Found result at line " << findPos;

     //update display to show area of history containing selection
     window->scrollTo(findPos);
     window->setSelectionStart( 0 , findPos - window->currentLine() , false );
     window->setSelectionEnd( window->columnCount() , findPos - window->currentLine() );
     window->setTrackOutput(false);
     window->notifyOutputChanged();
}

SearchHistoryTask::SearchHistoryTask(QObject* parent)
    : SessionTask(parent)
    , _direction(ForwardsSearch)
{

}
void SearchHistoryTask::setSearchDirection( SearchDirection direction )
{
    _direction = direction;
}
SearchHistoryTask::SearchDirection SearchHistoryTask::searchDirection() const
{
    return _direction;
}
void SearchHistoryTask::setRegExp(const QRegExp& expression)
{
    _regExp = expression;
}
QRegExp SearchHistoryTask::regExp() const
{
    return _regExp;
}

#include "SessionController.moc"


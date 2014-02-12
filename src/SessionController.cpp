/*
    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 2009 by Thomas Dreibholz <dreibh@iem.uni-due.de>

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
#include <QApplication>
#include <QMenu>
#include <QtGui/QKeyEvent>
#include <QPrinter>
#include <QPrintDialog>
#include <QPainter>

// KDE
#include <KAction>
#include <KActionMenu>
#include <KActionCollection>
#include <KIcon>
#include <KLocalizedString>
#include <KMenu>
#include <KMessageBox>
#include <KRun>
#include <KShell>
#include <KToolInvocation>
#include <KStandardDirs>
#include <KToggleAction>
#include <KSelectAction>
#include <KUrl>
#include <KXmlGuiWindow>
#include <KXMLGUIFactory>
#include <KXMLGUIBuilder>
#include <KDebug>
#include <KUriFilter>
#include <KStringHandler>
#include <KConfigGroup>
#include <KGlobal>

#include <kdeversion.h>
#if KDE_IS_VERSION(4, 9, 1)
#include <KCodecAction>
#else
#include <kcodecaction.h>
#endif

// Konsole
#include "EditProfileDialog.h"
#include "CopyInputDialog.h"
#include "Emulation.h"
#include "Filter.h"
#include "History.h"
#include "HistorySizeDialog.h"
#include "IncrementalSearchBar.h"
#include "RenameTabDialog.h"
#include "ScreenWindow.h"
#include "Session.h"
#include "ProfileList.h"
#include "TerminalDisplay.h"
#include "SessionManager.h"
#include "Enumeration.h"
#include "PrintOptions.h"

// for SaveHistoryTask
#include <KFileDialog>
#include <KIO/Job>
#include <KJob>
#include "TerminalCharacterDecoder.h"

// For Unix signal names
#include <signal.h>

using namespace Konsole;

// TODO - Replace the icon choices below when suitable icons for silence and
// activity are available
const KIcon SessionController::_activityIcon("dialog-information");
const KIcon SessionController::_silenceIcon("dialog-information");
const KIcon SessionController::_broadcastIcon("emblem-important");

QSet<SessionController*> SessionController::_allControllers;
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
    , _copyInputToAllTabsAction(0)
    , _findAction(0)
    , _findNextAction(0)
    , _findPreviousAction(0)
    , _urlFilterUpdateRequired(false)
    , _searchStartLine(0)
    , _prevSearchResultLine(0)
    , _searchBar(0)
    , _codecAction(0)
    , _switchProfileMenu(0)
    , _webSearchMenu(0)
    , _listenForScreenWindowUpdates(false)
    , _preventClose(false)
    , _keepIconUntilInteraction(false)
    , _showMenuAction(0)
    , _isSearchBarEnabled(false)
{
    Q_ASSERT(session);
    Q_ASSERT(view);

    // handle user interface related to session (menus etc.)
    if (isKonsolePart()) {
        setXMLFile("konsole/partui.rc");
        setupCommonActions();
    } else {
        setXMLFile("konsole/sessionui.rc");
        setupCommonActions();
        setupExtraActions();
    }

    actionCollection()->addAssociatedWidget(view);
    foreach(QAction * action, actionCollection()->actions()) {
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }

    setIdentifier(++_lastControllerId);
    sessionTitleChanged();

    view->installEventFilter(this);
    view->setSessionController(this);

    // listen for session resize requests
    connect(_session, SIGNAL(resizeRequest(QSize)), this,
            SLOT(sessionResizeRequest(QSize)));

    // listen for popup menu requests
    connect(_view, SIGNAL(configureRequest(QPoint)), this,
            SLOT(showDisplayContextMenu(QPoint)));

    // move view to newest output when keystrokes occur
    connect(_view, SIGNAL(keyPressedSignal(QKeyEvent*)), this,
            SLOT(trackOutput(QKeyEvent*)));

    // listen to activity / silence notifications from session
    connect(_session, SIGNAL(stateChanged(int)), this,
            SLOT(sessionStateChanged(int)));
    // listen to title and icon changes
    connect(_session, SIGNAL(titleChanged()), this, SLOT(sessionTitleChanged()));

    connect(_session , SIGNAL(currentDirectoryChanged(QString)) ,
            this , SIGNAL(currentDirectoryChanged(QString)));

    // listen for color changes
    connect(_session, SIGNAL(changeBackgroundColorRequest(QColor)), _view, SLOT(setBackgroundColor(QColor)));
    connect(_session, SIGNAL(changeForegroundColorRequest(QColor)), _view, SLOT(setForegroundColor(QColor)));

    // update the title when the session starts
    connect(_session, SIGNAL(started()), this, SLOT(snapshot()));

    // listen for output changes to set activity flag
    connect(_session->emulation(), SIGNAL(outputChanged()), this,
            SLOT(fireActivity()));

    // listen for detection of ZModem transfer
    connect(_session, SIGNAL(zmodemDetected()), this, SLOT(zmodemDownload()));

    // listen for flow control status changes
    connect(_session, SIGNAL(flowControlEnabledChanged(bool)), _view,
            SLOT(setFlowControlWarningEnabled(bool)));
    _view->setFlowControlWarningEnabled(_session->flowControlEnabled());

    // take a snapshot of the session state every so often when
    // user activity occurs
    //
    // the timer is owned by the session so that it will be destroyed along
    // with the session
    _interactionTimer = new QTimer(_session);
    _interactionTimer->setSingleShot(true);
    _interactionTimer->setInterval(500);
    connect(_interactionTimer, SIGNAL(timeout()), this, SLOT(snapshot()));
    connect(_view, SIGNAL(keyPressedSignal(QKeyEvent*)), this, SLOT(interactionHandler()));

    // take a snapshot of the session state periodically in the background
    QTimer* backgroundTimer = new QTimer(_session);
    backgroundTimer->setSingleShot(false);
    backgroundTimer->setInterval(2000);
    connect(backgroundTimer, SIGNAL(timeout()), this, SLOT(snapshot()));
    backgroundTimer->start();

    _allControllers.insert(this);

    // A list of programs that accept Ctrl+C to clear command line used
    // before outputting bookmark.
    _bookmarkValidProgramsToClear << "bash" << "fish" << "sh";
    _bookmarkValidProgramsToClear << "tcsh" << "zsh";
}

SessionController::~SessionController()
{
    if (_view)
        _view->setScreenWindow(0);

    _allControllers.remove(this);

    if (!_editProfileDialog.isNull()) {
        delete _editProfileDialog.data();
    }
}
void SessionController::trackOutput(QKeyEvent* event)
{
    Q_ASSERT(_view->screenWindow());

    // jump to the end of the history buffer unless the key pressed
    // is one of the three main modifiers, as these are used to select
    // the selection mode (eg. Ctrl+Alt+<Left Click> for column/block selection)
    switch (event->key()) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Alt:
        break;
    default:
        _view->screenWindow()->setTrackOutput(true);
    }
}
void SessionController::interactionHandler()
{
    // This flag is used to make sure those special icons indicating interest
    // events (activity/silence/bell?) remain in the tab until user interaction
    // happens. Otherwise, those special icons will quickly be replaced by
    // normal icon when ::snapshot() is triggered
    _keepIconUntilInteraction = false;
    _interactionTimer->start();
}

void SessionController::requireUrlFilterUpdate()
{
    // this method is called every time the screen window's output changes, so do not
    // do anything expensive here.

    _urlFilterUpdateRequired = true;
}
void SessionController::snapshot()
{
    Q_ASSERT(_session != 0);

    QString title = _session->getDynamicTitle();
    title         = title.simplified();

    // Visualize that the session is broadcasting to others
    if (_copyToGroup && _copyToGroup->sessions().count() > 1) {
        title.append('*');
    }

    // use the fallback title if needed
    if (title.isEmpty()) {
        title = _session->title(Session::NameRole);
    }

    // apply new title
    _session->setTitle(Session::DisplayedTitleRole, title);

    // do not forget icon
    updateSessionIcon();
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

void SessionController::openUrl(const KUrl& url)
{
    // Clear shell's command line
    if (!_session->isForegroundProcessActive()
            && _bookmarkValidProgramsToClear.contains(_session->foregroundProcessName())) {
        _session->emulation()->sendText(QChar(0x03)); // Ctrl+C
        _session->emulation()->sendText(QChar('\n'));
    }

    // handle local paths
    if (url.isLocalFile()) {
        QString path = url.toLocalFile();
        _session->emulation()->sendText("cd " + KShell::quoteArg(path) + '\r');
    } else if (url.protocol().isEmpty()) {
        // KUrl couldn't parse what the user entered into the URL field
        // so just dump it to the shell
        QString command = url.prettyUrl();
        if (!command.isEmpty())
            _session->emulation()->sendText(command + '\r');
    } else if (url.protocol() == "ssh") {
        QString sshCommand = "ssh ";

        if (url.port() > -1) {
            sshCommand += QString("-p %1 ").arg(url.port());
        }
        if (url.hasUser()) {
            sshCommand += (url.user() + '@');
        }
        if (url.hasHost()) {
            sshCommand += url.host();
        }

        _session->sendText(sshCommand + '\r');

    } else if (url.protocol() == "telnet") {
        QString telnetCommand = "telnet ";

        if (url.hasUser()) {
            telnetCommand += QString("-l %1 ").arg(url.user());
        }
        if (url.hasHost()) {
            telnetCommand += (url.host() + ' ');
        }
        if (url.port() > -1) {
            telnetCommand += QString::number(url.port());
        }

        _session->sendText(telnetCommand + '\r');

    } else {
        //TODO Implement handling for other Url types

        KMessageBox::sorry(_view->window(),
                           i18n("Konsole does not know how to open the bookmark: ") +
                           url.prettyUrl());

        kWarning() << "Unable to open bookmark at url" << url << ", I do not know"
                   << " how to handle the protocol " << url.protocol();
    }
}

void SessionController::setupPrimaryScreenSpecificActions(bool use)
{
    KActionCollection* collection = actionCollection();
    QAction* clearAction = collection->action("clear-history");
    QAction* resetAction = collection->action("clear-history-and-reset");
    QAction* selectAllAction = collection->action("select-all");
    QAction* selectLineAction = collection->action("select-line");

    // these actions are meaningful only when primary screen is used.
    clearAction->setEnabled(use);
    resetAction->setEnabled(use);
    selectAllAction->setEnabled(use);
    selectLineAction->setEnabled(use);
}

void SessionController::selectionChanged(const QString& selectedText)
{
    _selectedText = selectedText;
    updateCopyAction(selectedText);
}

void SessionController::updateCopyAction(const QString& selectedText)
{
    QAction* copyAction = actionCollection()->action("edit_copy");

    // copy action is meaningful only when some text is selected.
    copyAction->setEnabled(!selectedText.isEmpty());
}

void SessionController::updateWebSearchMenu()
{
    // reset
    _webSearchMenu->setVisible(false);
    _webSearchMenu->menu()->clear();

    if (_selectedText.isEmpty())
        return;

    QString searchText = _selectedText;
    searchText = searchText.replace('\n', ' ').replace('\r', ' ').simplified();

    if (searchText.isEmpty())
        return;

    KUriFilterData filterData(searchText);
    filterData.setSearchFilteringOptions(KUriFilterData::RetrievePreferredSearchProvidersOnly);

    if (KUriFilter::self()->filterSearchUri(filterData, KUriFilter::NormalTextFilter)) {
        const QStringList searchProviders = filterData.preferredSearchProviders();
        if (!searchProviders.isEmpty()) {
            _webSearchMenu->setText(i18n("Search for '%1' with",  KStringHandler::rsqueeze(searchText, 16)));

            KAction* action = 0;

            foreach(const QString& searchProvider, searchProviders) {
                action = new KAction(searchProvider, _webSearchMenu);
                action->setIcon(KIcon(filterData.iconNameForPreferredSearchProvider(searchProvider)));
                action->setData(filterData.queryForPreferredSearchProvider(searchProvider));
                connect(action, SIGNAL(triggered()), this, SLOT(handleWebShortcutAction()));
                _webSearchMenu->addAction(action);
            }

            _webSearchMenu->addSeparator();

            action = new KAction(i18n("Configure Web Shortcuts..."), _webSearchMenu);
            action->setIcon(KIcon("configure"));
            connect(action, SIGNAL(triggered()), this, SLOT(configureWebShortcuts()));
            _webSearchMenu->addAction(action);

            _webSearchMenu->setVisible(true);
        }
    }
}

void SessionController::handleWebShortcutAction()
{
    KAction* action = qobject_cast<KAction*>(sender());
    if (!action)
        return;

    KUriFilterData filterData(action->data().toString());

    if (KUriFilter::self()->filterUri(filterData, QStringList() << "kurisearchfilter")) {
        const KUrl& url = filterData.uri();
        new KRun(url, QApplication::activeWindow());
    }
}

void SessionController::configureWebShortcuts()
{
    KToolInvocation::kdeinitExec("kcmshell4", QStringList() << "ebrowsing");
}

void SessionController::sendSignal(QAction* action)
{
    const int signal = action->data().value<int>();
    _session->sendSignal(signal);
}

bool SessionController::eventFilter(QObject* watched , QEvent* event)
{
    if (watched == _view) {
        if (event->type() == QEvent::FocusIn) {
            // notify the world that the view associated with this session has been focused
            // used by the view manager to update the title of the MainWindow widget containing the view
            emit focused(this);

            // when the view is focused, set bell events from the associated session to be delivered
            // by the focused view

            // first, disconnect any other views which are listening for bell signals from the session
            disconnect(_session, SIGNAL(bellRequest(QString)), 0, 0);
            // second, connect the newly focused view to listen for the session's bell signal
            connect(_session, SIGNAL(bellRequest(QString)),
                    _view, SLOT(bell(QString)));

            if (_copyInputToAllTabsAction && _copyInputToAllTabsAction->isChecked()) {
                // A session with "Copy To All Tabs" has come into focus:
                // Ensure that newly created sessions are included in _copyToGroup!
                copyInputToAllTabs();
            }
        }
        // when a mouse move is received, create the URL filter and listen for output changes if
        // it has not already been created.  If it already exists, then update only if the output
        // has changed since the last update ( _urlFilterUpdateRequired == true )
        //
        // also check that no mouse buttons are pressed since the URL filter only applies when
        // the mouse is hovering over the view
        if (event->type() == QEvent::MouseMove &&
                (!_viewUrlFilter || _urlFilterUpdateRequired) &&
                ((QMouseEvent*)event)->buttons() == Qt::NoButton) {
            if (_view->screenWindow() && !_viewUrlFilter) {
                connect(_view->screenWindow(), SIGNAL(scrolled(int)), this,
                        SLOT(requireUrlFilterUpdate()));
                connect(_view->screenWindow(), SIGNAL(outputChanged()), this,
                        SLOT(requireUrlFilterUpdate()));

                // install filter on the view to highlight URLs
                _viewUrlFilter = new UrlFilter();
                _view->filterChain()->addFilter(_viewUrlFilter);
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
    if (_searchBar) {
        disconnect(this, 0, _searchBar, 0);
        disconnect(_searchBar, 0, this, 0);
    }

    // connect new search bar
    _searchBar = searchBar;
    if (_searchBar) {
        connect(_searchBar, SIGNAL(unhandledMovementKeyPressed(QKeyEvent*)), this, SLOT(movementKeyFromSearchBarReceived(QKeyEvent*)));
        connect(_searchBar, SIGNAL(closeClicked()), this, SLOT(searchClosed()));
        connect(_searchBar, SIGNAL(searchFromClicked()), this, SLOT(searchFrom()));
        connect(_searchBar, SIGNAL(findNextClicked()), this, SLOT(findNextInHistory()));
        connect(_searchBar, SIGNAL(findPreviousClicked()), this, SLOT(findPreviousInHistory()));
        connect(_searchBar, SIGNAL(highlightMatchesToggled(bool)) , this , SLOT(highlightMatches(bool)));
        connect(_searchBar, SIGNAL(matchCaseToggled(bool)), this, SLOT(changeSearchMatch()));

        // if the search bar was previously active
        // then re-enter search mode
        enableSearchBar(_isSearchBarEnabled);
    }
}
IncrementalSearchBar* SessionController::searchBar() const
{
    return _searchBar;
}

void SessionController::setShowMenuAction(QAction* action)
{
    _showMenuAction = action;
}

void SessionController::setupCommonActions()
{
    KAction* action = 0;
    KActionCollection* collection = actionCollection();

    // Close Session
    action = collection->addAction("close-session", this, SLOT(closeSession()));
    if (isKonsolePart())
        action->setText(i18n("&Close Session"));
    else
        action->setText(i18n("&Close Tab"));

    action->setIcon(KIcon("tab-close"));
    action->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_W));

    // Open Browser
    action = collection->addAction("open-browser", this, SLOT(openBrowser()));
    action->setText(i18n("Open File Manager"));
    action->setIcon(KIcon("system-file-manager"));

    // Copy and Paste
    action = KStandardAction::copy(this, SLOT(copy()), collection);
    action->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_C));
    // disabled at first, since nothing has been selected now
    action->setEnabled(false);

    action = KStandardAction::paste(this, SLOT(paste()), collection);
    KShortcut pasteShortcut = action->shortcut();
    pasteShortcut.setPrimary(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_V));
    pasteShortcut.setAlternate(QKeySequence(Qt::SHIFT + Qt::Key_Insert));
    action->setShortcut(pasteShortcut);

    action = collection->addAction("paste-selection", this, SLOT(pasteFromX11Selection()));
    action->setText(i18n("Paste Selection"));
    action->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_Insert));

    _webSearchMenu = new KActionMenu(i18n("Web Search"), this);
    _webSearchMenu->setIcon(KIcon("preferences-web-browser-shortcuts"));
    _webSearchMenu->setVisible(false);
    collection->addAction("web-search", _webSearchMenu);


    action = collection->addAction("select-all", this, SLOT(selectAll()));
    action->setText(i18n("&Select All"));
    action->setIcon(KIcon("edit-select-all"));

    action = collection->addAction("select-line", this, SLOT(selectLine()));
    action->setText(i18n("Select &Line"));

    action = KStandardAction::saveAs(this, SLOT(saveHistory()), collection);
    action->setText(i18n("Save Output &As..."));

    action = KStandardAction::print(this, SLOT(print_screen()), collection);
    action->setText(i18n("&Print Screen..."));
    action->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_P));

    action = collection->addAction("adjust-history", this, SLOT(showHistoryOptions()));
    action->setText(i18n("Adjust Scrollback..."));
    action->setIcon(KIcon("configure"));

    action = collection->addAction("clear-history", this, SLOT(clearHistory()));
    action->setText(i18n("Clear Scrollback"));
    action->setIcon(KIcon("edit-clear-history"));

    action = collection->addAction("clear-history-and-reset", this, SLOT(clearHistoryAndReset()));
    action->setText(i18n("Clear Scrollback and Reset"));
    action->setIcon(KIcon("edit-clear-history"));
    action->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_K));

    // Profile Options
    action = collection->addAction("edit-current-profile", this, SLOT(editCurrentProfile()));
    action->setText(i18n("Edit Current Profile..."));
    action->setIcon(KIcon("document-properties"));

    _switchProfileMenu = new KActionMenu(i18n("Switch Profile"), this);
    collection->addAction("switch-profile", _switchProfileMenu);
    connect(_switchProfileMenu->menu(), SIGNAL(aboutToShow()), this, SLOT(prepareSwitchProfileMenu()));

    // History
    _findAction = KStandardAction::find(this, SLOT(searchBarEvent()), collection);
    _findAction->setShortcut(QKeySequence());

    _findNextAction = KStandardAction::findNext(this, SLOT(findNextInHistory()), collection);
    _findNextAction->setShortcut(QKeySequence());
    _findNextAction->setEnabled(false);

    _findPreviousAction = KStandardAction::findPrev(this, SLOT(findPreviousInHistory()), collection);
    _findPreviousAction->setShortcut(QKeySequence());
    _findPreviousAction->setEnabled(false);

    // Character Encoding
    _codecAction = new KCodecAction(i18n("Set &Encoding"), this);
    _codecAction->setIcon(KIcon("character-set"));
    collection->addAction("set-encoding", _codecAction);
    connect(_codecAction->menu(), SIGNAL(aboutToShow()), this, SLOT(updateCodecAction()));
    connect(_codecAction, SIGNAL(triggered(QTextCodec*)), this, SLOT(changeCodec(QTextCodec*)));
}

void SessionController::setupExtraActions()
{
    KAction* action = 0;
    KToggleAction* toggleAction = 0;
    KActionCollection* collection = actionCollection();

    // Rename Session
    action = collection->addAction("rename-session", this, SLOT(renameSession()));
    action->setText(i18n("&Rename Tab..."));
    action->setIcon(KIcon("edit-rename"));
    action->setShortcut(QKeySequence(Qt::CTRL + Qt::ALT + Qt::Key_S));

    // Copy input to ==> all tabs
    KToggleAction* copyInputToAllTabsAction = collection->add<KToggleAction>("copy-input-to-all-tabs");
    copyInputToAllTabsAction->setText(i18n("&All Tabs in Current Window"));
    copyInputToAllTabsAction->setData(CopyInputToAllTabsMode);
    // this action is also used in other place, so remember it
    _copyInputToAllTabsAction = copyInputToAllTabsAction;

    // Copy input to ==> selected tabs
    KToggleAction* copyInputToSelectedTabsAction = collection->add<KToggleAction>("copy-input-to-selected-tabs");
    copyInputToSelectedTabsAction->setText(i18n("&Select Tabs..."));
    copyInputToSelectedTabsAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_Period));
    copyInputToSelectedTabsAction->setData(CopyInputToSelectedTabsMode);

    // Copy input to ==> none
    KToggleAction* copyInputToNoneAction = collection->add<KToggleAction>("copy-input-to-none");
    copyInputToNoneAction->setText(i18nc("@action:inmenu Do not select any tabs", "&None"));
    copyInputToNoneAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_Slash));
    copyInputToNoneAction->setData(CopyInputToNoneMode);
    copyInputToNoneAction->setChecked(true); // the default state

    // The "Copy Input To" submenu
    // The above three choices are represented as combo boxes
    KSelectAction* copyInputActions = collection->add<KSelectAction>("copy-input-to");
    copyInputActions->setText(i18n("Copy Input To"));
    copyInputActions->addAction(copyInputToAllTabsAction);
    copyInputActions->addAction(copyInputToSelectedTabsAction);
    copyInputActions->addAction(copyInputToNoneAction);
    connect(copyInputActions, SIGNAL(triggered(QAction*)), this, SLOT(copyInputActionsTriggered(QAction*)));

    action = collection->addAction("zmodem-upload", this, SLOT(zmodemUpload()));
    action->setText(i18n("&ZModem Upload..."));
    action->setIcon(KIcon("document-open"));
    action->setShortcut(QKeySequence(Qt::CTRL + Qt::ALT + Qt::Key_U));

    // Monitor
    toggleAction = new KToggleAction(i18n("Monitor for &Activity"), this);
    toggleAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_A));
    action = collection->addAction("monitor-activity", toggleAction);
    connect(action, SIGNAL(toggled(bool)), this, SLOT(monitorActivity(bool)));

    toggleAction = new KToggleAction(i18n("Monitor for &Silence"), this);
    toggleAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_I));
    action = collection->addAction("monitor-silence", toggleAction);
    connect(action, SIGNAL(toggled(bool)), this, SLOT(monitorSilence(bool)));

    // Text Size
    action = collection->addAction("enlarge-font", this, SLOT(increaseFontSize()));
    action->setText(i18n("Enlarge Font"));
    action->setIcon(KIcon("format-font-size-more"));
    KShortcut enlargeFontShortcut = action->shortcut();
    enlargeFontShortcut.setPrimary(QKeySequence(Qt::CTRL + Qt::Key_Plus));
    enlargeFontShortcut.setAlternate(QKeySequence(Qt::CTRL + Qt::Key_Equal));
    action->setShortcut(enlargeFontShortcut);

    action = collection->addAction("shrink-font", this, SLOT(decreaseFontSize()));
    action->setText(i18n("Shrink Font"));
    action->setIcon(KIcon("format-font-size-less"));
    action->setShortcut(KShortcut(Qt::CTRL | Qt::Key_Minus));

    // Send signal
    KSelectAction* sendSignalActions = collection->add<KSelectAction>("send-signal");
    sendSignalActions->setText(i18n("Send Signal"));
    connect(sendSignalActions, SIGNAL(triggered(QAction*)), this, SLOT(sendSignal(QAction*)));

    action = collection->addAction("sigstop-signal");
    action->setText(i18n("&Suspend Task")   + " (STOP)");
    action->setData(SIGSTOP);
    sendSignalActions->addAction(action);

    action = collection->addAction("sigcont-signal");
    action->setText(i18n("&Continue Task")  + " (CONT)");
    action->setData(SIGCONT);
    sendSignalActions->addAction(action);

    action = collection->addAction("sighup-signal");
    action->setText(i18n("&Hangup")         + " (HUP)");
    action->setData(SIGHUP);
    sendSignalActions->addAction(action);

    action = collection->addAction("sigint-signal");
    action->setText(i18n("&Interrupt Task") + " (INT)");
    action->setData(SIGINT);
    sendSignalActions->addAction(action);

    action = collection->addAction("sigterm-signal");
    action->setText(i18n("&Terminate Task") + " (TERM)");
    action->setData(SIGTERM);
    sendSignalActions->addAction(action);

    action = collection->addAction("sigkill-signal");
    action->setText(i18n("&Kill Task")      + " (KILL)");
    action->setData(SIGKILL);
    sendSignalActions->addAction(action);

    action = collection->addAction("sigusr1-signal");
    action->setText(i18n("User Signal &1")   + " (USR1)");
    action->setData(SIGUSR1);
    sendSignalActions->addAction(action);

    action = collection->addAction("sigusr2-signal");
    action->setText(i18n("User Signal &2")   + " (USR2)");
    action->setData(SIGUSR2);
    sendSignalActions->addAction(action);

    _findAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_F));
    _findNextAction->setShortcut(QKeySequence(Qt::Key_F3));
    _findPreviousAction->setShortcut(QKeySequence(Qt::SHIFT + Qt::Key_F3));
}

void SessionController::switchProfile(Profile::Ptr profile)
{
    SessionManager::instance()->setSessionProfile(_session, profile);
}

void SessionController::prepareSwitchProfileMenu()
{
    if (_switchProfileMenu->menu()->isEmpty()) {
        _profileList = new ProfileList(false, this);
        connect(_profileList, SIGNAL(profileSelected(Profile::Ptr)), this, SLOT(switchProfile(Profile::Ptr)));
    }

    _switchProfileMenu->menu()->clear();
    _switchProfileMenu->menu()->addActions(_profileList->actions());
}
void SessionController::updateCodecAction()
{
    _codecAction->setCurrentCodec(QString(_session->codec()));
}

void SessionController::changeCodec(QTextCodec* codec)
{
    _session->setCodec(codec);
}

EditProfileDialog* SessionController::profileDialogPointer()
{
    return _editProfileDialog.data();
}

void SessionController::editCurrentProfile()
{
    // Searching for Edit profile dialog opened with the same profile
    foreach (SessionController* session, _allControllers.values()) {
        if (session->profileDialogPointer()
                && session->profileDialogPointer()->isVisible()
                && session->profileDialogPointer()->lookupProfile() == SessionManager::instance()->sessionProfile(_session)) {
            session->profileDialogPointer()->close();
        }
    }

    // NOTE bug311270: For to prevent the crash, the profile must be reset.
    if (!_editProfileDialog.isNull()) {
        // exists but not visible
        delete _editProfileDialog.data();
    }

    _editProfileDialog = new EditProfileDialog(QApplication::activeWindow());
    _editProfileDialog.data()->setProfile(SessionManager::instance()->sessionProfile(_session));
    _editProfileDialog.data()->show();
}

void SessionController::renameSession()
{
    QScopedPointer<RenameTabDialog> dialog(new RenameTabDialog(QApplication::activeWindow()));
    dialog->setTabTitleText(_session->tabTitleFormat(Session::LocalTabTitle));
    dialog->setRemoteTabTitleText(_session->tabTitleFormat(Session::RemoteTabTitle));

    if (_session->isRemote()) {
        dialog->focusRemoteTabTitleText();
    } else {
        dialog->focusTabTitleText();
    }

    QPointer<Session> guard(_session);
    int result = dialog->exec();
    if (!guard)
        return;

    if (result) {
        QString tabTitle = dialog->tabTitleText();
        QString remoteTabTitle = dialog->remoteTabTitleText();

        _session->setTabTitleFormat(Session::LocalTabTitle, tabTitle);
        _session->setTabTitleFormat(Session::RemoteTabTitle, remoteTabTitle);

        // trigger an update of the tab text
        snapshot();
    }
}

bool SessionController::confirmClose() const
{
    if (_session->isForegroundProcessActive()) {
        QString title = _session->foregroundProcessName();

        // hard coded for now.  In future make it possible for the user to specify which programs
        // are ignored when considering whether to display a confirmation
        QStringList ignoreList;
        ignoreList << QString(qgetenv("SHELL")).section('/', -1);
        if (ignoreList.contains(title))
            return true;

        QString question;
        if (title.isEmpty())
            question = i18n("A program is currently running in this session."
                            "  Are you sure you want to close it?");
        else
            question = i18n("The program '%1' is currently running in this session."
                            "  Are you sure you want to close it?", title);

        int result = KMessageBox::warningYesNo(_view->window(), question, i18n("Confirm Close"));
        return (result == KMessageBox::Yes) ? true : false;
    }
    return true;
}
bool SessionController::confirmForceClose() const
{
    if (_session->isRunning()) {
        QString title = _session->program();

        // hard coded for now.  In future make it possible for the user to specify which programs
        // are ignored when considering whether to display a confirmation
        QStringList ignoreList;
        ignoreList << QString(qgetenv("SHELL")).section('/', -1);
        if (ignoreList.contains(title))
            return true;

        QString question;
        if (title.isEmpty())
            question = i18n("A program in this session would not die."
                            "  Are you sure you want to kill it by force?");
        else
            question = i18n("The program '%1' is in this session would not die."
                            "  Are you sure you want to kill it by force?", title);

        int result = KMessageBox::warningYesNo(_view->window(), question, i18n("Confirm Close"));
        return (result == KMessageBox::Yes) ? true : false;
    }
    return true;
}
void SessionController::closeSession()
{
    if (_preventClose)
        return;

    if (confirmClose()) {
        if (_session->closeInNormalWay()) {
            return;
        } else if (confirmForceClose()) {
            if (_session->closeInForceWay())
                return;
            else
                kWarning() << "Konsole failed to close a session in any way.";
        }
    }
}

// Trying to open a remote Url may produce unexpected results.
// Therefore, if a remote url, open the user's home path.
// TODO consider: 1) disable menu upon remote session
//   2) transform url to get the desired result (ssh -> sftp, etc)
void SessionController::openBrowser()
{
    KUrl currentUrl = url();

    if (currentUrl.isLocalFile())
        new KRun(currentUrl, QApplication::activeWindow(), 0, true, true);
    else
        new KRun(KUrl(QDir::homePath()), QApplication::activeWindow(), 0, true, true);
}

void SessionController::copy()
{
    _view->copyToClipboard();
}

void SessionController::paste()
{
    _view->pasteFromClipboard();
}
void SessionController::pasteFromX11Selection()
{
    _view->pasteFromX11Selection();
}
void SessionController::selectAll()
{
    ScreenWindow * screenWindow = _view->screenWindow();
    screenWindow->setSelectionByLineRange(0, _session->emulation()->lineCount());
    _view->copyToX11Selection();
}
void SessionController::selectLine()
{
    _view->selectCurrentLine();
}
static const KXmlGuiWindow* findWindow(const QObject* object)
{
    // Walk up the QObject hierarchy to find a KXmlGuiWindow.
    while (object != 0) {
        const KXmlGuiWindow* window = qobject_cast<const KXmlGuiWindow*>(object);
        if (window != 0) {
            return(window);
        }
        object = object->parent();
    }
    return(0);
}

static bool hasTerminalDisplayInSameWindow(const Session* session, const KXmlGuiWindow* window)
{
    // Iterate all TerminalDisplays of this Session ...
    foreach(const TerminalDisplay* terminalDisplay, session->views()) {
        // ... and check whether a TerminalDisplay has the same
        // window as given in the parameter
        if (window == findWindow(terminalDisplay)) {
            return(true);
        }
    }
    return(false);
}

void SessionController::copyInputActionsTriggered(QAction* action)
{
    const int mode = action->data().value<int>();

    switch (mode) {
    case CopyInputToAllTabsMode:
        copyInputToAllTabs();
        break;
    case CopyInputToSelectedTabsMode:
        copyInputToSelectedTabs();
        break;
    case CopyInputToNoneMode:
        copyInputToNone();
        break;
    default:
        Q_ASSERT(false);
    }
}

void SessionController::copyInputToAllTabs()
{
    if (!_copyToGroup) {
        _copyToGroup = new SessionGroup(this);
    }

    // Find our window ...
    const KXmlGuiWindow* myWindow = findWindow(_view);

    QSet<Session*> group =
        QSet<Session*>::fromList(SessionManager::instance()->sessions());
    for (QSet<Session*>::iterator iterator = group.begin();
            iterator != group.end(); ++iterator) {
        Session* session = *iterator;

        // First, ensure that the session is removed
        // (necessary to avoid duplicates on addSession()!)
        _copyToGroup->removeSession(session);

        // Add current session if it is displayed our window
        if (hasTerminalDisplayInSameWindow(session, myWindow)) {
            _copyToGroup->addSession(session);
        }
    }
    _copyToGroup->setMasterStatus(_session, true);
    _copyToGroup->setMasterMode(SessionGroup::CopyInputToAll);

    snapshot();
}

void SessionController::copyInputToSelectedTabs()
{
    if (!_copyToGroup) {
        _copyToGroup = new SessionGroup(this);
        _copyToGroup->addSession(_session);
        _copyToGroup->setMasterStatus(_session, true);
        _copyToGroup->setMasterMode(SessionGroup::CopyInputToAll);
    }

    QPointer<CopyInputDialog> dialog = new CopyInputDialog(_view);
    dialog->setMasterSession(_session);

    QSet<Session*> currentGroup = QSet<Session*>::fromList(_copyToGroup->sessions());
    currentGroup.remove(_session);

    dialog->setChosenSessions(currentGroup);

    QPointer<Session> guard(_session);
    int result = dialog->exec();
    if (!guard)
        return;

    if (result == QDialog::Accepted) {
        QSet<Session*> newGroup = dialog->chosenSessions();
        newGroup.remove(_session);

        QSet<Session*> completeGroup = newGroup | currentGroup;
        foreach(Session * session, completeGroup) {
            if (newGroup.contains(session) && !currentGroup.contains(session))
                _copyToGroup->addSession(session);
            else if (!newGroup.contains(session) && currentGroup.contains(session))
                _copyToGroup->removeSession(session);
        }

        _copyToGroup->setMasterStatus(_session, true);
        _copyToGroup->setMasterMode(SessionGroup::CopyInputToAll);
        snapshot();
    }
}

void SessionController::copyInputToNone()
{
    if (!_copyToGroup)      // No 'Copy To' is active
        return;

    QSet<Session*> group =
        QSet<Session*>::fromList(SessionManager::instance()->sessions());
    for (QSet<Session*>::iterator iterator = group.begin();
            iterator != group.end(); ++iterator) {
        Session* session = *iterator;

        if (session != _session) {
            _copyToGroup->removeSession(*iterator);
        }
    }
    delete _copyToGroup;
    _copyToGroup = 0;
    snapshot();
}

void SessionController::searchClosed()
{
    _isSearchBarEnabled = false;
    searchHistory(false);
}

void SessionController::setSearchStartToWindowCurrentLine()
{
    setSearchStartTo(-1);
}

void SessionController::setSearchStartTo(int line)
{
    _searchStartLine = line;
    _prevSearchResultLine = line;
}

void SessionController::listenForScreenWindowUpdates()
{
    if (_listenForScreenWindowUpdates)
        return;

    connect(_view->screenWindow(), SIGNAL(outputChanged()), this,
            SLOT(updateSearchFilter()));
    connect(_view->screenWindow(), SIGNAL(scrolled(int)), this,
            SLOT(updateSearchFilter()));
    connect(_view->screenWindow(), SIGNAL(currentResultLineChanged()), _view,
            SLOT(update()));

    _listenForScreenWindowUpdates = true;
}

void SessionController::updateSearchFilter()
{
    if (_searchFilter && _searchBar) {
        _view->processFilters();
    }
}

void SessionController::searchBarEvent()
{
    QString selectedText = _view->screenWindow()->selectedText(true, true);
    if (!selectedText.isEmpty())
        _searchBar->setSearchText(selectedText);

    if (_searchBar->isVisible()) {
        _searchBar->focusLineEdit();
    } else {
        searchHistory(true);
        _isSearchBarEnabled = true;
    }
}

void SessionController::enableSearchBar(bool showSearchBar)
{
    if (!_searchBar)
        return;

    if (showSearchBar && !_searchBar->isVisible()) {
        setSearchStartToWindowCurrentLine();
    }

    _searchBar->setVisible(showSearchBar);
    if (showSearchBar) {
        connect(_searchBar, SIGNAL(searchChanged(QString)), this,
                SLOT(searchTextChanged(QString)));
        connect(_searchBar, SIGNAL(searchReturnPressed(QString)), this,
                SLOT(findPreviousInHistory()));
        connect(_searchBar, SIGNAL(searchShiftPlusReturnPressed()), this,
                SLOT(findNextInHistory()));
    } else {
        disconnect(_searchBar, SIGNAL(searchChanged(QString)), this,
                   SLOT(searchTextChanged(QString)));
        disconnect(_searchBar, SIGNAL(searchReturnPressed(QString)), this,
                   SLOT(findPreviousInHistory()));
        disconnect(_searchBar, SIGNAL(searchShiftPlusReturnPressed()), this,
                   SLOT(findNextInHistory()));
        if (_view && _view->screenWindow()) {
            _view->screenWindow()->setCurrentResultLine(-1);
        }
    }
}


bool SessionController::reverseSearchChecked() const
{
    Q_ASSERT(_searchBar);

    QBitArray options = _searchBar->optionsChecked();
    return options.at(IncrementalSearchBar::ReverseSearch);
}

QRegExp SessionController::regexpFromSearchBarOptions()
{
    QBitArray options = _searchBar->optionsChecked();

    Qt::CaseSensitivity caseHandling = options.at(IncrementalSearchBar::MatchCase) ? Qt::CaseSensitive : Qt::CaseInsensitive;
    QRegExp::PatternSyntax syntax = options.at(IncrementalSearchBar::RegExp) ? QRegExp::RegExp : QRegExp::FixedString;

    QRegExp regExp(_searchBar->searchText(),  caseHandling , syntax);
    return regExp;
}

// searchHistory() may be called either as a result of clicking a menu item or
// as a result of changing the search bar widget
void SessionController::searchHistory(bool showSearchBar)
{
    enableSearchBar(showSearchBar);

    if (_searchBar) {
        if (showSearchBar) {
            removeSearchFilter();

            listenForScreenWindowUpdates();

            _searchFilter = new RegExpFilter();
            _searchFilter->setRegExp(regexpFromSearchBarOptions());
            _view->filterChain()->addFilter(_searchFilter);
            _view->processFilters();

            setFindNextPrevEnabled(true);
        } else {
            setFindNextPrevEnabled(false);

            removeSearchFilter();

            _view->setFocus(Qt::ActiveWindowFocusReason);
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
    Q_ASSERT(_view->screenWindow());

    if (_searchText == text)
        return;

    _searchText = text;

    if (text.isEmpty()) {
        _view->screenWindow()->clearSelection();
        _view->screenWindow()->scrollTo(_searchStartLine);
    }

    // update search.  this is called even when the text is
    // empty to clear the view's filters
    beginSearch(text , reverseSearchChecked() ? SearchHistoryTask::BackwardsSearch : SearchHistoryTask::ForwardsSearch);
}
void SessionController::searchCompleted(bool success)
{
    _prevSearchResultLine = _view->screenWindow()->currentResultLine();

    if (_searchBar)
        _searchBar->setFoundMatch(success);
}

void SessionController::beginSearch(const QString& text , int direction)
{
    Q_ASSERT(_searchBar);
    Q_ASSERT(_searchFilter);

    QRegExp regExp = regexpFromSearchBarOptions();
    _searchFilter->setRegExp(regExp);

    if (_searchStartLine == -1) {
        if (direction == SearchHistoryTask::ForwardsSearch) {
            setSearchStartTo(_view->screenWindow()->currentLine());
        } else {
            setSearchStartTo(_view->screenWindow()->currentLine() + _view->screenWindow()->windowLines());
        }
    }

    if (!regExp.isEmpty()) {
        _view->screenWindow()->setCurrentResultLine(-1);
        SearchHistoryTask* task = new SearchHistoryTask(this);

        connect(task, SIGNAL(completed(bool)), this, SLOT(searchCompleted(bool)));

        task->setRegExp(regExp);
        task->setSearchDirection((SearchHistoryTask::SearchDirection)direction);
        task->setAutoDelete(true);
        task->setStartLine(_searchStartLine);
        task->addScreenWindow(_session , _view->screenWindow());
        task->execute();
    } else if (text.isEmpty()) {
        searchCompleted(false);
    }

    _view->processFilters();
}
void SessionController::highlightMatches(bool highlight)
{
    if (highlight) {
        _view->filterChain()->addFilter(_searchFilter);
        _view->processFilters();
    } else {
        _view->filterChain()->removeFilter(_searchFilter);
    }

    _view->update();
}

void SessionController::searchFrom()
{
    Q_ASSERT(_searchBar);
    Q_ASSERT(_searchFilter);

    if (reverseSearchChecked()) {
        setSearchStartTo(_view->screenWindow()->lineCount());
    } else {
        setSearchStartTo(0);
    }


    beginSearch(_searchBar->searchText(), reverseSearchChecked() ? SearchHistoryTask::BackwardsSearch : SearchHistoryTask::ForwardsSearch);
}
void SessionController::findNextInHistory()
{
    Q_ASSERT(_searchBar);
    Q_ASSERT(_searchFilter);

    setSearchStartTo(_prevSearchResultLine);

    beginSearch(_searchBar->searchText(), reverseSearchChecked() ? SearchHistoryTask::BackwardsSearch : SearchHistoryTask::ForwardsSearch);
}
void SessionController::findPreviousInHistory()
{
    Q_ASSERT(_searchBar);
    Q_ASSERT(_searchFilter);

    setSearchStartTo(_prevSearchResultLine);

    beginSearch(_searchBar->searchText(), reverseSearchChecked() ? SearchHistoryTask::ForwardsSearch : SearchHistoryTask::BackwardsSearch);
}
void SessionController::changeSearchMatch()
{
    Q_ASSERT(_searchBar);
    Q_ASSERT(_searchFilter);

    // reset Selection for new case match
    _view->screenWindow()->clearSelection();
    beginSearch(_searchBar->searchText(), reverseSearchChecked() ? SearchHistoryTask::BackwardsSearch : SearchHistoryTask::ForwardsSearch);
}
void SessionController::showHistoryOptions()
{
    QScopedPointer<HistorySizeDialog> dialog(new HistorySizeDialog(QApplication::activeWindow()));
    const HistoryType& currentHistory = _session->historyType();

    if (currentHistory.isEnabled()) {
        if (currentHistory.isUnlimited()) {
            dialog->setMode(Enum::UnlimitedHistory);
        } else {
            dialog->setMode(Enum::FixedSizeHistory);
            dialog->setLineCount(currentHistory.maximumLineCount());
        }
    } else {
        dialog->setMode(Enum::NoHistory);
    }

    QPointer<Session> guard(_session);
    int result = dialog->exec();
    if (!guard)
        return;

    if (result) {
        scrollBackOptionsChanged(dialog->mode(), dialog->lineCount());
    }
}
void SessionController::sessionResizeRequest(const QSize& size)
{
    //kDebug() << "View resize requested to " << size;
    _view->setSize(size.width(), size.height());
}
void SessionController::scrollBackOptionsChanged(int mode, int lines)
{
    switch (mode) {
    case Enum::NoHistory:
        _session->setHistoryType(HistoryTypeNone());
        break;
    case Enum::FixedSizeHistory:
        _session->setHistoryType(CompactHistoryType(lines));
        break;
    case Enum::UnlimitedHistory:
        _session->setHistoryType(HistoryTypeFile());
        break;
    }
}

void SessionController::print_screen()
{
    QPrinter printer;

    QPointer<QPrintDialog> dialog = new QPrintDialog(&printer, _view);
    PrintOptions* options = new PrintOptions();

    dialog->setOptionTabs(QList<QWidget*>() << options);
    dialog->setWindowTitle(i18n("Print Shell"));
    connect(dialog, SIGNAL(accepted()), options, SLOT(saveSettings()));
    if (dialog->exec() != QDialog::Accepted)
        return;

    QPainter painter;
    painter.begin(&printer);

    KConfigGroup configGroup(KGlobal::config(), "PrintOptions");

    if (configGroup.readEntry("ScaleOutput", true)) {
        double scale = qMin(printer.pageRect().width() / static_cast<double>(_view->width()),
                            printer.pageRect().height() / static_cast<double>(_view->height()));
        painter.scale(scale, scale);
    }

    _view->printContent(painter, configGroup.readEntry("PrinterFriendly", true));
}

void SessionController::saveHistory()
{
    SessionTask* task = new SaveHistoryTask(this);
    task->setAutoDelete(true);
    task->addSession(_session);
    task->execute();
}

void SessionController::clearHistory()
{
    _session->clearHistory();
    _view->updateImage();   // To reset view scrollbar
}

void SessionController::clearHistoryAndReset()
{
    Profile::Ptr profile = SessionManager::instance()->sessionProfile(_session);
    QByteArray name = profile->defaultEncoding().toUtf8();

    Emulation* emulation = _session->emulation();
    emulation->reset();
    _session->refresh();
    _session->setCodec(QTextCodec::codecForName(name));
    clearHistory();
}

void SessionController::increaseFontSize()
{
    _view->increaseFontSize();
}

void SessionController::decreaseFontSize()
{
    _view->decreaseFontSize();
}

void SessionController::monitorActivity(bool monitor)
{
    _session->setMonitorActivity(monitor);
}
void SessionController::monitorSilence(bool monitor)
{
    _session->setMonitorSilence(monitor);
}
void SessionController::updateSessionIcon()
{
    // Visualize that the session is broadcasting to others
    if (_copyToGroup && _copyToGroup->sessions().count() > 1) {
        // Master Mode: set different icon, to warn the user to be careful
        setIcon(_broadcastIcon);
    } else {
        if (!_keepIconUntilInteraction) {
            // Not in Master Mode: use normal icon
            setIcon(_sessionIcon);
        }
    }
}
void SessionController::sessionTitleChanged()
{
    if (_sessionIconName != _session->iconName()) {
        _sessionIconName = _session->iconName();
        _sessionIcon = KIcon(_sessionIconName);
        updateSessionIcon();
    }

    QString title = _session->title(Session::DisplayedTitleRole);

    // special handling for the "%w" marker which is replaced with the
    // window title set by the shell
    title.replace("%w", _session->userTitle());
    // special handling for the "%#" marker which is replaced with the
    // number of the shell
    title.replace("%#", QString::number(_session->sessionId()));

    if (title.isEmpty())
        title = _session->title(Session::NameRole);

    setTitle(title);
    emit rawTitleChanged();
}

void SessionController::showDisplayContextMenu(const QPoint& position)
{
    // needed to make sure the popup menu is available, even if a hosting
    // application did not merge our GUI.
    if (!factory()) {
        if (!clientBuilder()) {
            setClientBuilder(new KXMLGUIBuilder(_view));
        }

        KXMLGUIFactory* factory = new KXMLGUIFactory(clientBuilder(), this);
        factory->addClient(this);
        //kDebug() << "Created xmlgui factory" << factory;
    }

    QPointer<QMenu> popup = qobject_cast<QMenu*>(factory()->container("session-popup-menu", this));
    if (popup) {
        // prepend content-specific actions such as "Open Link", "Copy Email Address" etc.
        QList<QAction*> contentActions = _view->filterActions(position);
        QAction* contentSeparator = new QAction(popup);
        contentSeparator->setSeparator(true);
        contentActions << contentSeparator;
        popup->insertActions(popup->actions().value(0, 0), contentActions);

        // always update this submenu before showing the context menu,
        // because the available search services might have changed
        // since the context menu is shown last time
        updateWebSearchMenu();

        _preventClose = true;

        if (_showMenuAction) {
            if (  _showMenuAction->isChecked() ) {
                popup->removeAction( _showMenuAction);
            } else {
                popup->insertAction(_switchProfileMenu, _showMenuAction);
            }
        }

        QAction* chosen = popup->exec(_view->mapToGlobal(position));

        // check for validity of the pointer to the popup menu
        if (popup) {
            // Remove content-specific actions
            //
            // If the close action was chosen, the popup menu will be partially
            // destroyed at this point, and the rest will be destroyed later by
            // 'chosen->trigger()'
            foreach(QAction * action, contentActions) {
                popup->removeAction(action);
            }

            delete contentSeparator;
        }

        _preventClose = false;

        if (chosen && chosen->objectName() == "close-session")
            chosen->trigger();
    } else {
        kWarning() << "Unable to display popup menu for session"
                   << _session->title(Session::NameRole)
                   << ", no GUI factory available to build the popup.";
    }
}

void SessionController::movementKeyFromSearchBarReceived(QKeyEvent *event)
{
    QCoreApplication::sendEvent(_view, event);
    setSearchStartToWindowCurrentLine();
}

void SessionController::sessionStateChanged(int state)
{
    if (state == _previousState)
        return;

    if (state == NOTIFYACTIVITY) {
        setIcon(_activityIcon);
        _keepIconUntilInteraction = true;
    } else if (state == NOTIFYSILENCE) {
        setIcon(_silenceIcon);
        _keepIconUntilInteraction = true;
    } else if (state == NOTIFYNORMAL) {
        if (_sessionIconName != _session->iconName()) {
            _sessionIconName = _session->iconName();
            _sessionIcon = KIcon(_sessionIconName);
        }

        updateSessionIcon();
    }

    _previousState = state;
}

void SessionController::zmodemDownload()
{
    QString zmodem = KStandardDirs::findExe("rz");
    if (zmodem.isEmpty()) {
        zmodem = KStandardDirs::findExe("lrz");
    }
    if (!zmodem.isEmpty()) {
        const QString path = KFileDialog::getExistingDirectory(
                                 QString(), _view,
                                 i18n("Save ZModem Download to..."));

        if (!path.isEmpty()) {
            _session->startZModem(zmodem, path, QStringList());
            return;
        }
    } else {
        KMessageBox::error(_view,
                           i18n("<p>A ZModem file transfer attempt has been detected, "
                                "but no suitable ZModem software was found on this system.</p>"
                                "<p>You may wish to install the 'rzsz' or 'lrzsz' package.</p>"));
    }
    _session->cancelZModem();
    return;
}

void SessionController::zmodemUpload()
{
    if (_session->isZModemBusy()) {
        KMessageBox::sorry(_view,
                           i18n("<p>The current session already has a ZModem file transfer in progress.</p>"));
        return;
    }
    QString zmodem = KStandardDirs::findExe("sz");
    if (zmodem.isEmpty()) {
        zmodem = KStandardDirs::findExe("lsz");
    }
    if (zmodem.isEmpty()) {
        KMessageBox::sorry(_view,
                           i18n("<p>No suitable ZModem software was found on this system.</p>"
                                "<p>You may wish to install the 'rzsz' or 'lrzsz' package.</p>"));
        return;
    }

    QStringList files = KFileDialog::getOpenFileNames(KUrl(), QString(), _view,
                        i18n("Select Files for ZModem Upload"));
    if (!files.isEmpty()) {
        _session->startZModem(zmodem, QString(), files);
    }
}

bool SessionController::isKonsolePart() const
{
    // Check to see if we are being called from Konsole or a KPart
    if (QString(qApp->metaObject()->className()) == "Konsole::Application")
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
    // TODO - think about the UI when saving multiple history sessions, if there are more than two or
    //        three then providing a URL for each one will be tedious

    // TODO - show a warning ( preferably passive ) if saving the history output fails
    //

    KFileDialog* dialog = new KFileDialog(QString(":konsole") /* check this */,
                                          QString(), QApplication::activeWindow());
    dialog->setOperationMode(KFileDialog::Saving);
    dialog->setConfirmOverwrite(true);

    QStringList mimeTypes;
    mimeTypes << "text/plain";
    mimeTypes << "text/html";
    dialog->setMimeFilter(mimeTypes, "text/plain");

    // iterate over each session in the task and display a dialog to allow the user to choose where
    // to save that session's history.
    // then start a KIO job to transfer the data from the history to the chosen URL
    foreach(const SessionPtr& session, sessions()) {
        dialog->setCaption(i18n("Save Output From %1", session->title(Session::NameRole)));

        int result = dialog->exec();

        if (result != QDialog::Accepted)
            continue;

        KUrl url = dialog->selectedUrl();

        if (!url.isValid()) {
            // UI:  Can we make this friendlier?
            KMessageBox::sorry(0 , i18n("%1 is an invalid URL, the output could not be saved.", url.url()));
            continue;
        }

        KIO::TransferJob* job = KIO::put(url,
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

        if (dialog->currentMimeFilter() == "text/html")
            jobInfo.decoder = new HTMLDecoder();
        else
            jobInfo.decoder = new PlainTextDecoder();

        _jobSession.insert(job, jobInfo);

        connect(job, SIGNAL(dataReq(KIO::Job*,QByteArray&)),
                this, SLOT(jobDataRequested(KIO::Job*,QByteArray&)));
        connect(job, SIGNAL(result(KJob*)),
                this, SLOT(jobResult(KJob*)));
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
    if (info.session) {
        // note:  when retrieving lines from the emulation,
        // the first line is at index 0.

        int sessionLines = info.session->emulation()->lineCount();

        if (sessionLines - 1 == info.lastLineFetched)
            return; // if there is no more data to transfer then stop the job

        int copyUpToLine = qMin(info.lastLineFetched + LINES_PER_REQUEST ,
                                sessionLines - 1);

        QTextStream stream(&data, QIODevice::ReadWrite);
        info.decoder->begin(&stream);
        info.session->emulation()->writeToStream(info.decoder , info.lastLineFetched + 1 , copyUpToLine);
        info.decoder->end();

        info.lastLineFetched = copyUpToLine;
    }
}
void SaveHistoryTask::jobResult(KJob* job)
{
    if (job->error()) {
        KMessageBox::sorry(0 , i18n("A problem occurred when saving the output.\n%1", job->errorString()));
    }

    TerminalCharacterDecoder * decoder = _jobSession[job].decoder;

    _jobSession.remove(job);

    delete decoder;

    // notify the world that the task is done
    emit completed(true);

    if (autoDelete())
        deleteLater();
}
void SearchHistoryTask::addScreenWindow(Session* session , ScreenWindow* searchWindow)
{
    _windows.insert(session, searchWindow);
}
void SearchHistoryTask::execute()
{
    QMapIterator< SessionPtr , ScreenWindowPtr > iter(_windows);

    while (iter.hasNext()) {
        iter.next();
        executeOnScreenWindow(iter.key() , iter.value());
    }
}

void SearchHistoryTask::executeOnScreenWindow(SessionPtr session , ScreenWindowPtr window)
{
    Q_ASSERT(session);
    Q_ASSERT(window);

    Emulation* emulation = session->emulation();

    if (!_regExp.isEmpty()) {
        int pos = -1;
        const bool forwards = (_direction == ForwardsSearch);
        const int lastLine = window->lineCount() - 1;

        int startLine;
        if (forwards && (_startLine == lastLine)) {
            startLine = 0;
        } else if (!forwards && (_startLine == 0)) {
            startLine = lastLine;
        } else {
            startLine = _startLine + (forwards ? 1 : -1);
        }

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
        const int maxDelta = qMin(window->lineCount(), 10000);
        int delta = forwards ? maxDelta : -maxDelta;

        int endLine = line;
        bool hasWrapped = false;  // set to true when we reach the top/bottom
        // of the output and continue from the other
        // end

        //loop through history in blocks of <delta> lines.
        do {
            // ensure that application does not appear to hang
            // if searching through a lengthy output
            QApplication::processEvents();

            // calculate lines to search in this iteration
            if (hasWrapped) {
                if (endLine == lastLine)
                    line = 0;
                else if (endLine == 0)
                    line = lastLine;

                endLine += delta;

                if (forwards)
                    endLine = qMin(startLine , endLine);
                else
                    endLine = qMax(startLine , endLine);
            } else {
                endLine += delta;

                if (endLine > lastLine) {
                    hasWrapped = true;
                    endLine = lastLine;
                } else if (endLine < 0) {
                    hasWrapped = true;
                    endLine = 0;
                }
            }

            decoder.begin(&searchStream);
            emulation->writeToStream(&decoder, qMin(endLine, line) , qMax(endLine, line));
            decoder.end();

            // line number search below assumes that the buffer ends with a new-line
            string.append('\n');

            if (forwards)
                pos = string.indexOf(_regExp);
            else
                pos = string.lastIndexOf(_regExp);

            //if a match is found, position the cursor on that line and update the screen
            if (pos != -1) {
                int newLines = 0;
                QList<int> linePositions = decoder.linePositions();
                while (newLines < linePositions.count() && linePositions[newLines] <= pos)
                    newLines++;

                // ignore the new line at the start of the buffer
                newLines--;

                int findPos = qMin(line, endLine) + newLines;

                highlightResult(window, findPos);

                emit completed(true);

                return;
            }

            //clear the current block of text and move to the next one
            string.clear();
            line = endLine;
        } while (startLine != endLine);

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

    //kDebug() << "Found result at line " << findPos;

    //update display to show area of history containing selection
    if ((findPos < window->currentLine()) ||
            (findPos >= (window->currentLine() + window->windowLines()))) {
        int centeredScrollPos = findPos - window->windowLines() / 2;
        if (centeredScrollPos < 0) {
            centeredScrollPos = 0;
        }

        window->scrollTo(centeredScrollPos);
    }

    window->setTrackOutput(false);
    window->notifyOutputChanged();
    window->setCurrentResultLine(findPos);
}

SearchHistoryTask::SearchHistoryTask(QObject* parent)
    : SessionTask(parent)
    , _direction(BackwardsSearch)
    , _startLine(0)
{
}
void SearchHistoryTask::setSearchDirection(SearchDirection direction)
{
    _direction = direction;
}
void SearchHistoryTask::setStartLine(int line)
{
    _startLine = line;
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

QString SessionController::userTitle() const
{
    if (_session) {
        return _session->userTitle();
    } else {
        return QString();
    }
}

#include "SessionController.moc"


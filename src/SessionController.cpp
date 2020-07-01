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

#include "ProfileManager.h"
#include "konsoledebug.h"

// Qt
#include <QApplication>
#include <QAction>
#include <QList>
#include <QMenu>
#include <QKeyEvent>
#include <QPrinter>
#include <QPrintDialog>
#include <QFileDialog>
#include <QPainter>
#include <QStandardPaths>
#include <QUrl>
#include <QIcon>

// KDE
#include <KActionMenu>
#include <KActionCollection>
#include <KLocalizedString>
#include <KMessageBox>
#include <KRun>
#include <KShell>
#include <KToolInvocation>
#include <KToggleAction>
#include <KSelectAction>
#include <KXmlGuiWindow>
#include <KXMLGUIFactory>
#include <KXMLGUIBuilder>
#include <KUriFilter>
#include <KStringHandler>
#include <KSharedConfig>
#include <KConfigGroup>
#include <KCodecAction>
#include <KNotification>

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
#include "SaveHistoryTask.h"
#include "SearchHistoryTask.h"
#include "SessionGroup.h"

// For Unix signal names
#include <csignal>

using namespace Konsole;

QSet<SessionController*> SessionController::_allControllers;
int SessionController::_lastControllerId;

SessionController::SessionController(Session* session , TerminalDisplay* view, QObject* parent)
    : ViewProperties(parent)
    , KXMLGUIClient()
    , _session(session)
    , _view(view)
    , _copyToGroup(nullptr)
    , _profileList(nullptr)
    , _sessionIcon(QIcon())
    , _sessionIconName(QString())
    , _searchFilter(nullptr)
    , _urlFilter(nullptr)
    , _fileFilter(nullptr)
    , _copyInputToAllTabsAction(nullptr)
    , _findAction(nullptr)
    , _findNextAction(nullptr)
    , _findPreviousAction(nullptr)
    , _interactionTimer(nullptr)
    , _searchStartLine(0)
    , _prevSearchResultLine(0)
    , _codecAction(nullptr)
    , _switchProfileMenu(nullptr)
    , _webSearchMenu(nullptr)
    , _listenForScreenWindowUpdates(false)
    , _preventClose(false)
    , _selectedText(QString())
    , _showMenuAction(nullptr)
    , _bookmarkValidProgramsToClear(QStringList())
    , _isSearchBarEnabled(false)
    , _editProfileDialog(nullptr)
    , _searchBar(view->searchBar())
    , _monitorProcessFinish(false)
{
    Q_ASSERT(session);
    Q_ASSERT(view);

    // handle user interface related to session (menus etc.)
    if (isKonsolePart()) {
        setComponentName(QStringLiteral("konsole"), i18n("Konsole"));
        setXMLFile(QStringLiteral("partui.rc"));
        setupCommonActions();
    } else {
        setXMLFile(QStringLiteral("sessionui.rc"));
        setupCommonActions();
        setupExtraActions();
    }

    actionCollection()->addAssociatedWidget(view);

    const QList<QAction *> actionsList = actionCollection()->actions();
    for (QAction *action : actionsList) {
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }

    setIdentifier(++_lastControllerId);
    sessionAttributeChanged();

    connect(_view, &TerminalDisplay::compositeFocusChanged, this, &SessionController::viewFocusChangeHandler);
    _view->setSessionController(this);

    // install filter on the view to highlight URLs and files
    updateFilterList(SessionManager::instance()->sessionProfile(_session));

    // listen for changes in session, we might need to change the enabled filters
    connect(ProfileManager::instance(), &Konsole::ProfileManager::profileChanged, this, &Konsole::SessionController::updateFilterList);

    // listen for session resize requests
    connect(_session, &Konsole::Session::resizeRequest, this, &Konsole::SessionController::sessionResizeRequest);

    // listen for popup menu requests
    connect(_view, &Konsole::TerminalDisplay::configureRequest, this, &Konsole::SessionController::showDisplayContextMenu);

    // move view to newest output when keystrokes occur
    connect(_view, &Konsole::TerminalDisplay::keyPressedSignal, this, &Konsole::SessionController::trackOutput);

    // listen to activity / silence notifications from session
    connect(_session, &Konsole::Session::notificationsChanged, this, &Konsole::SessionController::sessionNotificationsChanged);
    // listen to title and icon changes
    connect(_session, &Konsole::Session::sessionAttributeChanged, this, &Konsole::SessionController::sessionAttributeChanged);
    connect(_session, &Konsole::Session::readOnlyChanged, this, &Konsole::SessionController::sessionReadOnlyChanged);

    connect(this, &Konsole::SessionController::tabRenamedByUser, _session, &Konsole::Session::tabTitleSetByUser);
    connect(this, &Konsole::SessionController::tabColoredByUser, _session, &Konsole::Session::tabColorSetByUser);

    connect(_session , &Konsole::Session::currentDirectoryChanged , this , &Konsole::SessionController::currentDirectoryChanged);

    // listen for color changes
    connect(_session, &Konsole::Session::changeBackgroundColorRequest, _view, &Konsole::TerminalDisplay::setBackgroundColor);
    connect(_session, &Konsole::Session::changeForegroundColorRequest, _view, &Konsole::TerminalDisplay::setForegroundColor);

    // update the title when the session starts
    connect(_session, &Konsole::Session::started, this, &Konsole::SessionController::snapshot);

    // listen for output changes to set activity flag
    connect(_session->emulation(), &Konsole::Emulation::outputChanged, this, &Konsole::SessionController::fireActivity);

    // listen for detection of ZModem transfer
    connect(_session, &Konsole::Session::zmodemDownloadDetected, this, &Konsole::SessionController::zmodemDownload);
    connect(_session, &Konsole::Session::zmodemUploadDetected, this, &Konsole::SessionController::zmodemUpload);

    // listen for flow control status changes
    connect(_session, &Konsole::Session::flowControlEnabledChanged, _view, &Konsole::TerminalDisplay::setFlowControlWarningEnabled);
    _view->setFlowControlWarningEnabled(_session->flowControlEnabled());

    // take a snapshot of the session state every so often when
    // user activity occurs
    //
    // the timer is owned by the session so that it will be destroyed along
    // with the session
    _interactionTimer = new QTimer(_session);
    _interactionTimer->setSingleShot(true);
    _interactionTimer->setInterval(500);
    connect(_interactionTimer, &QTimer::timeout, this, &Konsole::SessionController::snapshot);
    connect(_view, &Konsole::TerminalDisplay::compositeFocusChanged,
            this, [this](bool focused) { if (focused) { interactionHandler(); }});
    connect(_view, &Konsole::TerminalDisplay::keyPressedSignal,
            this, &Konsole::SessionController::interactionHandler);

    // take a snapshot of the session state periodically in the background
    auto backgroundTimer = new QTimer(_session);
    backgroundTimer->setSingleShot(false);
    backgroundTimer->setInterval(2000);
    connect(backgroundTimer, &QTimer::timeout, this, &Konsole::SessionController::snapshot);
    backgroundTimer->start();

    // xterm '10;?' request
    connect(_session, &Konsole::Session::getForegroundColor,
            this, &Konsole::SessionController::sendForegroundColor);
    // xterm '11;?' request
    connect(_session, &Konsole::Session::getBackgroundColor,
            this, &Konsole::SessionController::sendBackgroundColor);

    _allControllers.insert(this);

    // A list of programs that accept Ctrl+C to clear command line used
    // before outputting bookmark.
    _bookmarkValidProgramsToClear = QStringList({
        QStringLiteral("bash"),
        QStringLiteral("fish"),
        QStringLiteral("sh"),
        QStringLiteral("tcsh"),
        QStringLiteral("zsh")
    });

    setupSearchBar();
    _searchBar->setVisible(_isSearchBarEnabled);
}

SessionController::~SessionController()
{
    _allControllers.remove(this);

    if (!_editProfileDialog.isNull()) {
        _editProfileDialog->deleteLater();
    }
    if(factory() != nullptr) {
        factory()->removeClient(this);
    }
}
void SessionController::trackOutput(QKeyEvent* event)
{
    Q_ASSERT(_view->screenWindow());

    // Qt has broken something, so we can't rely on just checking if certain
    // keys are passed as modifiers anymore.
    const int key = event->key();
    const bool shouldNotTriggerScroll =
        key == Qt::Key_Super_L      ||
        key == Qt::Key_Super_R      ||
        key == Qt::Key_Hyper_L      ||
        key == Qt::Key_Hyper_R      ||
        key == Qt::Key_Shift        ||
        key == Qt::Key_Control      ||
        key == Qt::Key_Meta         ||
        key == Qt::Key_Alt          ||
        key == Qt::Key_AltGr        ||
        key == Qt::Key_CapsLock     ||
        key == Qt::Key_NumLock      ||
        key == Qt::Key_ScrollLock;


    // Only jump to the bottom if the user actually typed something in,
    // not if the user e. g. just pressed a modifier.
    if (event->text().isEmpty() && ((event->modifiers() != 0u) || shouldNotTriggerScroll)) {
        return;
    }

    _view->screenWindow()->setTrackOutput(true);
}

void SessionController::viewFocusChangeHandler(bool focused)
{
    if (focused) {
        // notify the world that the view associated with this session has been focused
        // used by the view manager to update the title of the MainWindow widget containing the view
        emit viewFocused(this);

        // when the view is focused, set bell events from the associated session to be delivered
        // by the focused view

        // first, disconnect any other views which are listening for bell signals from the session
        disconnect(_session, &Konsole::Session::bellRequest, nullptr, nullptr);
        // second, connect the newly focused view to listen for the session's bell signal
        connect(_session, &Konsole::Session::bellRequest, _view, &Konsole::TerminalDisplay::bell);

        if ((_copyInputToAllTabsAction != nullptr) && _copyInputToAllTabsAction->isChecked()) {
            // A session with "Copy To All Tabs" has come into focus:
            // Ensure that newly created sessions are included in _copyToGroup!
            copyInputToAllTabs();
        }
    }
}

void SessionController::interactionHandler()
{
    _interactionTimer->start();
}

void SessionController::snapshot()
{
    Q_ASSERT(!_session.isNull());

    QString title = _session->getDynamicTitle();
    title         = title.simplified();

    // Visualize that the session is broadcasting to others
    if ((_copyToGroup != nullptr) && _copyToGroup->sessions().count() > 1) {
        title.append(QLatin1Char('*'));
    }

    // use the fallback title if needed
    if (title.isEmpty()) {
        title = _session->title(Session::NameRole);
    }

    QColor color = _session->color();
    // use the fallback color if needed
    if (!color.isValid()) {
        color = QColor(QColor::Invalid);
    }

    // apply new title
    _session->setTitle(Session::DisplayedTitleRole, title);

    // apply new color
    _session->setColor(color);

    // check if foreground process ended and notify if this option was requested
    if (_monitorProcessFinish) {
        bool isForegroundProcessActive = _session->isForegroundProcessActive();
        if (!_previousForegroundProcessName.isNull() && !isForegroundProcessActive) {
            KNotification::event(_session->hasFocus() ? QStringLiteral("ProcessFinished") : QStringLiteral("ProcessFinishedHidden"),
                                 i18n("The process '%1' has finished running in session '%2'", _previousForegroundProcessName, _session->nameTitle()),
                                 QPixmap(),
                                 QApplication::activeWindow(),
                                 KNotification::CloseWhenWidgetActivated);
        }
        _previousForegroundProcessName = isForegroundProcessActive ? _session->foregroundProcessName() : QString();
    }

    // do not forget icon
    updateSessionIcon();
}

QString SessionController::currentDir() const
{
    return _session->currentWorkingDirectory();
}

QUrl SessionController::url() const
{
    return _session->getUrl();
}

void SessionController::rename()
{
    renameSession();
}

void SessionController::openUrl(const QUrl& url)
{
    // Clear shell's command line
    if (!_session->isForegroundProcessActive()
            && _bookmarkValidProgramsToClear.contains(_session->foregroundProcessName())) {
        _session->sendTextToTerminal(QChar(0x03), QLatin1Char('\n')); // Ctrl+C
    }

    // handle local paths
    if (url.isLocalFile()) {
        QString path = url.toLocalFile();
        _session->sendTextToTerminal(QStringLiteral("cd ") + KShell::quoteArg(path), QLatin1Char('\r'));
    } else if (url.scheme().isEmpty()) {
        // QUrl couldn't parse what the user entered into the URL field
        // so just dump it to the shell
        // If you change this, change it also in autotests/BookMarkTest.cpp
        QString command = QUrl::fromPercentEncoding(url.toEncoded());
        if (!command.isEmpty()) {
            _session->sendTextToTerminal(command, QLatin1Char('\r'));
        }
    } else if (url.scheme() == QLatin1String("ssh")) {
        QString sshCommand = QStringLiteral("ssh ");

        if (url.port() > -1) {
            sshCommand += QStringLiteral("-p %1 ").arg(url.port());
        }
        if (!url.userName().isEmpty()) {
            sshCommand += (url.userName() + QLatin1Char('@'));
        }
        if (!url.host().isEmpty()) {
            sshCommand += url.host();
        }
        _session->sendTextToTerminal(sshCommand, QLatin1Char('\r'));

    } else if (url.scheme() == QLatin1String("telnet")) {
        QString telnetCommand = QStringLiteral("telnet ");

        if (!url.userName().isEmpty()) {
            telnetCommand += QStringLiteral("-l %1 ").arg(url.userName());
        }
        if (!url.host().isEmpty()) {
            telnetCommand += (url.host() + QLatin1Char(' '));
        }
        if (url.port() > -1) {
            telnetCommand += QString::number(url.port());
        }

        _session->sendTextToTerminal(telnetCommand, QLatin1Char('\r'));

    } else {
        //TODO Implement handling for other Url types

        KMessageBox::sorry(_view->window(),
                           i18n("Konsole does not know how to open the bookmark: ") +
                           url.toDisplayString());

        qCDebug(KonsoleDebug) << "Unable to open bookmark at url" << url << ", I do not know"
                   << " how to handle the protocol " << url.scheme();
    }
}

void SessionController::setupPrimaryScreenSpecificActions(bool use)
{
    KActionCollection* collection = actionCollection();
    QAction* clearAction = collection->action(QStringLiteral("clear-history"));
    QAction* resetAction = collection->action(QStringLiteral("clear-history-and-reset"));
    QAction* selectAllAction = collection->action(QStringLiteral("select-all"));
    QAction* selectLineAction = collection->action(QStringLiteral("select-line"));

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
    QAction* copyAction = actionCollection()->action(QStringLiteral("edit_copy"));
    QAction* copyContextMenu = actionCollection()->action(QStringLiteral("edit_copy_contextmenu"));
    // copy action is meaningful only when some text is selected.
    copyAction->setEnabled(!selectedText.isEmpty());
    copyContextMenu->setVisible(!selectedText.isEmpty());
}

void SessionController::updateWebSearchMenu()
{
    // reset
    _webSearchMenu->setVisible(false);
    _webSearchMenu->menu()->clear();

    if (_selectedText.isEmpty()) {
        return;
    }

    QString searchText = _selectedText;
    searchText = searchText.replace(QLatin1Char('\n'), QLatin1Char(' ')).replace(QLatin1Char('\r'), QLatin1Char(' ')).simplified();

    if (searchText.isEmpty()) {
        return;
    }

    // Is 'Enable Web shortcuts' checked in System Settings?
    KSharedConfigPtr kuriikwsConfig = KSharedConfig::openConfig(QStringLiteral("kuriikwsfilterrc"));
    if (!kuriikwsConfig->group("General").readEntry("EnableWebShortcuts", true)) {
        return;
    }

    KUriFilterData filterData(searchText);
    filterData.setSearchFilteringOptions(KUriFilterData::RetrievePreferredSearchProvidersOnly);

    if (KUriFilter::self()->filterSearchUri(filterData, KUriFilter::NormalTextFilter)) {
        const QStringList searchProviders = filterData.preferredSearchProviders();
        if (!searchProviders.isEmpty()) {
            _webSearchMenu->setText(i18n("Search for '%1' with",  KStringHandler::rsqueeze(searchText, 16)));

            QAction* action = nullptr;

            for (const QString &searchProvider : searchProviders) {
                action = new QAction(searchProvider, _webSearchMenu);
                action->setIcon(QIcon::fromTheme(filterData.iconNameForPreferredSearchProvider(searchProvider)));
                action->setData(filterData.queryForPreferredSearchProvider(searchProvider));
                connect(action, &QAction::triggered, this, &Konsole::SessionController::handleWebShortcutAction);
                _webSearchMenu->addAction(action);
            }

            _webSearchMenu->addSeparator();

            action = new QAction(i18n("Configure Web Shortcuts..."), _webSearchMenu);
            action->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
            connect(action, &QAction::triggered, this, &Konsole::SessionController::configureWebShortcuts);
            _webSearchMenu->addAction(action);

            _webSearchMenu->setVisible(true);
        }
    }
}

void SessionController::handleWebShortcutAction()
{
    auto * action = qobject_cast<QAction*>(sender());
    if (action == nullptr) {
        return;
    }

    KUriFilterData filterData(action->data().toString());

    if (KUriFilter::self()->filterUri(filterData, { QStringLiteral("kurisearchfilter") })) {
        const QUrl& url = filterData.uri();
        new KRun(url, QApplication::activeWindow());
    }
}

void SessionController::configureWebShortcuts()
{
    KToolInvocation::kdeinitExec(QStringLiteral("kcmshell5"), { QStringLiteral("webshortcuts") });
}

void SessionController::sendSignal(QAction* action)
{
    const auto signal = action->data().toInt();
    _session->sendSignal(signal);
}

void SessionController::sendForegroundColor()
{
    const QColor c = _view->getForegroundColor();
    _session->reportForegroundColor(c);
}

void SessionController::sendBackgroundColor()
{
    const QColor c = _view->getBackgroundColor();
    _session->reportBackgroundColor(c);
}

void SessionController::toggleReadOnly()
{
    auto *action = qobject_cast<QAction*>(sender());
    if (action != nullptr) {
        bool readonly = !isReadOnly();
        _session->setReadOnly(readonly);
    }
}

void SessionController::removeSearchFilter()
{
    if (_searchFilter == nullptr) {
        return;
    }

    _view->filterChain()->removeFilter(_searchFilter);
    delete _searchFilter;
    _searchFilter = nullptr;
}

void SessionController::setupSearchBar()
{
    connect(_searchBar, &Konsole::IncrementalSearchBar::unhandledMovementKeyPressed, this, &Konsole::SessionController::movementKeyFromSearchBarReceived);
    connect(_searchBar, &Konsole::IncrementalSearchBar::closeClicked, this, &Konsole::SessionController::searchClosed);
    connect(_searchBar, &Konsole::IncrementalSearchBar::searchFromClicked, this, &Konsole::SessionController::searchFrom);
    connect(_searchBar, &Konsole::IncrementalSearchBar::findNextClicked, this, &Konsole::SessionController::findNextInHistory);
    connect(_searchBar, &Konsole::IncrementalSearchBar::findPreviousClicked, this, &Konsole::SessionController::findPreviousInHistory);
    connect(_searchBar, &Konsole::IncrementalSearchBar::highlightMatchesToggled , this , &Konsole::SessionController::highlightMatches);
    connect(_searchBar, &Konsole::IncrementalSearchBar::matchCaseToggled, this, &Konsole::SessionController::changeSearchMatch);
    connect(_searchBar, &Konsole::IncrementalSearchBar::matchRegExpToggled, this, &Konsole::SessionController::changeSearchMatch);
}

void SessionController::setShowMenuAction(QAction* action)
{
    _showMenuAction = action;
}

void SessionController::setupCommonActions()
{
    KActionCollection* collection = actionCollection();

    // Close Session
    QAction* action = collection->addAction(QStringLiteral("close-session"), this, SLOT(closeSession()));
    action->setText(i18n("&Close Session"));

    action->setIcon(QIcon::fromTheme(QStringLiteral("tab-close")));
    collection->setDefaultShortcut(action, Konsole::ACCEL + Qt::SHIFT + Qt::Key_W);

    // Open Browser
    action = collection->addAction(QStringLiteral("open-browser"), this, SLOT(openBrowser()));
    action->setText(i18n("Open File Manager"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("system-file-manager")));

    // Copy and Paste
    action = KStandardAction::copy(this, &SessionController::copy, collection);
#ifdef Q_OS_MACOS
    // Don't use the Konsole::ACCEL const here, we really want the Command key (Qt::META)
    // TODO: check what happens if we leave it to Qt to assign the default?
    collection->setDefaultShortcut(action, Qt::META + Qt::Key_C);
#else
    collection->setDefaultShortcut(action, Konsole::ACCEL + Qt::SHIFT + Qt::Key_C);
#endif
    // disabled at first, since nothing has been selected now
    action->setEnabled(false);

    // We need a different QAction on the context menu because one will be disabled when there's no selection,
    // other will be hidden.
    action = collection->addAction(QStringLiteral("edit_copy_contextmenu"));
    action->setText(i18n("Copy"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    action->setVisible(false);
    connect(action, &QAction::triggered, this, &SessionController::copy);

    action = KStandardAction::paste(this, SLOT(paste()), collection);
    QList<QKeySequence> pasteShortcut;
#ifdef Q_OS_MACOS
    pasteShortcut.append(QKeySequence(Qt::META + Qt::Key_V));
    // No Insert key on Mac keyboards
#else
    pasteShortcut.append(QKeySequence(Konsole::ACCEL + Qt::SHIFT + Qt::Key_V));
    pasteShortcut.append(QKeySequence(Qt::SHIFT + Qt::Key_Insert));
#endif
    collection->setDefaultShortcuts(action, pasteShortcut);

    action = collection->addAction(QStringLiteral("paste-selection"), this, SLOT(pasteFromX11Selection()));
    action->setText(i18n("Paste Selection"));
#ifdef Q_OS_MACOS
    collection->setDefaultShortcut(action, Qt::META + Qt::SHIFT + Qt::Key_V);
#else
    collection->setDefaultShortcut(action, Konsole::ACCEL + Qt::SHIFT + Qt::Key_Insert);
#endif

    _webSearchMenu = new KActionMenu(i18n("Web Search"), this);
    _webSearchMenu->setIcon(QIcon::fromTheme(QStringLiteral("preferences-web-browser-shortcuts")));
    _webSearchMenu->setVisible(false);
    collection->addAction(QStringLiteral("web-search"), _webSearchMenu);


    action = collection->addAction(QStringLiteral("select-all"), this, SLOT(selectAll()));
    action->setText(i18n("&Select All"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-select-all")));

    action = collection->addAction(QStringLiteral("select-line"), this, SLOT(selectLine()));
    action->setText(i18n("Select &Line"));

    action = KStandardAction::saveAs(this, SLOT(saveHistory()), collection);
    action->setText(i18n("Save Output &As..."));
#ifdef Q_OS_MACOS
    action->setShortcut(QKeySequence(Qt::META + Qt::Key_S));
#endif

    action = KStandardAction::print(this, SLOT(print_screen()), collection);
    action->setText(i18n("&Print Screen..."));
    collection->setDefaultShortcut(action, Konsole::ACCEL + Qt::SHIFT + Qt::Key_P);

    action = collection->addAction(QStringLiteral("adjust-history"), this, SLOT(showHistoryOptions()));
    action->setText(i18n("Adjust Scrollback..."));
    action->setIcon(QIcon::fromTheme(QStringLiteral("configure")));

    action = collection->addAction(QStringLiteral("clear-history"), this, SLOT(clearHistory()));
    action->setText(i18n("Clear Scrollback"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear-history")));

    action = collection->addAction(QStringLiteral("clear-history-and-reset"), this, SLOT(clearHistoryAndReset()));
    action->setText(i18n("Clear Scrollback and Reset"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear-history")));
    collection->setDefaultShortcut(action, Konsole::ACCEL + Qt::SHIFT + Qt::Key_K);

    // Profile Options
    action = collection->addAction(QStringLiteral("edit-current-profile"), this, SLOT(editCurrentProfile()));
    action->setText(i18n("Edit Current Profile..."));
    action->setIcon(QIcon::fromTheme(QStringLiteral("document-properties")));

    _switchProfileMenu = new KActionMenu(i18n("Switch Profile"), this);
    collection->addAction(QStringLiteral("switch-profile"), _switchProfileMenu);
    connect(_switchProfileMenu->menu(), &QMenu::aboutToShow, this, &Konsole::SessionController::prepareSwitchProfileMenu);

    // History
    _findAction = KStandardAction::find(this, SLOT(searchBarEvent()), collection);

    _findNextAction = KStandardAction::findNext(this, SLOT(findNextInHistory()), collection);
    _findNextAction->setEnabled(false);

    _findPreviousAction = KStandardAction::findPrev(this, SLOT(findPreviousInHistory()), collection);
    _findPreviousAction->setEnabled(false);

#ifdef Q_OS_MACOS
    collection->setDefaultShortcut(_findAction, Qt::META + Qt::Key_F);
    collection->setDefaultShortcut(_findNextAction, Qt::META + Qt::Key_G);
    collection->setDefaultShortcut(_findPreviousAction, Qt::META + Qt::SHIFT + Qt::Key_G);
#else
    collection->setDefaultShortcut(_findAction, Konsole::ACCEL + Qt::SHIFT + Qt::Key_F);
    collection->setDefaultShortcut(_findNextAction, Qt::Key_F3);
    collection->setDefaultShortcut(_findPreviousAction, Qt::SHIFT + Qt::Key_F3);
#endif

    // Character Encoding
    _codecAction = new KCodecAction(i18n("Set &Encoding"), this);
    _codecAction->setIcon(QIcon::fromTheme(QStringLiteral("character-set")));
    collection->addAction(QStringLiteral("set-encoding"), _codecAction);
    _codecAction->setCurrentCodec(QString::fromUtf8(_session->codec()));
    connect(_session, &Konsole::Session::sessionCodecChanged, this, &Konsole::SessionController::updateCodecAction);
    connect(_codecAction,
            QOverload<QTextCodec*>::of(&KCodecAction::triggered), this,
            &Konsole::SessionController::changeCodec);

    // Read-only
    action = collection->addAction(QStringLiteral("view-readonly"), this, SLOT(toggleReadOnly()));
    action->setText(i18nc("@item:inmenu A read only (locked) session", "Read-only"));
    action->setCheckable(true);
    updateReadOnlyActionStates();
}

void SessionController::setupExtraActions()
{
    KActionCollection* collection = actionCollection();

    // Rename Session
    QAction* action = collection->addAction(QStringLiteral("rename-session"), this, SLOT(renameSession()));
    action->setText(i18n("&Current Tab Settings..."));
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-rename")));
    collection->setDefaultShortcut(action, Konsole::ACCEL + Qt::ALT + Qt::Key_S);

    // Copy input to ==> all tabs
    auto* copyInputToAllTabsAction = collection->add<KToggleAction>(QStringLiteral("copy-input-to-all-tabs"));
    copyInputToAllTabsAction->setText(i18n("&All Tabs in Current Window"));
    copyInputToAllTabsAction->setData(CopyInputToAllTabsMode);
    // this action is also used in other place, so remember it
    _copyInputToAllTabsAction = copyInputToAllTabsAction;

    // Copy input to ==> selected tabs
    auto* copyInputToSelectedTabsAction = collection->add<KToggleAction>(QStringLiteral("copy-input-to-selected-tabs"));
    copyInputToSelectedTabsAction->setText(i18n("&Select Tabs..."));
    collection->setDefaultShortcut(copyInputToSelectedTabsAction, Konsole::ACCEL + Qt::SHIFT + Qt::Key_Period);
    copyInputToSelectedTabsAction->setData(CopyInputToSelectedTabsMode);

    // Copy input to ==> none
    auto* copyInputToNoneAction = collection->add<KToggleAction>(QStringLiteral("copy-input-to-none"));
    copyInputToNoneAction->setText(i18nc("@action:inmenu Do not select any tabs", "&None"));
    collection->setDefaultShortcut(copyInputToNoneAction, Konsole::ACCEL + Qt::SHIFT + Qt::Key_Slash);
    copyInputToNoneAction->setData(CopyInputToNoneMode);
    copyInputToNoneAction->setChecked(true); // the default state

    // The "Copy Input To" submenu
    // The above three choices are represented as combo boxes
    auto* copyInputActions = collection->add<KSelectAction>(QStringLiteral("copy-input-to"));
    copyInputActions->setText(i18n("Copy Input To"));
    copyInputActions->addAction(copyInputToAllTabsAction);
    copyInputActions->addAction(copyInputToSelectedTabsAction);
    copyInputActions->addAction(copyInputToNoneAction);
    connect(copyInputActions,
            QOverload<QAction*>::of(&KSelectAction::triggered), this,
            &Konsole::SessionController::copyInputActionsTriggered);

    action = collection->addAction(QStringLiteral("zmodem-upload"), this, SLOT(zmodemUpload()));
    action->setText(i18n("&ZModem Upload..."));
    action->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
    collection->setDefaultShortcut(action, Konsole::ACCEL + Qt::ALT + Qt::Key_U);

    // Monitor
    KToggleAction* toggleAction = new KToggleAction(i18n("Monitor for &Activity"), this);
    collection->setDefaultShortcut(toggleAction, Konsole::ACCEL + Qt::SHIFT + Qt::Key_A);
    action = collection->addAction(QStringLiteral("monitor-activity"), toggleAction);
    connect(action, &QAction::toggled, this, &Konsole::SessionController::monitorActivity);

    toggleAction = new KToggleAction(i18n("Monitor for &Silence"), this);
    collection->setDefaultShortcut(toggleAction, Konsole::ACCEL + Qt::SHIFT + Qt::Key_I);
    action = collection->addAction(QStringLiteral("monitor-silence"), toggleAction);
    connect(action, &QAction::toggled, this, &Konsole::SessionController::monitorSilence);

    toggleAction = new KToggleAction(i18n("Monitor for Process Finishing"), this);
    action = collection->addAction(QStringLiteral("monitor-process-finish"), toggleAction);
    connect(action, &QAction::toggled, this, &Konsole::SessionController::monitorProcessFinish);

    // Text Size
    action = collection->addAction(QStringLiteral("enlarge-font"), this, SLOT(increaseFontSize()));
    action->setText(i18n("Enlarge Font"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("format-font-size-more")));
    QList<QKeySequence> enlargeFontShortcut;
    enlargeFontShortcut.append(QKeySequence(Konsole::ACCEL + Qt::Key_Plus));
    enlargeFontShortcut.append(QKeySequence(Konsole::ACCEL + Qt::Key_Equal));
    collection->setDefaultShortcuts(action, enlargeFontShortcut);

    action = collection->addAction(QStringLiteral("shrink-font"), this, SLOT(decreaseFontSize()));
    action->setText(i18n("Shrink Font"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("format-font-size-less")));
    collection->setDefaultShortcut(action, Konsole::ACCEL + Qt::Key_Minus);

    action = collection->addAction(QStringLiteral("reset-font-size"), this, SLOT(resetFontSize()));
    action->setText(i18n("Reset Font Size"));
    collection->setDefaultShortcut(action, Konsole::ACCEL + Qt::ALT + Qt::Key_0);

    // Send signal
    auto* sendSignalActions = collection->add<KSelectAction>(QStringLiteral("send-signal"));
    sendSignalActions->setText(i18n("Send Signal"));
    connect(sendSignalActions,
            QOverload<QAction*>::of(&KSelectAction::triggered), this,
            &Konsole::SessionController::sendSignal);

    action = collection->addAction(QStringLiteral("sigstop-signal"));
    action->setText(i18n("&Suspend Task")   + QStringLiteral(" (STOP)"));
    action->setData(SIGSTOP);
    sendSignalActions->addAction(action);

    action = collection->addAction(QStringLiteral("sigcont-signal"));
    action->setText(i18n("&Continue Task")  + QStringLiteral(" (CONT)"));
    action->setData(SIGCONT);
    sendSignalActions->addAction(action);

    action = collection->addAction(QStringLiteral("sighup-signal"));
    action->setText(i18n("&Hangup")         + QStringLiteral(" (HUP)"));
    action->setData(SIGHUP);
    sendSignalActions->addAction(action);

    action = collection->addAction(QStringLiteral("sigint-signal"));
    action->setText(i18n("&Interrupt Task") + QStringLiteral(" (INT)"));
    action->setData(SIGINT);
    sendSignalActions->addAction(action);

    action = collection->addAction(QStringLiteral("sigterm-signal"));
    action->setText(i18n("&Terminate Task") + QStringLiteral(" (TERM)"));
    action->setData(SIGTERM);
    sendSignalActions->addAction(action);

    action = collection->addAction(QStringLiteral("sigkill-signal"));
    action->setText(i18n("&Kill Task")      + QStringLiteral(" (KILL)"));
    action->setData(SIGKILL);
    sendSignalActions->addAction(action);

    action = collection->addAction(QStringLiteral("sigusr1-signal"));
    action->setText(i18n("User Signal &1")   + QStringLiteral(" (USR1)"));
    action->setData(SIGUSR1);
    sendSignalActions->addAction(action);

    action = collection->addAction(QStringLiteral("sigusr2-signal"));
    action->setText(i18n("User Signal &2")   + QStringLiteral(" (USR2)"));
    action->setData(SIGUSR2);
    sendSignalActions->addAction(action);
}

void SessionController::switchProfile(const Profile::Ptr &profile)
{
    SessionManager::instance()->setSessionProfile(_session, profile);
    updateFilterList(profile);
}

void SessionController::prepareSwitchProfileMenu()
{
    if (_switchProfileMenu->menu()->isEmpty()) {
        _profileList = new ProfileList(false, this);
        connect(_profileList, &Konsole::ProfileList::profileSelected, this, &Konsole::SessionController::switchProfile);
    }

    _switchProfileMenu->menu()->clear();
    _switchProfileMenu->menu()->addActions(_profileList->actions());
}
void SessionController::updateCodecAction(QTextCodec *codec)
{
    _codecAction->setCurrentCodec(codec);
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
    for (SessionController *controller : qAsConst(_allControllers)) {
        if ( (controller->profileDialogPointer() != nullptr)
             && controller->profileDialogPointer()->isVisible()
             && (controller->profileDialogPointer()->lookupProfile()
                 == SessionManager::instance()->sessionProfile(_session)) ) {
            controller->profileDialogPointer()->close();
        }
    }

    // NOTE bug311270: For to prevent the crash, the profile must be reset.
    if (!_editProfileDialog.isNull()) {
        // exists but not visible
        _editProfileDialog->deleteLater();
    }

    _editProfileDialog = new EditProfileDialog(QApplication::activeWindow());
    _editProfileDialog->setProfile(SessionManager::instance()->sessionProfile(_session));
    _editProfileDialog->show();
}

void SessionController::renameSession()
{
    const QString &sessionLocalTabTitleFormat = _session->tabTitleFormat(Session::LocalTabTitle);
    const QString &sessionRemoteTabTitleFormat = _session->tabTitleFormat(Session::RemoteTabTitle);
    const QColor &sessionTabColor = _session->color();

    QScopedPointer<RenameTabDialog> dialog(new RenameTabDialog(QApplication::activeWindow()));
    dialog->setTabTitleText(sessionLocalTabTitleFormat);
    dialog->setRemoteTabTitleText(sessionRemoteTabTitleFormat);
    dialog->setColor(sessionTabColor);

    if (_session->isRemote()) {
        dialog->focusRemoteTabTitleText();
    } else {
        dialog->focusTabTitleText();
    }

    QPointer<Session> guard(_session);
    int result = dialog->exec();
    if (guard.isNull()) {
        return;
    }

    if (result != 0) {
        const QString &tabTitle = dialog->tabTitleText();
        const QString &remoteTabTitle = dialog->remoteTabTitleText();
        const QColor &tabColor = dialog->color();

        if (tabTitle != sessionLocalTabTitleFormat) {
            _session->setTabTitleFormat(Session::LocalTabTitle, tabTitle);
            emit tabRenamedByUser(true);
            // trigger an update of the tab text
            snapshot();
        }

        if(remoteTabTitle != sessionRemoteTabTitleFormat) {
            _session->setTabTitleFormat(Session::RemoteTabTitle, remoteTabTitle);
            emit tabRenamedByUser(true);
            snapshot();
        }

        if (tabColor != sessionTabColor) {
            _session->setColor(tabColor);
            emit tabColoredByUser(true);
            snapshot();
        }
    }
}

// This is called upon Menu->Close Sesssion and right-click on tab->Close Tab
bool SessionController::confirmClose() const
{
    if (_session->isForegroundProcessActive()) {
        QString title = _session->foregroundProcessName();

        // hard coded for now.  In future make it possible for the user to specify which programs
        // are ignored when considering whether to display a confirmation
        QStringList ignoreList;
        ignoreList << QString::fromUtf8(qgetenv("SHELL")).section(QLatin1Char('/'), -1);
        if (ignoreList.contains(title)) {
            return true;
        }

        QString question;
        if (title.isEmpty()) {
            question = i18n("A program is currently running in this session."
                            "  Are you sure you want to close it?");
        } else {
            question = i18n("The program '%1' is currently running in this session."
                            "  Are you sure you want to close it?", title);
        }

        int result = KMessageBox::warningYesNo(_view->window(),
                                               question,
                                               i18n("Confirm Close"),
                                               KStandardGuiItem::yes(),
                                               KStandardGuiItem::no(),
                                               QStringLiteral("CloseSingleTab"));
        return result == KMessageBox::Yes;
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
        ignoreList << QString::fromUtf8(qgetenv("SHELL")).section(QLatin1Char('/'), -1);
        if (ignoreList.contains(title)) {
            return true;
        }

        QString question;
        if (title.isEmpty()) {
            question = i18n("A program in this session would not die."
                            "  Are you sure you want to kill it by force?");
        } else {
            question = i18n("The program '%1' is in this session would not die."
                            "  Are you sure you want to kill it by force?", title);
        }

        int result = KMessageBox::warningYesNo(_view->window(), question, i18n("Confirm Close"));
        return result == KMessageBox::Yes;
    }
    return true;
}
void SessionController::closeSession()
{
    if (_preventClose) {
        return;
    }

    if (!confirmClose()) {
        return;
    }

    if (!_session->closeInNormalWay()) {
        if (!confirmForceClose()) {
            return;
        }

        if (!_session->closeInForceWay()) {
            qCDebug(KonsoleDebug) << "Konsole failed to close a session in any way.";
            return;
        }
    }

    if (factory()) {
        factory()->removeClient(this);
    }
}

// Trying to open a remote Url may produce unexpected results.
// Therefore, if a remote url, open the user's home path.
// TODO consider: 1) disable menu upon remote session
//   2) transform url to get the desired result (ssh -> sftp, etc)
void SessionController::openBrowser()
{
    const QUrl currentUrl = url();

    if (currentUrl.isLocalFile()) {
        new KRun(currentUrl, QApplication::activeWindow(), true);
    } else {
        new KRun(QUrl::fromLocalFile(QDir::homePath()), QApplication::activeWindow(), true);
    }
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
    _view->selectAll();
}
void SessionController::selectLine()
{
    _view->selectCurrentLine();
}
static const KXmlGuiWindow* findWindow(const QObject* object)
{
    // Walk up the QObject hierarchy to find a KXmlGuiWindow.
    while (object != nullptr) {
        const auto* window = qobject_cast<const KXmlGuiWindow*>(object);
        if (window != nullptr) {
            return(window);
        }
        object = object->parent();
    }
    return(nullptr);
}

static bool hasTerminalDisplayInSameWindow(const Session* session, const KXmlGuiWindow* window)
{
    // Iterate all TerminalDisplays of this Session ...
    const QList<TerminalDisplay *> views = session->views();
    for (const TerminalDisplay *terminalDisplay : views) {
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
    const auto mode = action->data().toInt();

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
    if (_copyToGroup == nullptr) {
        _copyToGroup = new SessionGroup(this);
    }

    // Find our window ...
    const KXmlGuiWindow* myWindow = findWindow(_view);

    const QList<Session *> sessionsList = SessionManager::instance()->sessions();
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QSet<Session*> group(sessionsList.begin(), sessionsList.end());
#else
    QSet<Session*> group = QSet<Session*>::fromList(sessionsList);
#endif
    for (auto session : group) {
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
    emit copyInputChanged(this);
}

void SessionController::copyInputToSelectedTabs()
{
    if (_copyToGroup == nullptr) {
        _copyToGroup = new SessionGroup(this);
        _copyToGroup->addSession(_session);
        _copyToGroup->setMasterStatus(_session, true);
        _copyToGroup->setMasterMode(SessionGroup::CopyInputToAll);
    }

    QPointer<CopyInputDialog> dialog = new CopyInputDialog(_view);
    dialog->setMasterSession(_session);

    const QList<Session*> sessionsList = _copyToGroup->sessions();
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QSet<Session*> currentGroup(sessionsList.begin(), sessionsList.end());
#else
    QSet<Session*> currentGroup = QSet<Session*>::fromList(sessionsList);
#endif

    currentGroup.remove(_session);

    dialog->setChosenSessions(currentGroup);

    QPointer<Session> guard(_session);
    int result = dialog->exec();
    if (guard.isNull()) {
        return;
    }

    if (result == QDialog::Accepted) {
        QSet<Session*> newGroup = dialog->chosenSessions();
        newGroup.remove(_session);

        const QSet<Session *> completeGroup = newGroup | currentGroup;
        for (Session *session : completeGroup) {
            if (newGroup.contains(session) && !currentGroup.contains(session)) {
                _copyToGroup->addSession(session);
            } else if (!newGroup.contains(session) && currentGroup.contains(session)) {
                _copyToGroup->removeSession(session);
            }
        }

        _copyToGroup->setMasterStatus(_session, true);
        _copyToGroup->setMasterMode(SessionGroup::CopyInputToAll);
        snapshot();
        emit copyInputChanged(this);
    }
}

void SessionController::copyInputToNone()
{
    if (_copyToGroup == nullptr) {      // No 'Copy To' is active
        return;
    }

    // Once Qt5.14+ is the mininum, change to use range constructors
    const QList<Session*> groupList = SessionManager::instance()->sessions();
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QSet<Session*> group(groupList.begin(), groupList.end());
#else
    QSet<Session*> group = QSet<Session*>::fromList(groupList);
#endif

    for (auto iterator : group) {
        Session* session = iterator;

        if (session != _session) {
            _copyToGroup->removeSession(iterator);
        }
    }
    delete _copyToGroup;
    _copyToGroup = nullptr;
    snapshot();
    emit copyInputChanged(this);
}

void SessionController::searchClosed()
{
    _isSearchBarEnabled = false;
    searchHistory(false);
}

void SessionController::updateFilterList(Profile::Ptr profile)
{
    if (profile != SessionManager::instance()->sessionProfile(_session)) {
        return;
    }

    bool underlineFiles = profile->underlineFilesEnabled();

    if (!underlineFiles && (_fileFilter != nullptr)) {
        _view->filterChain()->removeFilter(_fileFilter);
        delete _fileFilter;
        _fileFilter = nullptr;
    } else if (underlineFiles && (_fileFilter == nullptr)) {
        _fileFilter = new FileFilter(_session);
        _view->filterChain()->addFilter(_fileFilter);
    }

    bool underlineLinks = profile->underlineLinksEnabled();
    if (!underlineLinks && (_urlFilter != nullptr)) {
        _view->filterChain()->removeFilter(_urlFilter);
        delete _urlFilter;
        _urlFilter = nullptr;
    } else if (underlineLinks && (_urlFilter == nullptr)) {
        _urlFilter = new UrlFilter();
        _view->filterChain()->addFilter(_urlFilter);
    }
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
    if (_listenForScreenWindowUpdates) {
        return;
    }

    connect(_view->screenWindow(), &Konsole::ScreenWindow::outputChanged, this, &Konsole::SessionController::updateSearchFilter);
    connect(_view->screenWindow(), &Konsole::ScreenWindow::scrolled, this, &Konsole::SessionController::updateSearchFilter);
    connect(_view->screenWindow(),
            &Konsole::ScreenWindow::currentResultLineChanged, _view,
            QOverload<>::of(&Konsole::TerminalDisplay::update));

    _listenForScreenWindowUpdates = true;
}

void SessionController::updateSearchFilter()
{
    if ((_searchFilter != nullptr) && (!_searchBar.isNull())) {
        _view->processFilters();
    }
}

void SessionController::searchBarEvent()
{
    QString selectedText = _view->screenWindow()->selectedText(Screen::PreserveLineBreaks | Screen::TrimLeadingWhitespace | Screen::TrimTrailingWhitespace);
    if (!selectedText.isEmpty()) {
        _searchBar->setSearchText(selectedText);
    }

    if (_searchBar->isVisible()) {
        _searchBar->focusLineEdit();
    } else {
        searchHistory(true);
        _isSearchBarEnabled = true;
    }
}

void SessionController::enableSearchBar(bool showSearchBar)
{
    if (_searchBar.isNull()) {
        return;
    }

    if (showSearchBar && !_searchBar->isVisible()) {
        setSearchStartToWindowCurrentLine();
    }

    _searchBar->setVisible(showSearchBar);
    if (showSearchBar) {
        connect(_searchBar, &Konsole::IncrementalSearchBar::searchChanged, this, &Konsole::SessionController::searchTextChanged);
        connect(_searchBar, &Konsole::IncrementalSearchBar::searchReturnPressed, this, &Konsole::SessionController::findPreviousInHistory);
        connect(_searchBar, &Konsole::IncrementalSearchBar::searchShiftPlusReturnPressed, this, &Konsole::SessionController::findNextInHistory);
    } else {
        disconnect(_searchBar, &Konsole::IncrementalSearchBar::searchChanged, this,
                   &Konsole::SessionController::searchTextChanged);
        disconnect(_searchBar, &Konsole::IncrementalSearchBar::searchReturnPressed, this,
                   &Konsole::SessionController::findPreviousInHistory);
        disconnect(_searchBar, &Konsole::IncrementalSearchBar::searchShiftPlusReturnPressed, this,
                   &Konsole::SessionController::findNextInHistory);
        if ((!_view.isNull()) && (_view->screenWindow() != nullptr)) {
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

QRegularExpression SessionController::regexpFromSearchBarOptions() const
{
    QBitArray options = _searchBar->optionsChecked();

    QString text(_searchBar->searchText());

    QRegularExpression regExp;
    if (options.at(IncrementalSearchBar::RegExp)) {
        regExp.setPattern(text);
    } else {
        regExp.setPattern(QRegularExpression::escape(text));
    }

    if (!options.at(IncrementalSearchBar::MatchCase)) {
        regExp.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    }

    return regExp;
}

// searchHistory() may be called either as a result of clicking a menu item or
// as a result of changing the search bar widget
void SessionController::searchHistory(bool showSearchBar)
{
    enableSearchBar(showSearchBar);

    if (!_searchBar.isNull()) {
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

    if (_searchText == text) {
        return;
    }

    _searchText = text;

    if (text.isEmpty()) {
        _view->screenWindow()->clearSelection();
        _view->screenWindow()->scrollTo(_searchStartLine);
    }

    // update search.  this is called even when the text is
    // empty to clear the view's filters
    beginSearch(text , reverseSearchChecked() ? Enum::BackwardsSearch : Enum::ForwardsSearch);
}
void SessionController::searchCompleted(bool success)
{
    _prevSearchResultLine = _view->screenWindow()->currentResultLine();

    if (!_searchBar.isNull()) {
        _searchBar->setFoundMatch(success);
    }
}

void SessionController::beginSearch(const QString& text, Enum::SearchDirection direction)
{
    Q_ASSERT(_searchBar);
    Q_ASSERT(_searchFilter);

    QRegularExpression regExp = regexpFromSearchBarOptions();
    _searchFilter->setRegExp(regExp);

    if (_searchStartLine < 0 || _searchStartLine > _view->screenWindow()->lineCount()) {
        if (direction == Enum::ForwardsSearch) {
            setSearchStartTo(_view->screenWindow()->currentLine());
        } else {
            setSearchStartTo(_view->screenWindow()->currentLine() + _view->screenWindow()->windowLines());
        }
    }

    if (!regExp.pattern().isEmpty()) {
        _view->screenWindow()->setCurrentResultLine(-1);
        auto task = new SearchHistoryTask(this);

        connect(task, &Konsole::SearchHistoryTask::completed, this, &Konsole::SessionController::searchCompleted);

        task->setRegExp(regExp);
        task->setSearchDirection(direction);
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


    beginSearch(_searchBar->searchText(), reverseSearchChecked() ? Enum::BackwardsSearch : Enum::ForwardsSearch);
}
void SessionController::findNextInHistory()
{
    Q_ASSERT(_searchBar);
    Q_ASSERT(_searchFilter);

    setSearchStartTo(_prevSearchResultLine);

    beginSearch(_searchBar->searchText(), reverseSearchChecked() ? Enum::BackwardsSearch : Enum::ForwardsSearch);
}
void SessionController::findPreviousInHistory()
{
    Q_ASSERT(_searchBar);
    Q_ASSERT(_searchFilter);

    setSearchStartTo(_prevSearchResultLine);

    beginSearch(_searchBar->searchText(), reverseSearchChecked() ? Enum::ForwardsSearch : Enum::BackwardsSearch);
}
void SessionController::changeSearchMatch()
{
    Q_ASSERT(_searchBar);
    Q_ASSERT(_searchFilter);

    // reset Selection for new case match
    _view->screenWindow()->clearSelection();
    beginSearch(_searchBar->searchText(), reverseSearchChecked() ? Enum::BackwardsSearch : Enum::ForwardsSearch);
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
    if (guard.isNull()) {
        return;
    }

    if (result != 0) {
        scrollBackOptionsChanged(dialog->mode(), dialog->lineCount());
    }
}
void SessionController::sessionResizeRequest(const QSize& size)
{
    ////qDebug() << "View resize requested to " << size;
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
    auto options = new PrintOptions();

    dialog->setOptionTabs({options});
    dialog->setWindowTitle(i18n("Print Shell"));
    connect(dialog,
            QOverload<>::of(&QPrintDialog::accepted),
            options,
            &Konsole::PrintOptions::saveSettings);
    if (dialog->exec() != QDialog::Accepted) {
        return;
    }

    QPainter painter;
    painter.begin(&printer);

    KConfigGroup configGroup(KSharedConfig::openConfig(), "PrintOptions");

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
    _view->repaint();
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

void SessionController::resetFontSize()
{
    _view->resetFontSize();
}

void SessionController::monitorActivity(bool monitor)
{
    _session->setMonitorActivity(monitor);
}
void SessionController::monitorSilence(bool monitor)
{
    _session->setMonitorSilence(monitor);
}
void SessionController::monitorProcessFinish(bool monitor)
{
    _monitorProcessFinish = monitor;
}
void SessionController::updateSessionIcon()
{
    // If the default profile icon is being used, don't put it on the tab
    // Only show the icon if the user specifically chose one
    if (_session->iconName() == QStringLiteral("utilities-terminal")) {
        _sessionIconName = QString();
    } else {
        _sessionIconName = _session->iconName();
    }
    _sessionIcon = QIcon::fromTheme(_sessionIconName);

    setIcon(_sessionIcon);
}

void SessionController::updateReadOnlyActionStates()
{
    bool readonly = isReadOnly();
    QAction *readonlyAction = actionCollection()->action(QStringLiteral("view-readonly"));
    Q_ASSERT(readonlyAction != nullptr);
    readonlyAction->setIcon(QIcon::fromTheme(readonly ? QStringLiteral("object-locked") : QStringLiteral("object-unlocked")));
    readonlyAction->setChecked(readonly);

    auto updateActionState = [this, readonly](const QString &name) {
        QAction *action = actionCollection()->action(name);
        if (action != nullptr) {
            action->setVisible(!readonly);
        }
    };

    updateActionState(QStringLiteral("edit_paste"));
    updateActionState(QStringLiteral("clear-history"));
    updateActionState(QStringLiteral("clear-history-and-reset"));
    updateActionState(QStringLiteral("edit-current-profile"));
    updateActionState(QStringLiteral("switch-profile"));
    updateActionState(QStringLiteral("adjust-history"));
    updateActionState(QStringLiteral("send-signal"));
    updateActionState(QStringLiteral("zmodem-upload"));

    _codecAction->setEnabled(!readonly);

    // Without the timer, when detaching a tab while the message widget is visible,
    // the size of the terminal becomes really small...
    QTimer::singleShot(0, this, [this, readonly]() {
        _view->updateReadOnlyState(readonly);
    });
}

bool SessionController::isReadOnly() const
{
    if (!_session.isNull()) {
        return _session->isReadOnly();
    } else {
        return false;
    }
}

bool SessionController::isCopyInputActive() const
{
    return ((_copyToGroup != nullptr) && _copyToGroup->sessions().count() > 1);
}

void SessionController::sessionAttributeChanged()
{
    if (_sessionIconName != _session->iconName()) {
        updateSessionIcon();
    }

    QString title = _session->title(Session::DisplayedTitleRole);

    // special handling for the "%w" marker which is replaced with the
    // window title set by the shell
    title.replace(QLatin1String("%w"), _session->userTitle());
    // special handling for the "%#" marker which is replaced with the
    // number of the shell
    title.replace(QLatin1String("%#"), QString::number(_session->sessionId()));

    if (title.isEmpty()) {
        title = _session->title(Session::NameRole);
    }

    setTitle(title);
    setColor(_session->color());
    emit rawTitleChanged();
}

void SessionController::sessionReadOnlyChanged() {
    updateReadOnlyActionStates();

    // Update all views
    const QList<TerminalDisplay *> viewsList = session()->views();
    for (TerminalDisplay *terminalDisplay : viewsList) {
        if (terminalDisplay != _view) {
            terminalDisplay->updateReadOnlyState(isReadOnly());
        }
        emit readOnlyChanged(this);
    }
}

void SessionController::showDisplayContextMenu(const QPoint& position)
{
    // needed to make sure the popup menu is available, even if a hosting
    // application did not merge our GUI.
    if (factory() == nullptr) {
        if (clientBuilder() == nullptr) {
            setClientBuilder(new KXMLGUIBuilder(_view));

            // Client builder does not get deleted automatically
            connect(this, &QObject::destroyed, this, [this]{ delete clientBuilder(); });
        }

        auto factory = new KXMLGUIFactory(clientBuilder(), _view);
        factory->addClient(this);
    }

    QPointer<QMenu> popup = qobject_cast<QMenu*>(factory()->container(QStringLiteral("session-popup-menu"), this));
    if (!popup.isNull()) {
        updateReadOnlyActionStates();

        auto contentSeparator = new QAction(popup);
        contentSeparator->setSeparator(true);

        // We don't actually use this shortcut, but we need to display it for consistency :/
        QAction *copy = actionCollection()->action(QStringLiteral("edit_copy_contextmenu"));
#ifdef Q_OS_MACOS
        copy->setShortcut(Qt::META + Qt::Key_C);
#else
        copy->setShortcut(Konsole::ACCEL + Qt::SHIFT + Qt::Key_C);
#endif
        // prepend content-specific actions such as "Open Link", "Copy Email Address" etc.
        QSharedPointer<Filter::HotSpot> hotSpot = _view->filterActions(position);
        if (hotSpot != nullptr) {
            popup->insertActions(popup->actions().value(0, nullptr), hotSpot->actions() << contentSeparator );
            popup->addAction(contentSeparator);
            hotSpot->setupMenu(popup.data());
        }

        // always update this submenu before showing the context menu,
        // because the available search services might have changed
        // since the context menu is shown last time
        updateWebSearchMenu();

        _preventClose = true;

        if (_showMenuAction != nullptr) {
            if (  _showMenuAction->isChecked() ) {
                popup->removeAction( _showMenuAction);
            } else {
                popup->insertAction(_switchProfileMenu, _showMenuAction);
            }
        }

        // they are here.
        // qDebug() << popup->actions().indexOf(contentActions[0]) << popup->actions().indexOf(contentActions[1]) << popup->actions()[3];
        QAction* chosen = popup->exec(QCursor::pos());

        // check for validity of the pointer to the popup menu
        if (!popup.isNull()) {
            delete contentSeparator;
        }

        _preventClose = false;

        if ((chosen != nullptr) && chosen->objectName() == QLatin1String("close-session")) {
            chosen->trigger();
        }

        // Remove the Accelerator for the copy shortcut so we don't have two actions with same shortcut.
        copy->setShortcut({});
    } else {
        qCDebug(KonsoleDebug) << "Unable to display popup menu for session"
                   << _session->title(Session::NameRole)
                   << ", no GUI factory available to build the popup.";
    }
}

void SessionController::movementKeyFromSearchBarReceived(QKeyEvent *event)
{
    QCoreApplication::sendEvent(_view, event);
    setSearchStartToWindowCurrentLine();
}

void SessionController::sessionNotificationsChanged(Session::Notification notification, bool enabled)
{
    emit notificationChanged(this, notification, enabled);
}

void SessionController::zmodemDownload()
{
    QString zmodem = QStandardPaths::findExecutable(QStringLiteral("rz"));
    if (zmodem.isEmpty()) {
        zmodem = QStandardPaths::findExecutable(QStringLiteral("lrz"));
    }
    if (!zmodem.isEmpty()) {
        const QString path = QFileDialog::getExistingDirectory(_view,
                i18n("Save ZModem Download to..."),
                QDir::homePath(),
                QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

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
}

void SessionController::zmodemUpload()
{
    if (_session->isZModemBusy()) {
        KMessageBox::sorry(_view,
                           i18n("<p>The current session already has a ZModem file transfer in progress.</p>"));
        return;
    }

    QString zmodem = QStandardPaths::findExecutable(QStringLiteral("sz"));
    if (zmodem.isEmpty()) {
        zmodem = QStandardPaths::findExecutable(QStringLiteral("lsz"));
    }
    if (zmodem.isEmpty()) {
        KMessageBox::sorry(_view,
                           i18n("<p>No suitable ZModem software was found on this system.</p>"
                                "<p>You may wish to install the 'rzsz' or 'lrzsz' package.</p>"));
        return;
    }

    QStringList files = QFileDialog::getOpenFileNames(_view,
                        i18n("Select Files for ZModem Upload"),
                        QDir::homePath());
    if (!files.isEmpty()) {
        _session->startZModem(zmodem, QString(), files);
    }
}

bool SessionController::isKonsolePart() const
{
    // Check to see if we are being called from Konsole or a KPart
    return !(qApp->applicationName() == QLatin1String("konsole"));
}

QString SessionController::userTitle() const
{
    if (!_session.isNull()) {
        return _session->userTitle();
    } else {
        return QString();
    }
}

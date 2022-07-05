/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2009 Thomas Dreibholz <dreibh@iem.uni-due.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "SessionController.h"

#include "konsoledebug.h"
#include "profile/ProfileManager.h"
#include "terminalDisplay/TerminalColor.h"
#include "terminalDisplay/TerminalFonts.h"

// Qt
#include <QAction>
#include <QApplication>

#include <QFileDialog>
#include <QIcon>
#include <QKeyEvent>
#include <QList>
#include <QMenu>
#include <QPainter>
#include <QStandardPaths>
#include <QUrl>

// KDE
#include <KActionCollection>
#include <KActionMenu>
#include <KCodecAction>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <KNotification>
#include <KSelectAction>
#include <KSharedConfig>
#include <KShell>
#include <KStringHandler>
#include <KToggleAction>
#include <KUriFilter>
#include <KXMLGUIBuilder>
#include <KXMLGUIFactory>
#include <KXmlGuiWindow>

#include <KIO/CommandLauncherJob>

#include <KIO/JobUiDelegate>
#include <KIO/OpenFileManagerWindowJob>
#include <KIO/OpenUrlJob>

#include <KFileItemListProperties>

#include <kconfigwidgets_version.h>
#include <kio_version.h>
#include <kwidgetsaddons_version.h>

// Konsole
#include "CopyInputDialog.h"
#include "Emulation.h"
#include "HistorySizeDialog.h"
#include "RenameTabDialog.h"
#include "SaveHistoryTask.h"
#include "ScreenWindow.h"
#include "SearchHistoryTask.h"

#include "filterHotSpots/ColorFilter.h"
#include "filterHotSpots/EscapeSequenceUrlFilter.h"
#include "filterHotSpots/FileFilter.h"
#include "filterHotSpots/FileFilterHotspot.h"
#include "filterHotSpots/Filter.h"
#include "filterHotSpots/FilterChain.h"
#include "filterHotSpots/HotSpot.h"
#include "filterHotSpots/RegExpFilter.h"
#include "filterHotSpots/UrlFilter.h"

#include "history/HistoryType.h"
#include "history/HistoryTypeFile.h"
#include "history/HistoryTypeNone.h"
#include "history/compact/CompactHistoryType.h"

#include "profile/ProfileList.h"

#include "SessionGroup.h"
#include "SessionManager.h"

#include "widgets/EditProfileDialog.h"
#include "widgets/IncrementalSearchBar.h"

#include "terminalDisplay/TerminalColor.h"
#include "terminalDisplay/TerminalDisplay.h"

// For Unix signal names
#include <csignal>

using namespace Konsole;

QSet<SessionController *> SessionController::_allControllers;
int SessionController::_lastControllerId;

SessionController::SessionController(Session *sessionParam, TerminalDisplay *viewParam, QObject *parent)
    : ViewProperties(parent)
    , KXMLGUIClient()
    , _copyToGroup(nullptr)
    , _profileList(nullptr)
    , _sessionIcon(QIcon())
    , _sessionIconName(QString())
    , _searchFilter(nullptr)
    , _urlFilter(nullptr)
    , _fileFilter(nullptr)
    , _colorFilter(nullptr)
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
    , _selectionEmpty(false)
    , _selectionChanged(true)
    , _selectedText(QString())
    , _showMenuAction(nullptr)
    , _bookmarkValidProgramsToClear(QStringList())
    , _isSearchBarEnabled(false)
    , _searchBar(viewParam->searchBar())
    , _monitorProcessFinish(false)
    , _escapedUrlFilter(nullptr)
{
    Q_ASSERT(sessionParam);
    Q_ASSERT(viewParam);

    _sessionDisplayConnection = new SessionDisplayConnection(sessionParam, viewParam, this);
    viewParam->setSessionController(this);

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

    connect(this, &SessionController::requestPrint, view(), &TerminalDisplay::printScreen);

    actionCollection()->addAssociatedWidget(viewParam);

    const QList<QAction *> actionsList = actionCollection()->actions();
    for (QAction *action : actionsList) {
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }

    setIdentifier(++_lastControllerId);
    sessionAttributeChanged();

    connect(view(), &TerminalDisplay::compositeFocusChanged, this, &SessionController::viewFocusChangeHandler);

    Profile::Ptr currentProfile = SessionManager::instance()->sessionProfile(session());

    // install filter on the view to highlight URLs and files
    updateFilterList(currentProfile);

    // listen for changes in session, we might need to change the enabled filters
    connect(ProfileManager::instance(), &Konsole::ProfileManager::profileChanged, this, &Konsole::SessionController::updateFilterList);

    // listen for session resize requests
    connect(session(), &Konsole::Session::resizeRequest, this, &Konsole::SessionController::sessionResizeRequest);

    // listen for popup menu requests
    connect(view(), &Konsole::TerminalDisplay::configureRequest, this, &Konsole::SessionController::showDisplayContextMenu);

    // move view to newest output when keystrokes occur
    connect(view(), &Konsole::TerminalDisplay::keyPressedSignal, this, &Konsole::SessionController::trackOutput);

    // listen to activity / silence notifications from session
    connect(session(), &Konsole::Session::notificationsChanged, this, &Konsole::SessionController::sessionNotificationsChanged);
    // listen to title and icon changes
    connect(session(), &Konsole::Session::sessionAttributeChanged, this, &Konsole::SessionController::sessionAttributeChanged);
    connect(session(), &Konsole::Session::readOnlyChanged, this, &Konsole::SessionController::sessionReadOnlyChanged);

    connect(this, &Konsole::SessionController::tabRenamedByUser, session(), &Konsole::Session::tabTitleSetByUser);
    connect(this, &Konsole::SessionController::tabColoredByUser, session(), &Konsole::Session::tabColorSetByUser);

    connect(session(), &Konsole::Session::currentDirectoryChanged, this, &Konsole::SessionController::currentDirectoryChanged);

    // listen for color changes
    connect(session(), &Konsole::Session::changeBackgroundColorRequest, view()->terminalColor(), &TerminalColor::setBackgroundColor);
    connect(session(), &Konsole::Session::changeForegroundColorRequest, view()->terminalColor(), &TerminalColor::setForegroundColor);

    // update the title when the session starts
    connect(session(), &Konsole::Session::started, this, &Konsole::SessionController::snapshot);

    // listen for output changes to set activity flag
    connect(session()->emulation(), &Konsole::Emulation::outputChanged, this, &Konsole::SessionController::fireActivity);

    // listen for detection of ZModem transfer
    connect(session(), &Konsole::Session::zmodemDownloadDetected, this, &Konsole::SessionController::zmodemDownload);
    connect(session(), &Konsole::Session::zmodemUploadDetected, this, &Konsole::SessionController::zmodemUpload);

    // listen for flow control status changes
    connect(session(), &Konsole::Session::flowControlEnabledChanged, view(), &Konsole::TerminalDisplay::setFlowControlWarningEnabled);
    view()->setFlowControlWarningEnabled(session()->flowControlEnabled());

    // take a snapshot of the session state every so often when
    // user activity occurs
    //
    // the timer is owned by the session so that it will be destroyed along
    // with the session
    _interactionTimer = new QTimer(session());
    _interactionTimer->setSingleShot(true);
    _interactionTimer->setInterval(2000);
    connect(_interactionTimer, &QTimer::timeout, this, &Konsole::SessionController::snapshot);
    connect(view(), &Konsole::TerminalDisplay::compositeFocusChanged, this, [this](bool focused) {
        if (focused) {
            interactionHandler();
        }
    });
    connect(view(), &Konsole::TerminalDisplay::keyPressedSignal, this, &Konsole::SessionController::interactionHandler);
    connect(session()->emulation(), &Konsole::Emulation::outputChanged, this, &Konsole::SessionController::interactionHandler);

    // xterm '10;?' request
    connect(session(), &Konsole::Session::getForegroundColor, this, &Konsole::SessionController::sendForegroundColor);
    // xterm '11;?' request
    connect(session(), &Konsole::Session::getBackgroundColor, this, &Konsole::SessionController::sendBackgroundColor);

    _allControllers.insert(this);

    // A list of programs that accept Ctrl+C to clear command line used
    // before outputting bookmark.
    _bookmarkValidProgramsToClear =
        QStringList({QStringLiteral("bash"), QStringLiteral("fish"), QStringLiteral("sh"), QStringLiteral("tcsh"), QStringLiteral("zsh")});

    setupSearchBar();
    _searchBar->setVisible(_isSearchBarEnabled);

    // Setup default state for mouse tracking
    const bool allowMouseTracking = currentProfile->property<bool>(Profile::AllowMouseTracking);
    view()->setAllowMouseTracking(allowMouseTracking);
    actionCollection()->action(QStringLiteral("allow-mouse-tracking"))->setChecked(allowMouseTracking);
}

SessionController::~SessionController()
{
    _allControllers.remove(this);

    if (factory() != nullptr) {
        factory()->removeClient(this);
    }
}
void SessionController::trackOutput(QKeyEvent *event)
{
    Q_ASSERT(view()->screenWindow());

    // Qt has broken something, so we can't rely on just checking if certain
    // keys are passed as modifiers anymore.
    const int key = event->key();

    /* clang-format off */
    const bool shouldNotTriggerScroll =
        key == Qt::Key_Super_L
        || key == Qt::Key_Super_R
        || key == Qt::Key_Hyper_L
        || key == Qt::Key_Hyper_R
        || key == Qt::Key_Shift
        || key == Qt::Key_Control
        || key == Qt::Key_Meta
        || key == Qt::Key_Alt
        || key == Qt::Key_AltGr
        || key == Qt::Key_CapsLock
        || key == Qt::Key_NumLock
        || key == Qt::Key_ScrollLock;
    /* clang-format on */

    // Only jump to the bottom if the user actually typed something in,
    // not if the user e. g. just pressed a modifier.
    if (event->text().isEmpty() && ((event->modifiers() != 0u) || shouldNotTriggerScroll)) {
        return;
    }

    view()->screenWindow()->setTrackOutput(true);
}

void SessionController::viewFocusChangeHandler(bool focused)
{
    if (focused) {
        // notify the world that the view associated with this session has been focused
        // used by the view manager to update the title of the MainWindow widget containing the view
        Q_EMIT viewFocused(this);

        // when the view is focused, set bell events from the associated session to be delivered
        // by the focused view

        // first, disconnect any other views which are listening for bell signals from the session
        disconnect(session(), &Konsole::Session::bellRequest, nullptr, nullptr);
        // second, connect the newly focused view to listen for the session's bell signal
        connect(session(), &Konsole::Session::bellRequest, view(), &Konsole::TerminalDisplay::bell);

        if ((_copyInputToAllTabsAction != nullptr) && _copyInputToAllTabsAction->isChecked()) {
            // A session with "Copy To All Tabs" has come into focus:
            // Ensure that newly created sessions are included in _copyToGroup!
            copyInputToAllTabs();
        }
    }
}

void SessionController::interactionHandler()
{
    if (!_interactionTimer->isActive()) {
        _interactionTimer->start();
    }
}

void SessionController::snapshot()
{
    Q_ASSERT(!session().isNull());

    QString title = session()->getDynamicTitle();
    title = title.simplified();

    // Visualize that the session is broadcasting to others
    if ((_copyToGroup != nullptr) && _copyToGroup->sessions().count() > 1) {
        title.append(QLatin1Char('*'));
    }

    // use the fallback title if needed
    if (title.isEmpty()) {
        title = session()->title(Session::NameRole);
    }

    QColor color = session()->color();
    // use the fallback color if needed
    if (!color.isValid()) {
        color = QColor(QColor::Invalid);
    }

    // apply new title
    session()->setTitle(Session::DisplayedTitleRole, title);

    // apply new color
    session()->setColor(color);

    // check if foreground process ended and notify if this option was requested
    if (_monitorProcessFinish) {
        bool isForegroundProcessActive = session()->isForegroundProcessActive();
        if (!_previousForegroundProcessName.isNull() && !isForegroundProcessActive) {
            KNotification *notification =
                KNotification::event(session()->hasFocus() ? QStringLiteral("ProcessFinished") : QStringLiteral("ProcessFinishedHidden"),
                                     i18n("The process '%1' has finished running in session '%2'", _previousForegroundProcessName, session()->nameTitle()),
                                     QPixmap(),
                                     view(),
                                     KNotification::CloseWhenWidgetActivated);
            notification->setDefaultAction(i18n("Show session"));
            connect(notification, &KNotification::defaultActivated, this, [this, notification]() {
                view()->notificationClicked(notification->xdgActivationToken());
            });
        }
        _previousForegroundProcessName = isForegroundProcessActive ? session()->foregroundProcessName() : QString();
    }

    // do not forget icon
    updateSessionIcon();
}

QString SessionController::currentDir() const
{
    return session()->currentWorkingDirectory();
}

QUrl SessionController::url() const
{
    return session()->getUrl();
}

void SessionController::rename()
{
    renameSession();
}

void SessionController::openUrl(const QUrl &url)
{
    // Clear shell's command line
    if (!session()->isForegroundProcessActive() && _bookmarkValidProgramsToClear.contains(session()->foregroundProcessName())) {
        session()->sendTextToTerminal(QChar(0x03), QLatin1Char('\n')); // Ctrl+C
    }

    // handle local paths
    if (url.isLocalFile()) {
        QString path = url.toLocalFile();
        session()->sendTextToTerminal(QStringLiteral("cd ") + KShell::quoteArg(path), QLatin1Char('\r'));
    } else if (url.scheme().isEmpty()) {
        // QUrl couldn't parse what the user entered into the URL field
        // so just dump it to the shell
        // If you change this, change it also in autotests/BookMarkTest.cpp
        QString command = QUrl::fromPercentEncoding(url.toEncoded());
        if (!command.isEmpty()) {
            session()->sendTextToTerminal(command, QLatin1Char('\r'));
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
        session()->sendTextToTerminal(sshCommand, QLatin1Char('\r'));

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

        session()->sendTextToTerminal(telnetCommand, QLatin1Char('\r'));

    } else {
        // TODO Implement handling for other Url types

        KMessageBox::sorry(view()->window(), i18n("Konsole does not know how to open the bookmark: ") + url.toDisplayString());

        qCDebug(KonsoleDebug) << "Unable to open bookmark at url" << url << ", I do not know"
                              << " how to handle the protocol " << url.scheme();
    }
}

void SessionController::setupPrimaryScreenSpecificActions(bool use)
{
    KActionCollection *collection = actionCollection();
    QAction *clearAction = collection->action(QStringLiteral("clear-history"));
    QAction *resetAction = collection->action(QStringLiteral("clear-history-and-reset"));
    QAction *selectAllAction = collection->action(QStringLiteral("select-all"));
    QAction *selectLineAction = collection->action(QStringLiteral("select-line"));

    // these actions are meaningful only when primary screen is used.
    clearAction->setEnabled(use);
    resetAction->setEnabled(use);
    selectAllAction->setEnabled(use);
    selectLineAction->setEnabled(use);
}

void SessionController::selectionChanged(const bool selectionEmpty)
{
    _selectionChanged = true;
    _selectionEmpty = selectionEmpty;
    updateCopyAction(selectionEmpty);
}

void SessionController::updateCopyAction(const bool selectionEmpty)
{
    QAction *copyAction = actionCollection()->action(QStringLiteral("edit_copy"));
    QAction *copyContextMenu = actionCollection()->action(QStringLiteral("edit_copy_contextmenu"));
    // copy action is meaningful only when some text is selected.
    // Or when semantic integration is used.
    bool hasRepl = view() && view()->screenWindow() && view()->screenWindow()->screen() && view()->screenWindow()->screen()->hasRepl();
    copyAction->setEnabled(!selectionEmpty || hasRepl);
    copyContextMenu->setVisible(!selectionEmpty || hasRepl);
    QAction *Action = actionCollection()->action(QStringLiteral("edit_copy_contextmenu_in"));
    Action->setVisible(!selectionEmpty && hasRepl);
    Action = actionCollection()->action(QStringLiteral("edit_copy_contextmenu_out"));
    Action->setVisible(!selectionEmpty && hasRepl);
    Action = actionCollection()->action(QStringLiteral("edit_copy_contextmenu_in_out"));
    Action->setVisible(!selectionEmpty && hasRepl);
}

void SessionController::updateWebSearchMenu()
{
    // reset
    _webSearchMenu->setVisible(false);
    _webSearchMenu->menu()->clear();

    if (_selectionEmpty) {
        return;
    }

    if (_selectionChanged) {
        _selectedText = view()->screenWindow()->selectedText(Screen::PreserveLineBreaks);
        _selectionChanged = false;
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
            _webSearchMenu->setText(i18n("Search for '%1' with", KStringHandler::rsqueeze(searchText, 16)));

            QAction *action = nullptr;

            for (const QString &searchProvider : searchProviders) {
                action = new QAction(searchProvider, _webSearchMenu);
                action->setIcon(QIcon::fromTheme(filterData.iconNameForPreferredSearchProvider(searchProvider)));
                action->setData(filterData.queryForPreferredSearchProvider(searchProvider));
                connect(action, &QAction::triggered, this, [this, action]() {
                    handleWebShortcutAction(action);
                });
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

void SessionController::handleWebShortcutAction(QAction *action)
{
    if (action == nullptr) {
        return;
    }

    KUriFilterData filterData(action->data().toString());

    if (KUriFilter::self()->filterUri(filterData, {QStringLiteral("kurisearchfilter")})) {
        const QUrl url = filterData.uri();

        auto *job = new KIO::OpenUrlJob(url);
        job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, QApplication::activeWindow()));
        job->start();
    }
}

void SessionController::configureWebShortcuts()
{
    auto job = new KIO::CommandLauncherJob(QStringLiteral("kcmshell5"), {QStringLiteral("webshortcuts")});
    job->start();
}

void SessionController::sendSignal(QAction *action)
{
    const auto signal = action->data().toInt();
    session()->sendSignal(signal);
}

void SessionController::sendForegroundColor(uint terminator)
{
    const QColor c = view()->terminalColor()->foregroundColor();
    session()->reportForegroundColor(c, terminator);
}

void Konsole::SessionController::sendBackgroundColor(uint terminator)
{
    const QColor c = view()->terminalColor()->backgroundColor();
    session()->reportBackgroundColor(c, terminator);
}

void SessionController::toggleReadOnly(QAction *action)
{
    if (action != nullptr) {
        bool readonly = !isReadOnly();
        session()->setReadOnly(readonly);
    }
}

void SessionController::toggleAllowMouseTracking(QAction *action)
{
    if (action == nullptr) {
        // Crash if running in debug build (aka. someone developing)
        Q_ASSERT(false && "Invalid function called toggleAllowMouseTracking");
        return;
    }

    _sessionDisplayConnection->view()->setAllowMouseTracking(action->isChecked());
}

void SessionController::removeSearchFilter()
{
    if (_searchFilter == nullptr) {
        return;
    }

    view()->filterChain()->removeFilter(_searchFilter);
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
    connect(_searchBar,
            &Konsole::IncrementalSearchBar::reverseSearchToggled,
            this,
            &Konsole::SessionController::updateMenuIconsAccordingToReverseSearchSetting);
    connect(_searchBar, &Konsole::IncrementalSearchBar::highlightMatchesToggled, this, &Konsole::SessionController::highlightMatches);
    connect(_searchBar, &Konsole::IncrementalSearchBar::matchCaseToggled, this, &Konsole::SessionController::changeSearchMatch);
    connect(_searchBar, &Konsole::IncrementalSearchBar::matchRegExpToggled, this, &Konsole::SessionController::changeSearchMatch);

    updateMenuIconsAccordingToReverseSearchSetting();
}

void SessionController::setShowMenuAction(QAction *action)
{
    _showMenuAction = action;
}

void SessionController::setupCommonActions()
{
    KActionCollection *collection = actionCollection();

    // Close Session
    QAction *action = collection->addAction(QStringLiteral("close-session"), this, &SessionController::closeSession);
    action->setText(i18n("&Close Session"));

    action->setIcon(QIcon::fromTheme(QStringLiteral("tab-close")));
    collection->setDefaultShortcut(action, Konsole::ACCEL | Qt::Key_W);

    // Open Browser
    action = collection->addAction(QStringLiteral("open-browser"), this, &SessionController::openBrowser);
    action->setText(i18n("Open File Manager"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("system-file-manager")));

    // Copy and Paste
    action = KStandardAction::copy(this, &SessionController::copy, collection);
    collection->setDefaultShortcut(action, Konsole::ACCEL | Qt::Key_C);
    // disabled at first, since nothing has been selected now
    action->setEnabled(false);

    // We need a different QAction on the context menu because one will be disabled when there's no selection,
    // other will be hidden.
    action = collection->addAction(QStringLiteral("edit_copy_contextmenu"));
    action->setText(i18n("Copy"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    action->setVisible(false);
    connect(action, &QAction::triggered, this, &SessionController::copy);

    action = collection->addAction(QStringLiteral("edit_copy_contextmenu_in_out"));
    action->setText(i18n("Copy except prompts"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    action->setVisible(false);
    connect(action, &QAction::triggered, this, &SessionController::copyInputOutput);

    action = collection->addAction(QStringLiteral("edit_copy_contextmenu_in"));
    action->setText(i18n("Copy user input"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    action->setVisible(false);
    connect(action, &QAction::triggered, this, &SessionController::copyInput);

    action = collection->addAction(QStringLiteral("edit_copy_contextmenu_out"));
    action->setText(i18n("Copy command output"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    action->setVisible(false);
    connect(action, &QAction::triggered, this, &SessionController::copyOutput);

    action = KStandardAction::paste(this, &SessionController::paste, collection);
    QList<QKeySequence> pasteShortcut;
    pasteShortcut.append(QKeySequence(Konsole::ACCEL | Qt::Key_V));
#ifndef Q_OS_MACOS
    // No Insert key on Mac keyboards
    pasteShortcut.append(QKeySequence(Qt::SHIFT | Qt::Key_Insert));
#endif
    collection->setDefaultShortcuts(action, pasteShortcut);

    action = collection->addAction(QStringLiteral("paste-selection"), this, &SessionController::pasteFromX11Selection);
    action->setText(i18n("Paste Selection"));
#ifdef Q_OS_MACOS
    collection->setDefaultShortcut(action, Qt::CTRL | Qt::SHIFT | Qt::Key_V);
#else
    collection->setDefaultShortcut(action, Qt::CTRL | Qt::SHIFT | Qt::Key_Insert);
#endif

    _webSearchMenu = new KActionMenu(i18n("Web Search"), this);
    _webSearchMenu->setIcon(QIcon::fromTheme(QStringLiteral("preferences-web-browser-shortcuts")));
    _webSearchMenu->setVisible(false);
    collection->addAction(QStringLiteral("web-search"), _webSearchMenu);

    action = collection->addAction(QStringLiteral("select-all"), this, &SessionController::selectAll);
    action->setText(i18n("&Select All"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-select-all")));

    action = collection->addAction(QStringLiteral("select-line"), this, &SessionController::selectLine);
    action->setText(i18n("Select &Line"));

    action = KStandardAction::saveAs(this, &SessionController::saveHistory, collection);
    action->setText(i18n("Save Output &As..."));
#ifdef Q_OS_MACOS
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
#endif

    action = KStandardAction::print(this, &SessionController::requestPrint, collection);
    action->setText(i18n("&Print Screen..."));
    collection->setDefaultShortcut(action, Konsole::ACCEL | Qt::Key_P);

    action = collection->addAction(QStringLiteral("adjust-history"), this, &SessionController::showHistoryOptions);
    action->setText(i18n("Adjust Scrollback..."));
    action->setIcon(QIcon::fromTheme(QStringLiteral("configure")));

    action = collection->addAction(QStringLiteral("clear-history"), this, &SessionController::clearHistory);
    action->setText(i18n("Clear Scrollback"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear-history")));

    action = collection->addAction(QStringLiteral("clear-history-and-reset"), this, &SessionController::clearHistoryAndReset);
    action->setText(i18n("Clear Scrollback and Reset"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear-history")));
    collection->setDefaultShortcut(action, Konsole::ACCEL | Qt::Key_K);

    // Profile Options
    action = collection->addAction(QStringLiteral("edit-current-profile"), this, &SessionController::editCurrentProfile);
    action->setIcon(QIcon::fromTheme(QStringLiteral("document-properties")));
    setEditProfileActionText(SessionManager::instance()->sessionProfile(session()));

    _switchProfileMenu = new KActionMenu(i18n("Switch Profile"), this);
    collection->addAction(QStringLiteral("switch-profile"), _switchProfileMenu);
    connect(_switchProfileMenu->menu(), &QMenu::aboutToShow, this, &Konsole::SessionController::prepareSwitchProfileMenu);
#if KWIDGETSADDONS_VERSION >= QT_VERSION_CHECK(5, 77, 0)
    _switchProfileMenu->setPopupMode(QToolButton::MenuButtonPopup);
#else
    _switchProfileMenu->setDelayed(false);
#endif

    // History
    _findAction = KStandardAction::find(this, &SessionController::searchBarEvent, collection);

    _findNextAction = KStandardAction::findNext(this, &SessionController::findNextInHistory, collection);
    _findNextAction->setEnabled(false);

    _findPreviousAction = KStandardAction::findPrev(this, &SessionController::findPreviousInHistory, collection);
    _findPreviousAction->setEnabled(false);

#ifdef Q_OS_MACOS
    collection->setDefaultShortcut(_findAction, Qt::CTRL | Qt::Key_F);
    collection->setDefaultShortcut(_findNextAction, Qt::CTRL | Qt::Key_G);
    collection->setDefaultShortcut(_findPreviousAction, Qt::CTRL | Qt::SHIFT | Qt::Key_G);
#else
    collection->setDefaultShortcut(_findAction, Qt::CTRL | Qt::SHIFT | Qt::Key_F);
    collection->setDefaultShortcut(_findNextAction, Qt::Key_F3);
    collection->setDefaultShortcut(_findPreviousAction, QKeySequence{Qt::SHIFT | Qt::Key_F3});
#endif

    // Character Encoding
    _codecAction = new KCodecAction(i18n("Set &Encoding"), this);
    _codecAction->setIcon(QIcon::fromTheme(QStringLiteral("character-set")));
    collection->addAction(QStringLiteral("set-encoding"), _codecAction);
    _codecAction->setCurrentCodec(QString::fromUtf8(session()->codec()));
    connect(session(), &Konsole::Session::sessionCodecChanged, this, &Konsole::SessionController::updateCodecAction);
    connect(_codecAction,
#if KCONFIGWIDGETS_VERSION >= QT_VERSION_CHECK(5, 78, 0)
            QOverload<QTextCodec *>::of(&KCodecAction::codecTriggered),
            this,
#else
            QOverload<QTextCodec *>::of(&KCodecAction::triggered),
            this,
#endif
            &Konsole::SessionController::changeCodec);

    // Mouse tracking enabled
    action = collection->addAction(QStringLiteral("allow-mouse-tracking"), this);
    connect(action, &QAction::toggled, this, [this, action]() {
        toggleAllowMouseTracking(action);
    });
    action->setText(i18nc("@item:inmenu Allows terminal applications to request mouse tracking", "Allow mouse tracking"));
    action->setCheckable(true);

    // Read-only
    action = collection->addAction(QStringLiteral("view-readonly"), this);
    connect(action, &QAction::toggled, this, [this, action]() {
        toggleReadOnly(action);
    });
    action->setText(i18nc("@item:inmenu A read only (locked) session", "Read-only"));
    action->setCheckable(true);
    updateReadOnlyActionStates();
}

void SessionController::setupExtraActions()
{
    KActionCollection *collection = actionCollection();

    // Rename Session
    QAction *action = collection->addAction(QStringLiteral("rename-session"), this, &SessionController::renameSession);
    action->setText(i18n("&Configure or Rename Tab..."));
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-rename")));
    collection->setDefaultShortcut(action, Qt::CTRL | Qt::ALT | Qt::Key_S);

    // Copy input to ==> all tabs
    auto *copyInputToAllTabsAction = collection->add<KToggleAction>(QStringLiteral("copy-input-to-all-tabs"));
    copyInputToAllTabsAction->setText(i18n("&All Tabs in Current Window"));
    copyInputToAllTabsAction->setData(CopyInputToAllTabsMode);
    // this action is also used in other place, so remember it
    _copyInputToAllTabsAction = copyInputToAllTabsAction;

    // Copy input to ==> selected tabs
    auto *copyInputToSelectedTabsAction = collection->add<KToggleAction>(QStringLiteral("copy-input-to-selected-tabs"));
    copyInputToSelectedTabsAction->setText(i18n("&Select Tabs..."));
    collection->setDefaultShortcut(copyInputToSelectedTabsAction, Konsole::ACCEL | Qt::Key_Period);
    copyInputToSelectedTabsAction->setData(CopyInputToSelectedTabsMode);

    // Copy input to ==> none
    auto *copyInputToNoneAction = collection->add<KToggleAction>(QStringLiteral("copy-input-to-none"));
    copyInputToNoneAction->setText(i18nc("@action:inmenu Do not select any tabs", "&None"));
    collection->setDefaultShortcut(copyInputToNoneAction, Konsole::ACCEL | Qt::Key_Slash);
    copyInputToNoneAction->setData(CopyInputToNoneMode);
    copyInputToNoneAction->setChecked(true); // the default state

    // The "Copy Input To" submenu
    // The above three choices are represented as combo boxes
    auto *copyInputActions = collection->add<KSelectAction>(QStringLiteral("copy-input-to"));
    copyInputActions->setText(i18n("Copy Input To"));
    copyInputActions->addAction(copyInputToAllTabsAction);
    copyInputActions->addAction(copyInputToSelectedTabsAction);
    copyInputActions->addAction(copyInputToNoneAction);
    connect(copyInputActions, QOverload<QAction *>::of(&KSelectAction::triggered), this, &Konsole::SessionController::copyInputActionsTriggered);

    action = collection->addAction(QStringLiteral("zmodem-upload"), this, &SessionController::zmodemUpload);
    action->setText(i18n("&ZModem Upload..."));
    action->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
    collection->setDefaultShortcut(action, Qt::CTRL | Qt::ALT | Qt::Key_U);

    // Monitor
    KToggleAction *toggleAction = new KToggleAction(i18n("Monitor for &Activity"), this);
    collection->setDefaultShortcut(toggleAction, Konsole::ACCEL | Qt::Key_A);
    action = collection->addAction(QStringLiteral("monitor-activity"), toggleAction);
    connect(action, &QAction::toggled, this, &Konsole::SessionController::monitorActivity);
    action->setIcon(QIcon::fromTheme(QStringLiteral("tools-media-optical-burn")));

    toggleAction = new KToggleAction(i18n("Monitor for &Silence"), this);
    collection->setDefaultShortcut(toggleAction, Konsole::ACCEL | Qt::Key_I);
    action = collection->addAction(QStringLiteral("monitor-silence"), toggleAction);
    connect(action, &QAction::toggled, this, &Konsole::SessionController::monitorSilence);
    action->setIcon(QIcon::fromTheme(QStringLiteral("tools-media-optical-copy")));

    toggleAction = new KToggleAction(i18n("Monitor for Process Finishing"), this);
    action = collection->addAction(QStringLiteral("monitor-process-finish"), toggleAction);
    connect(action, &QAction::toggled, this, &Konsole::SessionController::monitorProcessFinish);
    action->setIcon(QIcon::fromTheme(QStringLiteral("tools-media-optical-burn-image")));

    // Text Size
    action = collection->addAction(QStringLiteral("enlarge-font"), this, &SessionController::increaseFontSize);
    action->setText(i18n("Enlarge Font"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("format-font-size-more")));
    QList<QKeySequence> enlargeFontShortcut;
    enlargeFontShortcut.append(QKeySequence(Qt::CTRL | Qt::Key_Plus));
    enlargeFontShortcut.append(QKeySequence(Qt::CTRL | Qt::Key_Equal));
    collection->setDefaultShortcuts(action, enlargeFontShortcut);

    action = collection->addAction(QStringLiteral("shrink-font"), this, &SessionController::decreaseFontSize);
    action->setText(i18n("Shrink Font"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("format-font-size-less")));
    collection->setDefaultShortcut(action, QKeySequence(Qt::CTRL | Qt::Key_Minus));

    action = collection->addAction(QStringLiteral("reset-font-size"), this, &SessionController::resetFontSize);
    action->setText(i18n("Reset Font Size"));
    collection->setDefaultShortcut(action, Qt::CTRL | Qt::ALT | Qt::Key_0);

    // Send signal
    auto *sendSignalActions = collection->add<KSelectAction>(QStringLiteral("send-signal"));
    sendSignalActions->setText(i18n("Send Signal"));
    connect(sendSignalActions, QOverload<QAction *>::of(&KSelectAction::triggered), this, &Konsole::SessionController::sendSignal);

    action = collection->addAction(QStringLiteral("sigstop-signal"));
    action->setText(i18n("&Suspend Task") + QStringLiteral(" (STOP)"));
    action->setData(SIGSTOP);
    sendSignalActions->addAction(action);

    action = collection->addAction(QStringLiteral("sigcont-signal"));
    action->setText(i18n("&Continue Task") + QStringLiteral(" (CONT)"));
    action->setData(SIGCONT);
    sendSignalActions->addAction(action);

    action = collection->addAction(QStringLiteral("sighup-signal"));
    action->setText(i18n("&Hangup") + QStringLiteral(" (HUP)"));
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
    action->setText(i18n("&Kill Task") + QStringLiteral(" (KILL)"));
    action->setData(SIGKILL);
    sendSignalActions->addAction(action);

    action = collection->addAction(QStringLiteral("sigusr1-signal"));
    action->setText(i18n("User Signal &1") + QStringLiteral(" (USR1)"));
    action->setData(SIGUSR1);
    sendSignalActions->addAction(action);

    action = collection->addAction(QStringLiteral("sigusr2-signal"));
    action->setText(i18n("User Signal &2") + QStringLiteral(" (USR2)"));
    action->setData(SIGUSR2);
    sendSignalActions->addAction(action);
}

void SessionController::switchProfile(const Profile::Ptr &profile)
{
    SessionManager::instance()->setSessionProfile(session(), profile);
    _switchProfileMenu->setIcon(QIcon::fromTheme(profile->icon()));
    updateFilterList(profile);
    setEditProfileActionText(profile);
}

void SessionController::setEditProfileActionText(const Profile::Ptr &profile)
{
    QAction *action = actionCollection()->action(QStringLiteral("edit-current-profile"));
    if (profile->isBuiltin()) {
        action->setText(i18n("Create New Profile..."));
    } else {
        action->setText(i18n("Edit Current Profile..."));
    }
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

void SessionController::changeCodec(QTextCodec *codec)
{
    session()->setCodec(codec);
}

void SessionController::editCurrentProfile()
{
    auto *dialog = new EditProfileDialog(QApplication::activeWindow());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(true);

    auto profile = SessionManager::instance()->sessionProfile(session());
    auto state = EditProfileDialog::ExistingProfile;
    // Don't edit the built-in profile, instead create a new one
    if (profile->isBuiltin()) {
        auto newProfile = Profile::Ptr(new Profile(profile));
        newProfile->clone(profile, true);
        const QString uniqueName = ProfileManager::instance()->generateUniqueName();
        newProfile->setProperty(Profile::Name, uniqueName);
        newProfile->setProperty(Profile::UntranslatedName, uniqueName);
        profile = newProfile;
        SessionManager::instance()->setSessionProfile(session(), profile);
        state = EditProfileDialog::NewProfile;

        connect(dialog, &QDialog::accepted, this, [this, profile]() {
            setEditProfileActionText(profile);
        });
    }

    dialog->setProfile(profile, state);

    dialog->show();
}

void SessionController::renameSession()
{
    const QString sessionLocalTabTitleFormat = session()->tabTitleFormat(Session::LocalTabTitle);
    const QString sessionRemoteTabTitleFormat = session()->tabTitleFormat(Session::RemoteTabTitle);
    const QColor sessionTabColor = session()->color();

    auto *dialog = new RenameTabDialog(QApplication::activeWindow());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(true);
    dialog->setTabTitleText(sessionLocalTabTitleFormat);
    dialog->setRemoteTabTitleText(sessionRemoteTabTitleFormat);
    dialog->setColor(sessionTabColor);

    if (session()->isRemote()) {
        dialog->focusRemoteTabTitleText();
    } else {
        dialog->focusTabTitleText();
    }

    connect(dialog, &QDialog::accepted, this, [=]() {
        const QString tabTitle = dialog->tabTitleText();
        const QString remoteTabTitle = dialog->remoteTabTitleText();
        const QColor tabColor = dialog->color();

        if (tabTitle != sessionLocalTabTitleFormat) {
            session()->setTabTitleFormat(Session::LocalTabTitle, tabTitle);
            Q_EMIT tabRenamedByUser(true);
            // trigger an update of the tab text
            snapshot();
        }

        if (remoteTabTitle != sessionRemoteTabTitleFormat) {
            session()->setTabTitleFormat(Session::RemoteTabTitle, remoteTabTitle);
            Q_EMIT tabRenamedByUser(true);
            snapshot();
        }

        if (tabColor != sessionTabColor) {
            session()->setColor(tabColor);
            Q_EMIT tabColoredByUser(true);
            snapshot();
        }
    });

    dialog->show();
}

// This is called upon Menu->Close Session and right-click on tab->Close Tab
bool SessionController::confirmClose() const
{
    if (session()->isForegroundProcessActive()) {
        QString title = session()->foregroundProcessName();

        // hard coded for now.  In future make it possible for the user to specify which programs
        // are ignored when considering whether to display a confirmation
        QStringList ignoreList;
        ignoreList << QString::fromUtf8(qgetenv("SHELL")).section(QLatin1Char('/'), -1);
        if (ignoreList.contains(title)) {
            return true;
        }

        QString question;
        if (title.isEmpty()) {
            question = i18n(
                "A program is currently running in this session."
                "  Are you sure you want to close it?");
        } else {
            question = i18n(
                "The program '%1' is currently running in this session."
                "  Are you sure you want to close it?",
                title);
        }

        int result = KMessageBox::warningYesNo(view()->window(),
                                               question,
                                               i18n("Confirm Close"),
                                               KGuiItem(i18nc("@action:button", "Close Program"), QStringLiteral("application-exit")),
                                               KStandardGuiItem::cancel(),
                                               QStringLiteral("CloseSingleTab"));
        return result == KMessageBox::Yes;
    }
    return true;
}
bool SessionController::confirmForceClose() const
{
    if (session()->isRunning()) {
        QString title = session()->program();

        // hard coded for now.  In future make it possible for the user to specify which programs
        // are ignored when considering whether to display a confirmation
        QStringList ignoreList;
        ignoreList << QString::fromUtf8(qgetenv("SHELL")).section(QLatin1Char('/'), -1);
        if (ignoreList.contains(title)) {
            return true;
        }

        QString question;
        if (title.isEmpty()) {
            question = i18n(
                "A program in this session would not die."
                "  Are you sure you want to kill it by force?");
        } else {
            question = i18n(
                "The program '%1' is in this session would not die."
                "  Are you sure you want to kill it by force?",
                title);
        }

        int result = KMessageBox::warningYesNo(view()->window(),
                                               question,
                                               i18n("Confirm Close"),
                                               KGuiItem(i18nc("@action:button", "Kill Program"), QStringLiteral("application-exit")),
                                               KStandardGuiItem::cancel());
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

    if (!session()->closeInNormalWay()) {
        if (!confirmForceClose()) {
            return;
        }

        if (!session()->closeInForceWay()) {
            qCDebug(KonsoleDebug) << "Konsole failed to close a session in any way.";
            return;
        }
    }

    if (factory() != nullptr) {
        factory()->removeClient(this);
    }
}

// Trying to open a remote Url may produce unexpected results.
// Therefore, if a remote url, open the user's home path.
// TODO consider: 1) disable menu upon remote session
//   2) transform url to get the desired result (ssh -> sftp, etc)
void SessionController::openBrowser()
{
    // if we requested the browser on a file, we can't use OpenUrlJob
    // because it does not open the file in a browser, it opens another program
    // based on it's mime type.
    // so force open dolphin with  it selected.
    // TODO: and for people that have other default file browsers such as
    // konqueror and krusader?

    if (_currentHotSpot && _currentHotSpot->type() == HotSpot::File) {
        auto *fileHotSpot = qobject_cast<FileFilterHotSpot *>(_currentHotSpot.get());
        assert(fileHotSpot);
        auto *job = new KIO::OpenFileManagerWindowJob();
        job->setHighlightUrls({fileHotSpot->fileItem().url()});
        job->start();
    } else {
        const QUrl currentUrl = url().isLocalFile() ? url() : QUrl::fromLocalFile(QDir::homePath());
        auto *job = new KIO::OpenUrlJob(currentUrl);
        job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, QApplication::activeWindow()));
        job->start();
    }
}

void SessionController::copy()
{
    view()->copyToClipboard();
}

void SessionController::copyInput()
{
    view()->copyToClipboard(Screen::ExcludePrompt | Screen::ExcludeOutput);
}

void SessionController::copyOutput()
{
    view()->copyToClipboard(Screen::ExcludePrompt | Screen::ExcludeInput);
}

void SessionController::copyInputOutput()
{
    view()->copyToClipboard(Screen::ExcludePrompt);
}

void SessionController::paste()
{
    view()->pasteFromClipboard();
}
void SessionController::pasteFromX11Selection()
{
    view()->pasteFromX11Selection();
}
void SessionController::selectAll()
{
    view()->selectAll();
}
void SessionController::selectLine()
{
    view()->selectCurrentLine();
}
static const KXmlGuiWindow *findWindow(const QObject *object)
{
    // Walk up the QObject hierarchy to find a KXmlGuiWindow.
    while (object != nullptr) {
        const auto *window = qobject_cast<const KXmlGuiWindow *>(object);
        if (window != nullptr) {
            return (window);
        }
        object = object->parent();
    }
    return (nullptr);
}

static bool hasTerminalDisplayInSameWindow(const Session *session, const KXmlGuiWindow *window)
{
    // Iterate all TerminalDisplays of this Session ...
    const QList<TerminalDisplay *> views = session->views();
    for (const TerminalDisplay *terminalDisplay : views) {
        // ... and check whether a TerminalDisplay has the same
        // window as given in the parameter
        if (window == findWindow(terminalDisplay)) {
            return (true);
        }
    }
    return (false);
}

void SessionController::copyInputActionsTriggered(QAction *action)
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
    const KXmlGuiWindow *myWindow = findWindow(view());

    const QList<Session *> sessionsList = SessionManager::instance()->sessions();
    QSet<Session *> group(sessionsList.begin(), sessionsList.end());
    for (auto session : group) {
        // First, ensure that the session is removed
        // (necessary to avoid duplicates on addSession()!)
        _copyToGroup->removeSession(session);

        // Add current session if it is displayed our window
        if (hasTerminalDisplayInSameWindow(session, myWindow)) {
            _copyToGroup->addSession(session);
        }
    }
    _copyToGroup->setMasterStatus(session(), true);
    _copyToGroup->setMasterMode(SessionGroup::CopyInputToAll);

    snapshot();
    Q_EMIT copyInputChanged(this);
}

void SessionController::copyInputToSelectedTabs()
{
    if (_copyToGroup == nullptr) {
        _copyToGroup = new SessionGroup(this);
        _copyToGroup->addSession(session());
        _copyToGroup->setMasterStatus(session(), true);
        _copyToGroup->setMasterMode(SessionGroup::CopyInputToAll);
    }

    auto *dialog = new CopyInputDialog(view());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(true);
    dialog->setMasterSession(session());

    const QList<Session *> sessionsList = _copyToGroup->sessions();
    QSet<Session *> currentGroup(sessionsList.begin(), sessionsList.end());

    currentGroup.remove(session());

    dialog->setChosenSessions(currentGroup);

    connect(dialog, &QDialog::accepted, this, [=]() {
        QSet<Session *> newGroup = dialog->chosenSessions();
        newGroup.remove(session());

        const QSet<Session *> completeGroup = newGroup | currentGroup;
        for (Session *session : completeGroup) {
            if (newGroup.contains(session) && !currentGroup.contains(session)) {
                _copyToGroup->addSession(session);
            } else if (!newGroup.contains(session) && currentGroup.contains(session)) {
                _copyToGroup->removeSession(session);
            }
        }

        _copyToGroup->setMasterStatus(session(), true);
        _copyToGroup->setMasterMode(SessionGroup::CopyInputToAll);
        snapshot();
        Q_EMIT copyInputChanged(this);
    });

    dialog->show();
}

void SessionController::copyInputToNone()
{
    if (_copyToGroup == nullptr) { // No 'Copy To' is active
        return;
    }

    // Once Qt5.14+ is the minimum, change to use range constructors
    const QList<Session *> groupList = SessionManager::instance()->sessions();
    QSet<Session *> group(groupList.begin(), groupList.end());

    for (auto iterator : group) {
        Session *s = iterator;

        if (s != session()) {
            _copyToGroup->removeSession(iterator);
        }
    }
    delete _copyToGroup;
    _copyToGroup = nullptr;
    snapshot();
    Q_EMIT copyInputChanged(this);
}

void SessionController::searchClosed()
{
    _isSearchBarEnabled = false;
    searchHistory(false);
}

void SessionController::updateFilterList(const Profile::Ptr &profile)
{
    if (profile != SessionManager::instance()->sessionProfile(session())) {
        return;
    }

    auto *filterChain = view()->filterChain();

    const QString currentWordCharacters = profile->wordCharacters();
    static QString _wordChars = currentWordCharacters;

    if (profile->underlineFilesEnabled()) {
        if (_fileFilter == nullptr) { // Initialize
            _fileFilter = new FileFilter(session(), currentWordCharacters);
            filterChain->addFilter(_fileFilter);
        } else {
            // If wordCharacters changed, we need to change the static regex
            // pattern in _fileFilter
            if (_wordChars != currentWordCharacters) {
                _wordChars = currentWordCharacters;
                _fileFilter->updateRegex(currentWordCharacters);
            }
        }
    } else if (_fileFilter != nullptr) { // It became disabled, clean up
        filterChain->removeFilter(_fileFilter);
        delete _fileFilter;
        _fileFilter = nullptr;
    }

    if (profile->underlineLinksEnabled()) {
        if (_urlFilter == nullptr) { // Initialize
            _urlFilter = new UrlFilter();
            filterChain->addFilter(_urlFilter);
        }
    } else if (_urlFilter != nullptr) { // It became disabled, clean up
        filterChain->removeFilter(_urlFilter);
        delete _urlFilter;
        _urlFilter = nullptr;
    }

    if (profile->allowEscapedLinks()) {
        if (_escapedUrlFilter == nullptr) { // Initialize
            _escapedUrlFilter = new EscapeSequenceUrlFilter(session(), view());
            filterChain->addFilter(_escapedUrlFilter);
        }
    } else if (_escapedUrlFilter != nullptr) { // It became disabled, clean up
        filterChain->removeFilter(_escapedUrlFilter);
        delete _escapedUrlFilter;
        _escapedUrlFilter = nullptr;
    }

    const bool allowColorFilters = profile->colorFilterEnabled();
    if (!allowColorFilters && (_colorFilter != nullptr)) {
        filterChain->removeFilter(_colorFilter);
        delete _colorFilter;
        _colorFilter = nullptr;
    } else if (allowColorFilters && (_colorFilter == nullptr)) {
        _colorFilter = new ColorFilter();
        filterChain->addFilter(_colorFilter);
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

    connect(view()->screenWindow(), &Konsole::ScreenWindow::outputChanged, this, &Konsole::SessionController::updateSearchFilter);
    connect(view()->screenWindow(), &Konsole::ScreenWindow::scrolled, this, &Konsole::SessionController::updateSearchFilter);
    connect(view()->screenWindow(), &Konsole::ScreenWindow::currentResultLineChanged, view(), QOverload<>::of(&Konsole::TerminalDisplay::update));

    _listenForScreenWindowUpdates = true;
}

void SessionController::updateSearchFilter()
{
    if ((_searchFilter != nullptr) && (!_searchBar.isNull())) {
        view()->processFilters();
    }
}

void SessionController::searchBarEvent()
{
    QString selectedText = view()->screenWindow()->selectedText(Screen::PreserveLineBreaks | Screen::TrimLeadingWhitespace | Screen::TrimTrailingWhitespace);
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
        disconnect(_searchBar, &Konsole::IncrementalSearchBar::searchChanged, this, &Konsole::SessionController::searchTextChanged);
        disconnect(_searchBar, &Konsole::IncrementalSearchBar::searchReturnPressed, this, &Konsole::SessionController::findPreviousInHistory);
        disconnect(_searchBar, &Konsole::IncrementalSearchBar::searchShiftPlusReturnPressed, this, &Konsole::SessionController::findNextInHistory);
        if ((!view().isNull()) && (view()->screenWindow() != nullptr)) {
            view()->screenWindow()->setCurrentResultLine(-1);
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
            view()->filterChain()->addFilter(_searchFilter);
            view()->processFilters();

            setFindNextPrevEnabled(true);
        } else {
            setFindNextPrevEnabled(false);

            removeSearchFilter();

            view()->setFocus(Qt::ActiveWindowFocusReason);
        }
    }
}

void SessionController::setFindNextPrevEnabled(bool enabled)
{
    _findNextAction->setEnabled(enabled);
    _findPreviousAction->setEnabled(enabled);
}
void SessionController::searchTextChanged(const QString &text)
{
    Q_ASSERT(view()->screenWindow());

    if (_searchText == text) {
        return;
    }

    _searchText = text;

    if (text.isEmpty()) {
        view()->screenWindow()->clearSelection();
        view()->screenWindow()->scrollTo(_searchStartLine);
    }

    // update search.  this is called even when the text is
    // empty to clear the view's filters
    beginSearch(text, reverseSearchChecked() ? Enum::BackwardsSearch : Enum::ForwardsSearch);
}
void SessionController::searchCompleted(bool success)
{
    _prevSearchResultLine = view()->screenWindow()->currentResultLine();

    if (!_searchBar.isNull()) {
        _searchBar->setFoundMatch(success);
    }
}

void SessionController::beginSearch(const QString &text, Enum::SearchDirection direction)
{
    Q_ASSERT(_searchBar);
    Q_ASSERT(_searchFilter);

    QRegularExpression regExp = regexpFromSearchBarOptions();
    _searchFilter->setRegExp(regExp);

    if (_searchStartLine < 0 || _searchStartLine > view()->screenWindow()->lineCount()) {
        if (direction == Enum::ForwardsSearch) {
            setSearchStartTo(view()->screenWindow()->currentLine());
        } else {
            setSearchStartTo(view()->screenWindow()->currentLine() + view()->screenWindow()->windowLines());
        }
    }

    if (!regExp.pattern().isEmpty()) {
        view()->screenWindow()->setCurrentResultLine(-1);
        auto task = new SearchHistoryTask(this);

        connect(task, &Konsole::SearchHistoryTask::completed, this, &Konsole::SessionController::searchCompleted);

        task->setRegExp(regExp);
        task->setSearchDirection(direction);
        task->setAutoDelete(true);
        task->setStartLine(_searchStartLine);
        task->addScreenWindow(session(), view()->screenWindow());
        task->execute();
    } else if (text.isEmpty()) {
        searchCompleted(false);
    }

    view()->processFilters();
}
void SessionController::highlightMatches(bool highlight)
{
    if (highlight) {
        view()->filterChain()->addFilter(_searchFilter);
        view()->processFilters();
    } else {
        view()->filterChain()->removeFilter(_searchFilter);
    }

    view()->update();
}

void SessionController::searchFrom()
{
    Q_ASSERT(_searchBar);
    Q_ASSERT(_searchFilter);

    if (reverseSearchChecked()) {
        setSearchStartTo(view()->screenWindow()->lineCount());
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
void SessionController::updateMenuIconsAccordingToReverseSearchSetting()
{
    if (reverseSearchChecked()) {
        _findNextAction->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
        _findPreviousAction->setIcon(QIcon::fromTheme(QStringLiteral("go-down")));
    } else {
        _findNextAction->setIcon(QIcon::fromTheme(QStringLiteral("go-down")));
        _findPreviousAction->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    }
}
void SessionController::changeSearchMatch()
{
    Q_ASSERT(_searchBar);
    Q_ASSERT(_searchFilter);

    // reset Selection for new case match
    view()->screenWindow()->clearSelection();
    beginSearch(_searchBar->searchText(), reverseSearchChecked() ? Enum::BackwardsSearch : Enum::ForwardsSearch);
}
void SessionController::showHistoryOptions()
{
    auto *dialog = new HistorySizeDialog(QApplication::activeWindow());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(true);

    const HistoryType &currentHistory = session()->historyType();
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

    connect(dialog, &QDialog::accepted, this, [this, dialog]() {
        scrollBackOptionsChanged(dialog->mode(), dialog->lineCount());
    });

    dialog->show();
}
void SessionController::sessionResizeRequest(const QSize &size)
{
    ////qDebug() << "View resize requested to " << size;
    view()->setSize(size.width(), size.height());
}
void SessionController::scrollBackOptionsChanged(int mode, int lines)
{
    switch (mode) {
    case Enum::NoHistory:
        session()->setHistoryType(HistoryTypeNone());
        break;
    case Enum::FixedSizeHistory:
        session()->setHistoryType(CompactHistoryType(lines));
        break;
    case Enum::UnlimitedHistory:
        session()->setHistoryType(HistoryTypeFile());
        break;
    }
}

void SessionController::saveHistory()
{
    SessionTask *task = new SaveHistoryTask(this);
    task->setAutoDelete(true);
    task->addSession(session());
    task->execute();
}

void SessionController::clearHistory()
{
    session()->clearHistory();
    view()->updateImage(); // To reset view scrollbar
    view()->repaint();
}

void SessionController::clearHistoryAndReset()
{
    Profile::Ptr profile = SessionManager::instance()->sessionProfile(session());
    QByteArray name = profile->defaultEncoding().toUtf8();

    Emulation *emulation = session()->emulation();
    emulation->reset(false, true);
    session()->refresh();
    session()->setCodec(QTextCodec::codecForName(name));
    clearHistory();
}

void SessionController::increaseFontSize()
{
    view()->terminalFont()->increaseFontSize();
}

void SessionController::decreaseFontSize()
{
    view()->terminalFont()->decreaseFontSize();
}

void SessionController::resetFontSize()
{
    view()->terminalFont()->resetFontSize();
}

void SessionController::monitorActivity(bool monitor)
{
    session()->setMonitorActivity(monitor);
}
void SessionController::monitorSilence(bool monitor)
{
    session()->setMonitorSilence(monitor);
}
void SessionController::monitorProcessFinish(bool monitor)
{
    _monitorProcessFinish = monitor;
}
void SessionController::updateSessionIcon()
{
    // If the default profile icon is being used, don't put it on the tab
    // Only show the icon if the user specifically chose one
    if (session()->iconName() == QStringLiteral("utilities-terminal")) {
        _sessionIconName = QString();
    } else {
        _sessionIconName = session()->iconName();
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
        view()->updateReadOnlyState(readonly);
    });
}

bool SessionController::isReadOnly() const
{
    if (!session().isNull()) {
        return session()->isReadOnly();
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
    if (_sessionIconName != session()->iconName()) {
        updateSessionIcon();
    }

    QString title = session()->title(Session::DisplayedTitleRole);

    // special handling for the "%w" marker which is replaced with the
    // window title set by the shell
    title.replace(QLatin1String("%w"), session()->userTitle());
    // special handling for the "%#" marker which is replaced with the
    // number of the shell
    title.replace(QLatin1String("%#"), QString::number(session()->sessionId()));

    if (title.isEmpty()) {
        title = session()->title(Session::NameRole);
    }

    setTitle(title);
    setColor(session()->color());
    Q_EMIT rawTitleChanged();
}

void SessionController::sessionReadOnlyChanged()
{
    updateReadOnlyActionStates();

    // Update all views
    const QList<TerminalDisplay *> viewsList = session()->views();
    for (TerminalDisplay *terminalDisplay : viewsList) {
        if (terminalDisplay != view()) {
            terminalDisplay->updateReadOnlyState(isReadOnly());
        }
        Q_EMIT readOnlyChanged(this);
    }
}

void SessionController::showDisplayContextMenu(const QPoint &position)
{
    // needed to make sure the popup menu is available, even if a hosting
    // application did not merge our GUI.
    if (factory() == nullptr) {
        if (clientBuilder() == nullptr) {
            // Client builder does not get deleted automatically, we handle this
            _clientBuilder.reset(new KXMLGUIBuilder(view()));
            setClientBuilder(_clientBuilder.get());
        }

        auto factory = new KXMLGUIFactory(clientBuilder(), view());
        factory->addClient(this);
    }

    QPointer<QMenu> popup = qobject_cast<QMenu *>(factory()->container(QStringLiteral("session-popup-menu"), this));
    if (!popup.isNull()) {
        updateReadOnlyActionStates();

        auto contentSeparator = new QAction(popup);
        contentSeparator->setSeparator(true);

        // We don't actually use this shortcut, but we need to display it for consistency :/
        QAction *copy = actionCollection()->action(QStringLiteral("edit_copy_contextmenu"));
        copy->setShortcut(Konsole::ACCEL | Qt::Key_C);

        // Adds a "Open Folder With" action
        const QUrl currentUrl = url().isLocalFile() ? url() : QUrl::fromLocalFile(QDir::homePath());
        KFileItem item(currentUrl);

        const auto old = popup->actions();

        const KFileItemListProperties props({item});
        QScopedPointer<KFileItemActions> ac(new KFileItemActions());
        ac->setItemListProperties(props);

#if KIO_VERSION >= QT_VERSION_CHECK(5, 82, 0)
        ac->insertOpenWithActionsTo(popup->actions().value(4, nullptr), popup, QStringList{qApp->desktopFileName()});
#elif KIO_VERSION >= QT_VERSION_CHECK(5, 78, 0)
        ac->insertOpenWithActionsTo(popup->actions().value(4, nullptr), popup, QString());
#else
        ac->addOpenWithActionsTo(popup);
#endif

        auto newActions = popup->actions();
        for (auto *elm : old) {
            newActions.removeAll(elm);
        }
        // Finish Ading the "Open Folder With" action.

        QList<QAction *> toRemove;
        // prepend content-specific actions such as "Open Link", "Copy Email Address" etc
        _currentHotSpot = view()->filterActions(position);
        if (_currentHotSpot != nullptr) {
            popup->insertActions(popup->actions().value(0, nullptr), _currentHotSpot->actions() << contentSeparator);
            popup->addAction(contentSeparator);
            toRemove = _currentHotSpot->setupMenu(popup.data());

            // The action above can create an action for Open Folder With,
            // for the selected folder, but then we have two different
            // Open Folder With - with different folders on each.
            // Change the text of the second one, that points to the
            // current folder.
            for (auto *action : newActions) {
                if (action->objectName() == QStringLiteral("openWith_submenu")) {
                    action->setText(i18n("Open Current Folder With"));
                }
            }
            toRemove = toRemove + newActions;
        } else {
            toRemove = newActions;
        }

        // always update this submenu before showing the context menu,
        // because the available search services might have changed
        // since the context menu is shown last time
        updateWebSearchMenu();

        _preventClose = true;

        if (_showMenuAction != nullptr) {
            if (_showMenuAction->isChecked()) {
                popup->removeAction(_showMenuAction);
            } else {
                popup->insertAction(_switchProfileMenu, _showMenuAction);
            }
        }

        // they are here.
        // qDebug() << popup->actions().indexOf(contentActions[0]) << popup->actions().indexOf(contentActions[1]) << popup->actions()[3];
        QAction *chosen = popup->exec(QCursor::pos());

        // check for validity of the pointer to the popup menu
        if (!popup.isNull()) {
            delete contentSeparator;
            // Remove the 'Open with' actions from it.
            for (auto *act : toRemove) {
                popup->removeAction(act);
            }

            // Remove the Accelerator for the copy shortcut so we don't have two actions with same shortcut.
            copy->setShortcut({});
        }

        // This should be at the end, to prevent crashes if the session
        // is closed from the menu in e.g. konsole kpart
        _preventClose = false;
        if ((chosen != nullptr) && chosen->objectName() == QLatin1String("close-session")) {
            chosen->trigger();
        }
    } else {
        qCDebug(KonsoleDebug) << "Unable to display popup menu for session" << session()->title(Session::NameRole)
                              << ", no GUI factory available to build the popup.";
    }
}

void SessionController::movementKeyFromSearchBarReceived(QKeyEvent *event)
{
    QCoreApplication::sendEvent(view(), event);
    setSearchStartToWindowCurrentLine();
}

void SessionController::sessionNotificationsChanged(Session::Notification notification, bool enabled)
{
    Q_EMIT notificationChanged(this, notification, enabled);
}

void SessionController::zmodemDownload()
{
    QString zmodem = QStandardPaths::findExecutable(QStringLiteral("rz"));
    if (zmodem.isEmpty()) {
        zmodem = QStandardPaths::findExecutable(QStringLiteral("lrz"));
    }
    if (!zmodem.isEmpty()) {
        const QString path = QFileDialog::getExistingDirectory(view(),
                                                               i18n("Save ZModem Download to..."),
                                                               QDir::homePath(),
                                                               QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

        if (!path.isEmpty()) {
            session()->startZModem(zmodem, path, QStringList());
            return;
        }
    } else {
        KMessageBox::error(view(),
                           i18n("<p>A ZModem file transfer attempt has been detected, "
                                "but no suitable ZModem software was found on this system.</p>"
                                "<p>You may wish to install the 'rzsz' or 'lrzsz' package.</p>"));
    }
    session()->cancelZModem();
}

void SessionController::zmodemUpload()
{
    if (session()->isZModemBusy()) {
        KMessageBox::sorry(view(), i18n("<p>The current session already has a ZModem file transfer in progress.</p>"));
        return;
    }

    QString zmodem = QStandardPaths::findExecutable(QStringLiteral("sz"));
    if (zmodem.isEmpty()) {
        zmodem = QStandardPaths::findExecutable(QStringLiteral("lsz"));
    }
    if (zmodem.isEmpty()) {
        KMessageBox::sorry(view(),
                           i18n("<p>No suitable ZModem software was found on this system.</p>"
                                "<p>You may wish to install the 'rzsz' or 'lrzsz' package.</p>"));
        return;
    }

    QStringList files = QFileDialog::getOpenFileNames(view(), i18n("Select Files for ZModem Upload"), QDir::homePath());
    if (!files.isEmpty()) {
        session()->startZModem(zmodem, QString(), files);
    }
}

bool SessionController::isKonsolePart() const
{
    // Check to see if we are being called from Konsole or a KPart
    return !(qApp->applicationName() == QLatin1String("konsole"));
}

QString SessionController::userTitle() const
{
    if (!session().isNull()) {
        return session()->userTitle();
    } else {
        return QString();
    }
}

bool SessionController::isValid() const
{
    return _sessionDisplayConnection->isValid();
}

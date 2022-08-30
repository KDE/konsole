/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>
    SPDX-FileCopyrightText: 2009 Thomas Dreibholz <dreibh@iem.uni-due.de>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "Session.h"

// Standard
#include <csignal>
#include <cstdlib>
#include <unistd.h>

// Qt
#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QKeyEvent>

// KDE
#include <KConfigGroup>
#include <KIO/DesktopExecParser>
#include <KLocalizedString>
#include <KNotification>
#include <KProcess>
#include <KPtyDevice>
#include <KShell>
#include <kcoreaddons_version.h>

// Konsole
#include <sessionadaptor.h>

#include "Pty.h"
#include "SSHProcessInfo.h"
#include "SessionManager.h"
#include "ShellCommand.h"
#include "Vt102Emulation.h"
#include "ZModemDialog.h"
#include "history/HistoryTypeFile.h"
#include "history/HistoryTypeNone.h"
#include "history/compact/CompactHistoryType.h"
#include "konsoledebug.h"
#include "profile/Profile.h"
#include "profile/ProfileManager.h"

#include "terminalDisplay/TerminalDisplay.h"
#include "terminalDisplay/TerminalFonts.h"
#include "terminalDisplay/TerminalScrollBar.h"

// Linux
#if HAVE_GETPWUID
#include <pwd.h>
#include <sys/types.h>
#endif

#if KCOREADDONS_VERSION >= QT_VERSION_CHECK(5, 97, 0)
#include <KSandbox>
#endif

using namespace Konsole;

int Session::lastSessionId = 0;
static bool show_disallow_certain_dbus_methods_message = true;

static const int ZMODEM_BUFFER_SIZE = 1048576; // 1 Mb

Session::Session(QObject *parent)
    : QObject(parent)
{
    _uniqueIdentifier = QUuid::createUuid();

    // prepare DBus communication
    new SessionAdaptor(this);
    _sessionId = ++lastSessionId;
    QDBusConnection::sessionBus().registerObject(QLatin1String("/Sessions/") + QString::number(_sessionId), this);

    // create emulation backend
    _emulation = new Vt102Emulation();
    _emulation->reset();

    connect(_emulation, &Konsole::Emulation::sessionAttributeChanged, this, &Konsole::Session::setSessionAttribute);
    connect(_emulation, &Konsole::Emulation::bell, this, [this]() {
        Q_EMIT bellRequest(i18n("Bell in '%1' (Session '%2')", _displayTitle, _nameTitle));
        this->setPendingNotification(Notification::Bell);
    });
    connect(_emulation, &Konsole::Emulation::zmodemDownloadDetected, this, &Konsole::Session::fireZModemDownloadDetected);
    connect(_emulation, &Konsole::Emulation::zmodemUploadDetected, this, &Konsole::Session::fireZModemUploadDetected);
    connect(_emulation, &Konsole::Emulation::profileChangeCommandReceived, this, &Konsole::Session::profileChangeCommandReceived);
    connect(_emulation, &Konsole::Emulation::flowControlKeyPressed, this, &Konsole::Session::updateFlowControlState);
    connect(_emulation, &Konsole::Emulation::primaryScreenInUse, this, &Konsole::Session::onPrimaryScreenInUse);
    connect(_emulation, &Konsole::Emulation::selectionChanged, this, &Konsole::Session::selectionChanged);
    connect(_emulation, &Konsole::Emulation::imageResizeRequest, this, &Konsole::Session::resizeRequest);
    connect(_emulation, &Konsole::Emulation::sessionAttributeRequest, this, &Konsole::Session::sessionAttributeRequest);

    // create new teletype for I/O with shell process
    openTeletype(-1, true);

    // setup timer for monitoring session activity & silence
    _silenceTimer = new QTimer(this);
    _silenceTimer->setSingleShot(true);
    connect(_silenceTimer, &QTimer::timeout, this, &Konsole::Session::silenceTimerDone);

    _activityTimer = new QTimer(this);
    _activityTimer->setSingleShot(true);
    connect(_activityTimer, &QTimer::timeout, this, &Konsole::Session::activityTimerDone);
}

Session::~Session()
{
    delete _foregroundProcessInfo;
    delete _sessionProcessInfo;
    delete _emulation;
    delete _shellProcess;
    delete _zmodemProc;
}

void Session::openTeletype(int fd, bool runShell)
{
    if (isRunning()) {
        qWarning() << "Attempted to open teletype in a running session.";
        return;
    }

    delete _shellProcess;

    if (fd < 0) {
        _shellProcess = new Pty();
    } else {
        _shellProcess = new Pty(fd);
    }

    _shellProcess->setUtf8Mode(_emulation->utf8());

    // connect the I/O between emulator and pty process
    connect(_shellProcess, &Konsole::Pty::receivedData, this, &Konsole::Session::onReceiveBlock);
    connect(_emulation, &Konsole::Emulation::sendData, _shellProcess, &Konsole::Pty::sendData);

    // UTF8 mode
    connect(_emulation, &Konsole::Emulation::useUtf8Request, _shellProcess, &Konsole::Pty::setUtf8Mode);

    // get notified when the pty process is finished
    connect(_shellProcess, QOverload<int, QProcess::ExitStatus>::of(&Konsole::Pty::finished), this, &Konsole::Session::done);

    // emulator size
    connect(_emulation, &Konsole::Emulation::imageSizeChanged, this, &Konsole::Session::updateWindowSize);
    if (fd < 0 || runShell) {
        // Using a queued connection guarantees that starting the session
        // is delayed until all (both) image size updates at startup have
        // been processed. See #203185 and #412598.
        connect(_emulation, &Konsole::Emulation::imageSizeInitialized, this, &Konsole::Session::run, Qt::QueuedConnection);
    } else {
        // run needs to be disconnected, as it may be already connected by the constructor
        disconnect(_emulation, &Konsole::Emulation::imageSizeInitialized, this, &Konsole::Session::run);
    }
}

WId Session::windowId() const
{
    // Returns a window ID for this session which is used
    // to set the WINDOWID environment variable in the shell
    // process.
    //
    // Sessions can have multiple views or no views, which means
    // that a single ID is not always going to be accurate.
    //
    // If there are no views, the window ID is just 0.  If
    // there are multiple views, then the window ID for the
    // top-level window which contains the first view is
    // returned

    if (_views.count() == 0) {
        return 0;
    } else {
        /**
         * compute the windows id to use
         * doesn't call winId on some widget, as this might lead
         * to rendering artifacts as this will trigger the
         * creation of a native window, see https://doc.qt.io/qt-5/qwidget.html#winId
         * instead, use https://doc.qt.io/qt-5/qwidget.html#effectiveWinId
         */
        QWidget *widget = _views.first();
        Q_ASSERT(widget);
        return widget->effectiveWinId();
    }
}

void Session::setDarkBackground(bool darkBackground)
{
    _hasDarkBackground = darkBackground;
}

bool Session::isRunning() const
{
    return (_shellProcess != nullptr) && (_shellProcess->state() == QProcess::Running);
}

bool Session::hasFocus() const
{
    return std::any_of(_views.constBegin(), _views.constEnd(), [](const TerminalDisplay *display) {
        return display->hasFocus();
    });
}

void Session::setCodec(QTextCodec *codec)
{
    if (isReadOnly()) {
        return;
    }

    emulation()->setCodec(codec);

    Q_EMIT sessionCodecChanged(codec);
}

bool Session::setCodec(const QByteArray &name)
{
    QTextCodec *codec = QTextCodec::codecForName(name);

    if (codec != nullptr) {
        setCodec(codec);
        return true;
    } else {
        return false;
    }
}

QByteArray Session::codec()
{
    return _emulation->codec()->name();
}

void Session::setProgram(const QString &program)
{
    _program = ShellCommand::expand(program);
}

void Session::setArguments(const QStringList &arguments)
{
    _arguments = ShellCommand::expand(arguments);
}

void Session::setInitialWorkingDirectory(const QString &dir)
{
    _initialWorkingDir = validDirectory(KShell::tildeExpand(ShellCommand::expand(dir)));
}

QString Session::currentWorkingDirectory()
{
    if (_reportedWorkingUrl.isValid() && _reportedWorkingUrl.isLocalFile()) {
        return _reportedWorkingUrl.path();
    }

    // only returned cached value
    if (_currentWorkingDir.isEmpty()) {
        updateWorkingDirectory();
    }

    return _currentWorkingDir;
}
void Session::updateWorkingDirectory()
{
    updateSessionProcessInfo();

    const QString currentDir = _sessionProcessInfo->validCurrentDir();
    if (currentDir != _currentWorkingDir) {
        _currentWorkingDir = currentDir;
        Q_EMIT currentDirectoryChanged(_currentWorkingDir);
    }
}

QList<TerminalDisplay *> Session::views() const
{
    return _views;
}

void Session::addView(TerminalDisplay *widget)
{
    Q_ASSERT(!_views.contains(widget));

    _views.append(widget);

    // connect emulation - view signals and slots
    connect(widget, &Konsole::TerminalDisplay::keyPressedSignal, _emulation, &Konsole::Emulation::sendKeyEvent);
    connect(widget, &Konsole::TerminalDisplay::mouseSignal, _emulation, &Konsole::Emulation::sendMouseEvent);
    connect(widget, &Konsole::TerminalDisplay::sendStringToEmu, _emulation, &Konsole::Emulation::sendString);
    connect(widget, &Konsole::TerminalDisplay::peekPrimaryRequested, _emulation, &Konsole::Emulation::setPeekPrimary);

    // allow emulation to notify the view when the foreground process
    // indicates whether or not it is interested in Mouse Tracking events
    connect(_emulation, &Konsole::Emulation::programRequestsMouseTracking, widget, &Konsole::TerminalDisplay::setUsesMouseTracking);

    widget->setUsesMouseTracking(_emulation->programUsesMouseTracking());

    connect(_emulation, &Konsole::Emulation::enableAlternateScrolling, widget->scrollBar(), &Konsole::TerminalScrollBar::setAlternateScrolling);

    connect(_emulation, &Konsole::Emulation::programBracketedPasteModeChanged, widget, &Konsole::TerminalDisplay::setBracketedPasteMode);

    widget->setBracketedPasteMode(_emulation->programBracketedPasteMode());

    widget->setScreenWindow(_emulation->createWindow());

    _emulation->setCurrentTerminalDisplay(widget);

    // connect view signals and slots
    connect(widget, &Konsole::TerminalDisplay::changedContentSizeSignal, this, &Konsole::Session::onViewSizeChange);

    connect(widget, &Konsole::TerminalDisplay::destroyed, this, &Konsole::Session::viewDestroyed);

    connect(widget, &Konsole::TerminalDisplay::compositeFocusChanged, _emulation, &Konsole::Emulation::focusChanged);

    connect(_emulation, &Konsole::Emulation::setCursorStyleRequest, widget, &Konsole::TerminalDisplay::setCursorStyle);
    connect(_emulation, &Konsole::Emulation::resetCursorStyleRequest, widget, &Konsole::TerminalDisplay::resetCursorStyle);

    connect(widget, &Konsole::TerminalDisplay::keyPressedSignal, this, &Konsole::Session::resetNotifications);
}

void Session::viewDestroyed(QObject *view)
{
    auto *display = reinterpret_cast<TerminalDisplay *>(view);

    Q_ASSERT(_views.contains(display));

    removeView(display);
}

void Session::removeView(TerminalDisplay *widget)
{
    _views.removeAll(widget);

    disconnect(widget, nullptr, this, nullptr);

    // disconnect
    //  - key presses signals from widget
    //  - mouse activity signals from widget
    //  - string sending signals from widget
    //
    //  ... and any other signals connected in addView()
    disconnect(widget, nullptr, _emulation, nullptr);

    // disconnect state change signals emitted by emulation
    disconnect(_emulation, nullptr, widget, nullptr);

    // close the session automatically when the last view is removed
    if (_views.count() == 0) {
        close();
    }
}

// Upon a KPty error, there is no description on what that error was...
// Check to see if the given program is executable.
QString Session::checkProgram(const QString &program)
{
    QString exec = program;

    if (exec.isEmpty()) {
        return QString();
    }

    QFileInfo info(exec);
    if (info.isAbsolute() && info.exists() && info.isExecutable()) {
        return exec;
    }

    exec = KIO::DesktopExecParser::executablePath(exec);
    exec = KShell::tildeExpand(exec);
    const QString pexec = QStandardPaths::findExecutable(exec);
    if (pexec.isEmpty()) {
        qCritical() << i18n("Could not find binary: ") << exec;
        return QString();
    }

    return exec;
}

void Session::terminalWarning(const QString &message)
{
    static const QByteArray warningText = i18nc("@info:shell Alert the user with red color text", "Warning: ").toLocal8Bit();
    QByteArray messageText = message.toLocal8Bit();

    static const char redPenOn[] = "\033[1m\033[31m";
    static const char redPenOff[] = "\033[0m";

    _emulation->receiveData(redPenOn, qstrlen(redPenOn));
    _emulation->receiveData("\n\r\n\r", 4);
    _emulation->receiveData(warningText.constData(), qstrlen(warningText.constData()));
    _emulation->receiveData(messageText.constData(), qstrlen(messageText.constData()));
    _emulation->receiveData("\n\r\n\r", 4);
    _emulation->receiveData(redPenOff, qstrlen(redPenOff));
}

QString Session::shellSessionId() const
{
    QString friendlyUuid(_uniqueIdentifier.toString());
    friendlyUuid.remove(QLatin1Char('-')).remove(QLatin1Char('{')).remove(QLatin1Char('}'));

    return friendlyUuid;
}

void Session::run()
{
    // FIXME: run() is called twice in some instances
    if (isRunning()) {
        qCDebug(KonsoleDebug) << "Attempted to re-run an already running session (" << processId() << ")";
        return;
    }

    // check that everything is in place to run the session
    if (_program.isEmpty()) {
        qWarning() << "Program to run not set.";
    }
    if (_arguments.isEmpty()) {
        qWarning() << "No command line arguments specified.";
    }
    if (_uniqueIdentifier.isNull()) {
        _uniqueIdentifier = QUuid::createUuid();
    }

    QStringList programs = {_program, QString::fromUtf8(qgetenv("SHELL")), QStringLiteral("/bin/sh")};

#if HAVE_GETPWUID
    auto pw = getpwuid(getuid());
    // pw: Do not pass the returned pointer to free.
    if (pw != nullptr) {
#if KCOREADDONS_VERSION >= QT_VERSION_CHECK(5, 97, 0)
        if (KSandbox::isFlatpak()) {
            QProcess proc;
            proc.setProgram(QStringLiteral("getent"));
            proc.setArguments({QStringLiteral("passwd"), QString::number(pw->pw_uid)});
            KSandbox::startHostProcess(proc);
            proc.waitForFinished();
            const auto shell = proc.readAllStandardOutput().simplified().split(':').at(6);
            programs.insert(1, QString::fromUtf8(shell));
        } else {
            programs.insert(1, QString::fromLocal8Bit(pw->pw_shell));
        }
#else
        programs.insert(1, QString::fromLocal8Bit(pw->pw_shell));
#endif
    }
#endif

    QString exec;
    for (const auto &choice : programs) {
        exec = checkProgram(choice);
        if (!exec.isEmpty()) {
            break;
        }
    }

    // if nothing could be found (not even the fallbacks), print a warning and do not run
    if (exec.isEmpty()) {
        terminalWarning(i18n("Could not find an interactive shell to start."));
        return;
    }

    // if a program was specified via setProgram(), but it couldn't be found (but a fallback was), print a warning
    if (exec != checkProgram(_program)) {
        terminalWarning(i18n("Could not find '%1', starting '%2' instead.  Please check your profile settings.", _program, exec));
    } else if (exec != checkProgram(exec)) {
        terminalWarning(i18n("Could not find '%1', starting '%2' instead.  Please check your profile settings.", exec, checkProgram(exec)));
    }

    // if no arguments are specified, fall back to program name
    QStringList arguments = _arguments.join(QLatin1Char(' ')).isEmpty() ? QStringList() << exec : _arguments;

    // For historical reasons, the first argument in _arguments is the
    // name of the program to execute, remove it in favor of the actual program name
    Q_ASSERT(arguments.count() >= 1);
    arguments = arguments.mid(1);

    if (!_initialWorkingDir.isEmpty()) {
        _shellProcess->setInitialWorkingDirectory(_initialWorkingDir);
    } else {
        _shellProcess->setInitialWorkingDirectory(QDir::currentPath());
    }

    _shellProcess->setFlowControlEnabled(_flowControlEnabled);
    _shellProcess->setEraseChar(_emulation->eraseChar());
    _shellProcess->setUseUtmp(_addToUtmp);
#if KCOREADDONS_VERSION >= QT_VERSION_CHECK(5, 97, 0)
    if (KSandbox::isFlatpak()) {
        _shellProcess->pty()->setCTtyEnabled(false); // not possibly inside sandbox
    }
#endif

    // this is not strictly accurate use of the COLORFGBG variable.  This does not
    // tell the terminal exactly which colors are being used, but instead approximates
    // the color scheme as "black on white" or "white on black" depending on whether
    // the background color is deemed dark or not
    const QString backgroundColorHint = _hasDarkBackground ? QStringLiteral("COLORFGBG=15;0") : QStringLiteral("COLORFGBG=0;15");
    addEnvironmentEntry(backgroundColorHint);

    addEnvironmentEntry(QStringLiteral("SHELL_SESSION_ID=%1").arg(shellSessionId()));

    addEnvironmentEntry(QStringLiteral("WINDOWID=%1").arg(QString::number(windowId())));

    const QString dbusService = QDBusConnection::sessionBus().baseService();
    addEnvironmentEntry(QStringLiteral("KONSOLE_DBUS_SERVICE=%1").arg(dbusService));

    const QString dbusObject = QStringLiteral("/Sessions/%1").arg(QString::number(_sessionId));
    addEnvironmentEntry(QStringLiteral("KONSOLE_DBUS_SESSION=%1").arg(dbusObject));

#if KCOREADDONS_VERSION >= QT_VERSION_CHECK(5, 97, 0)
    _shellProcess->setProgram(exec);
    _shellProcess->setArguments(arguments);
    _shellProcess->setEnvironment(_environment);
    const auto context = KSandbox::makeHostContext(*_shellProcess);
    // The Pty class is incredibly janky and will topple over when starting with environment, so unset it again.
    _shellProcess->setEnvironment({});
    const auto result = _shellProcess->start(context.program, context.arguments, _environment);
#else
    int result = _shellProcess->start(exec, arguments, _environment);
#endif

    if (result < 0) {
        terminalWarning(i18n("Could not start program '%1' with arguments '%2'.", exec, arguments.join(QLatin1String(" "))));
        terminalWarning(_shellProcess->errorString());
        return;
    }

    _shellProcess->setWriteable(false); // We are reachable via kwrited.

    Q_EMIT started();
}

void Session::setSessionAttribute(int what, const QString &caption)
{
    // set to true if anything has actually changed
    // eg. old _nameTitle != new _nameTitle
    bool modified = false;

    if ((what == IconNameAndWindowTitle) || (what == WindowTitle)) {
        if (_userTitle != caption) {
            _userTitle = caption;
            modified = true;
        }
    }

    if ((what == IconNameAndWindowTitle) || (what == IconName)) {
        if (_iconText != caption) {
            _iconText = caption;
            modified = true;
        }
    }

    if (what == TextColor || what == BackgroundColor) {
        QString colorString = caption.section(QLatin1Char(';'), 0, 0);
        QColor color = QColor(colorString);
        if (color.isValid()) {
            if (what == TextColor) {
                Q_EMIT changeForegroundColorRequest(color);
            } else {
                Q_EMIT changeBackgroundColorRequest(color);
            }
        }
    }

    if (what == SessionName) {
        if (_localTabTitleFormat != caption) {
            _localTabTitleFormat = caption;
            setTitle(Session::DisplayedTitleRole, caption);
            modified = true;
        }
    }

    /* The below use of 32 works but appears to non-standard.
       It is from a commit from 2004 c20973eca8776f9b4f15bee5fdcb5a3205aa69de
     */
    // change icon via \033]32;Icon\007
    if (what == SessionIcon) {
        if (_iconName != caption) {
            _iconName = caption;

            modified = true;
        }
    }

    if (what == CurrentDirectory) {
        _reportedWorkingUrl = QUrl::fromUserInput(caption);
        Q_EMIT currentDirectoryChanged(currentWorkingDirectory());
        modified = true;
    }

    if (what == ProfileChange) {
        Q_EMIT profileChangeCommandReceived(caption);
        return;
    }

    if (modified) {
        Q_EMIT sessionAttributeChanged();
    }
}

QString Session::userTitle() const
{
    return _userTitle;
}

void Session::setTabTitleFormat(TabTitleContext context, const QString &format)
{
    if (context == LocalTabTitle) {
        _localTabTitleFormat = format;
        ProcessInfo *process = getProcessInfo();
        process->setUserNameRequired(format.contains(QLatin1String("%u")));
    } else if (context == RemoteTabTitle) {
        _remoteTabTitleFormat = format;
    }
}

QString Session::tabTitleFormat(TabTitleContext context) const
{
    if (context == LocalTabTitle) {
        return _localTabTitleFormat;
    } else if (context == RemoteTabTitle) {
        return _remoteTabTitleFormat;
    }

    return QString();
}

void Session::tabTitleSetByUser(bool set)
{
    _tabTitleSetByUser = set;
}

bool Session::isTabTitleSetByUser() const
{
    return _tabTitleSetByUser;
}

void Session::tabColorSetByUser(bool set)
{
    _tabColorSetByUser = set;
}

bool Session::isTabColorSetByUser() const
{
    return _tabColorSetByUser;
}

void Session::silenceTimerDone()
{
    // FIXME: The idea here is that the notification popup will appear to tell the user than output from
    // the terminal has stopped and the popup will disappear when the user activates the session.
    //
    // This breaks with the addition of multiple views of a session.  The popup should disappear
    // when any of the views of the session becomes active

    // FIXME: Make message text for this notification and the activity notification more descriptive.
    if (!_monitorSilence) {
        setPendingNotification(Notification::Silence, false);
        return;
    }

    TerminalDisplay *view = nullptr;
    if (!_views.isEmpty()) {
        view = _views.first();
    }

    KNotification *notification = KNotification::event(hasFocus() ? QStringLiteral("Silence") : QStringLiteral("SilenceHidden"),
                                                       i18n("Silence in '%1' (Session '%2')", _displayTitle, _nameTitle),
                                                       QPixmap(),
                                                       view,
                                                       KNotification::CloseWhenWidgetActivated);
    notification->setDefaultAction(i18n("Show session"));
    connect(notification, &KNotification::defaultActivated, this, [view, notification]() {
        view->notificationClicked(notification->xdgActivationToken());
    });
    setPendingNotification(Notification::Silence);
}

void Session::activityTimerDone()
{
    _notifiedActivity = false;
}

void Session::resetNotifications()
{
    static const Notification availableNotifications[] = {Activity, Silence, Bell};
    for (auto notification : availableNotifications) {
        setPendingNotification(notification, false);
    }
}

void Session::updateFlowControlState(bool suspended)
{
    if (suspended) {
        if (flowControlEnabled()) {
            for (TerminalDisplay *display : qAsConst(_views)) {
                if (display->flowControlWarningEnabled()) {
                    display->outputSuspended(true);
                }
            }
        }
    } else {
        for (TerminalDisplay *display : qAsConst(_views)) {
            display->outputSuspended(false);
        }
    }
}

void Session::onPrimaryScreenInUse(bool use)
{
    _isPrimaryScreen = use;
    Q_EMIT primaryScreenInUse(use);
}

bool Session::isPrimaryScreen()
{
    return _isPrimaryScreen;
}

void Session::sessionAttributeRequest(int id, uint terminator)
{
    switch (id) {
    case TextColor:
        // Get 'TerminalDisplay' (_view) foreground color
        Q_EMIT getForegroundColor(terminator);
        break;
    case BackgroundColor:
        // Get 'TerminalDisplay' (_view) background color
        Q_EMIT getBackgroundColor(terminator);
        break;
    }
}

void Session::onViewSizeChange(int /* height */, int /* width */)
{
    updateTerminalSize();
}

void Session::updateTerminalSize()
{
    int minLines = -1;
    int minColumns = -1;

    // minimum number of lines and columns that views require for
    // their size to be taken into consideration ( to avoid problems
    // with new view widgets which haven't yet been set to their correct size )
    const int VIEW_LINES_THRESHOLD = 2;
    const int VIEW_COLUMNS_THRESHOLD = 2;

    // select largest number of lines and columns that will fit in all visible views
    for (TerminalDisplay *view : qAsConst(_views)) {
        if (!view->isHidden() && view->lines() >= VIEW_LINES_THRESHOLD && view->columns() >= VIEW_COLUMNS_THRESHOLD) {
            minLines = (minLines == -1) ? view->lines() : qMin(minLines, view->lines());
            minColumns = (minColumns == -1) ? view->columns() : qMin(minColumns, view->columns());
            view->processFilters();
        }
    }

    // backend emulation must have a _terminal of at least 1 column x 1 line in size
    if (minLines > 0 && minColumns > 0) {
        _emulation->setImageSize(minLines, minColumns);
    }
}
void Session::updateWindowSize(int lines, int columns)
{
    Q_ASSERT(lines > 0 && columns > 0);

    int width = 0;
    int height = 0;
    if (!_views.isEmpty()) {
        // This is somewhat arbitrary. Views having potentially different font sizes is
        // irreconcilable with the PTY user having accurate knowledge of the geometry.
        QSize cr = _views.at(0)->contentRect().size();
        width = cr.width();
        height = cr.height();
    }
    _shellProcess->setWindowSize(columns, lines, width, height);
}
void Session::refresh()
{
    // attempt to get the shell process to redraw the display
    //
    // this requires the program running in the shell
    // to cooperate by sending an update in response to
    // a window size change
    //
    // the window size is changed twice, first made slightly larger and then
    // resized back to its normal size so that there is actually a change
    // in the window size (some shells do nothing if the
    // new and old sizes are the same)
    //
    // if there is a more 'correct' way to do this, please
    // send an email with method or patches to konsole-devel@kde.org

    const QSize existingSize = _shellProcess->windowSize();
    const QSize existingPxSize = _shellProcess->pixelSize();
    _shellProcess->setWindowSize(existingSize.width() + 1, existingSize.height(), existingPxSize.width() + 1, existingPxSize.height());
    // introduce small delay to avoid changing size too quickly
    QThread::usleep(500);
    _shellProcess->setWindowSize(existingSize.width(), existingSize.height(), existingPxSize.width(), existingPxSize.height());
}

void Session::sendSignal(int signal)
{
    const ProcessInfo *process = getProcessInfo();
    bool ok = false;
    int pid;
    pid = process->foregroundPid(&ok);

    if (ok) {
        ::kill(pid, signal);
    } else {
        qWarning() << "foreground process id not set, unable to send signal " << signal;
    }
}

void Session::reportColor(SessionAttributes r, const QColor &c, uint terminator)
{
#define to65k(a) (QStringLiteral("%1").arg(int(((a)*0xFFFF)), 4, 16, QLatin1Char('0')))
    QString msg = QStringLiteral("\033]%1;rgb:").arg(r) + to65k(c.redF()) + QLatin1Char('/') + to65k(c.greenF()) + QLatin1Char('/') + to65k(c.blueF());

    // Match termination of OSC reply to termination of OSC request.
    if (terminator == '\a') { // non standard BEL terminator
        msg += QLatin1Char('\a');
    } else { // standard 7-bit ST terminator
        msg += QStringLiteral("\033\\");
    }
    _emulation->sendString(msg.toUtf8());
#undef to65k
}

void Session::reportForegroundColor(const QColor &c, uint terminator)
{
    reportColor(SessionAttributes::TextColor, c, terminator);
}

void Session::reportBackgroundColor(const QColor &c, uint terminator)
{
    reportColor(SessionAttributes::BackgroundColor, c, terminator);
}

bool Session::kill(int signal)
{
    if (processId() <= 0) {
        return false;
    }

    int result = ::kill(processId(), signal);

    if (result == 0) {
        return _shellProcess->waitForFinished(1000);
    } else {
        return false;
    }
}

void Session::close()
{
    if (isRunning()) {
        if (!closeInNormalWay()) {
            closeInForceWay();
        }
    } else {
        // terminal process has finished, just close the session
        QTimer::singleShot(1, this, [this]() {
            Q_EMIT finished(this);
        });
    }
}

bool Session::closeInNormalWay()
{
    _autoClose = true;
    _closePerUserRequest = true;

    // for the possible case where following events happen in sequence:
    //
    // 1). the terminal process crashes
    // 2). the tab stays open and displays warning message
    // 3). the user closes the tab explicitly
    //
    if (!isRunning()) {
        Q_EMIT finished(this);
        return true;
    }

    // try SIGHUP, afterwards do hard kill
    // this is the sequence used by most other terminal emulators like xterm, gnome-terminal, ...
    // see bug 401898 for details about tries to have some "soft-terminate" via EOF character
    if (kill(SIGHUP)) {
        return true;
    }

    qWarning() << "Process " << processId() << " did not die with SIGHUP";
    _shellProcess->closePty();
    return (_shellProcess->waitForFinished(1000));
}

bool Session::closeInForceWay()
{
    _autoClose = true;
    _closePerUserRequest = true;

    if (kill(SIGKILL)) {
        return true;
    } else {
        qWarning() << "Process " << processId() << " did not die with SIGKILL";
        return false;
    }
}

void Session::sendTextToTerminal(const QString &text, const QChar &eol) const
{
    if (isReadOnly()) {
        return;
    }

    if (eol.isNull()) {
        _emulation->sendText(text);
    } else {
        _emulation->sendText(text + eol);
    }
}

// Only D-Bus calls this function (via SendText or runCommand)
void Session::sendText(const QString &text) const
{
    if (isReadOnly()) {
        return;
    }

#if !REMOVE_SENDTEXT_RUNCOMMAND_DBUS_METHODS
    if (show_disallow_certain_dbus_methods_message) {
        KNotification::event(KNotification::Warning,
                             QStringLiteral("Konsole D-Bus Warning"),
                             i18n("The D-Bus methods sendText/runCommand were just used.  There are security concerns about allowing these methods to be "
                                  "public.  If desired, these methods can be changed to internal use only by re-compiling Konsole. <p>This warning will only "
                                  "show once for this Konsole instance.</p>"));

        show_disallow_certain_dbus_methods_message = false;
    }
#endif

    _emulation->sendText(text);
}

// Only D-Bus calls this function
void Session::runCommand(const QString &command) const
{
    if (isReadOnly()) {
        return;
    }

    sendText(command + QLatin1Char('\n'));
}

void Session::sendMouseEvent(int buttons, int column, int line, int eventType)
{
    if (isReadOnly()) {
        return;
    }

    _emulation->sendMouseEvent(buttons, column, line, eventType);
}

void Session::done(int exitCode, QProcess::ExitStatus exitStatus)
{
    // This slot should be triggered only one time
    disconnect(_shellProcess, QOverload<int, QProcess::ExitStatus>::of(&Konsole::Pty::finished), this, &Konsole::Session::done);

    if (!_autoClose) {
        _userTitle = i18nc("@info:shell This session is done", "Finished");
        Q_EMIT sessionAttributeChanged();
        return;
    }

    if (_closePerUserRequest) {
        Q_EMIT finished(this);
        return;
    }

    QString message;

    if (exitCode != 0) {
        if (exitStatus != QProcess::NormalExit) {
            message = i18n("Program '%1' crashed.", _program);
        } else {
            message = i18n("Program '%1' exited with status %2.", _program, exitCode);
        }

        // FIXME: See comments in Session::silenceTimerDone()
        KNotification::event(QStringLiteral("Finished"), message, QPixmap(), QApplication::activeWindow(), KNotification::CloseWhenWidgetActivated);
    }

    if (exitStatus != QProcess::NormalExit) {
        // this seeming duplicated line is for the case when exitCode is 0
        message = i18n("Program '%1' crashed.", _program);
        terminalWarning(message);
    } else {
        Q_EMIT finished(this);
    }
}

Emulation *Session::emulation() const
{
    return _emulation;
}

QString Session::keyBindings() const
{
    return _emulation->keyBindings();
}

QStringList Session::environment() const
{
    return _environment;
}

void Session::setEnvironment(const QStringList &environment)
{
    if (isReadOnly()) {
        return;
    }

    _environment = environment;
}

void Session::addEnvironmentEntry(const QString &entry)
{
    _environment << entry;
}

int Session::sessionId() const
{
    return _sessionId;
}

void Session::setKeyBindings(const QString &name)
{
    _emulation->setKeyBindings(name);
}

void Session::setTitle(TitleRole role, const QString &newTitle)
{
    if (title(role) != newTitle) {
        if (role == NameRole) {
            _nameTitle = newTitle;
        } else if (role == DisplayedTitleRole) {
            _displayTitle = newTitle;
        }

        Q_EMIT sessionAttributeChanged();
    }
}

QString Session::title(TitleRole role) const
{
    if (role == NameRole) {
        return _nameTitle;
    } else if (role == DisplayedTitleRole) {
        return _displayTitle;
    } else {
        return QString();
    }
}

ProcessInfo *Session::getProcessInfo()
{
    ProcessInfo *process = nullptr;

    if (isForegroundProcessActive() && updateForegroundProcessInfo()) {
        process = _foregroundProcessInfo;
    } else {
        updateSessionProcessInfo();
        process = _sessionProcessInfo;
    }

    return process;
}

void Session::updateSessionProcessInfo()
{
    Q_ASSERT(_shellProcess);

    bool ok;
    // The checking for pid changing looks stupid, but it is needed
    // at the moment to workaround the problem that processId() might
    // return 0
    if ((_sessionProcessInfo == nullptr) || (processId() != 0 && processId() != _sessionProcessInfo->pid(&ok))) {
        delete _sessionProcessInfo;
        _sessionProcessInfo = ProcessInfo::newInstance(processId());
        _sessionProcessInfo->setUserHomeDir();
    }
    _sessionProcessInfo->update();
}

bool Session::updateForegroundProcessInfo()
{
    Q_ASSERT(_shellProcess);

    const int foregroundPid = _shellProcess->foregroundProcessGroup();
    if (foregroundPid != _foregroundPid) {
        delete _foregroundProcessInfo;
        _foregroundProcessInfo = ProcessInfo::newInstance(foregroundPid);
        _foregroundPid = foregroundPid;
    }

    if (_foregroundProcessInfo != nullptr) {
        _foregroundProcessInfo->update();
        return _foregroundProcessInfo->isValid();
    } else {
        return false;
    }
}

bool Session::isRemote()
{
    ProcessInfo *process = getProcessInfo();

    bool ok = false;
    return (process->name(&ok) == QLatin1String("ssh") && ok);
}

QString Session::getDynamicTitle()
{
    ProcessInfo *process = getProcessInfo();
    std::unique_ptr<SSHProcessInfo> sshProcess;

    // format tab titles using process info
    bool ok = false;
    if (process->name(&ok) == QLatin1String("ssh") && ok) {
        sshProcess = std::make_unique<SSHProcessInfo>(*process);
    }

    QString currHostName = sshProcess ? sshProcess->host() : process->localHost();

    if (_currentHostName != currHostName) {
        _currentHostName = currHostName;
        Q_EMIT hostnameChanged(currHostName);
    }

    if (sshProcess) {
        QString title = tabTitleFormat(Session::RemoteTabTitle);
        title.replace(QLatin1String("%w"), userTitle());
        title.replace(QLatin1String("%#"), QString::number(sessionId()));
        return sshProcess->format(title);
    }

    /*
     * Parses an input string, looking for markers beginning with a '%'
     * character and returns a string with the markers replaced
     * with information from this process description.
     * <br>
     * The markers recognized are:
     * <ul>
     * <li> %B - User's Bourne prompt sigil ($, or # for superuser). </li>
     * <li> %u - Name of the user which owns the process. </li>
     * <li> %n - Replaced with the name of the process.   </li>
     * <li> %d - Replaced with the last part of the path name of the
     *      process' current working directory.
     *
     *      (eg. if the current directory is '/home/bob' then
     *      'bob' would be returned)
     * </li>
     * <li> %D - Replaced with the current working directory of the process. </li>
     * <li> %h - Replaced with the local host name. <li>
     * <li> %w - Replaced with the window title set by the shell. </li>
     * <li> %# - Replaced with the number of the session. <li>
     * </ul>
     */
    QString title = tabTitleFormat(Session::LocalTabTitle);
    // search for and replace known marker

    int UID = process->userId(&ok);
    if (!ok) {
        title.replace(QLatin1String("%B"), QStringLiteral("-"));
    } else {
        // title.replace(QLatin1String("%I"), QString::number(UID));
        if (UID == 0) {
            title.replace(QLatin1String("%B"), QStringLiteral("#"));
        } else {
            title.replace(QLatin1String("%B"), QStringLiteral("$"));
        }
    }

    title.replace(QLatin1String("%u"), process->userName());
    title.replace(QLatin1String("%h"), Konsole::ProcessInfo::localHost());
    title.replace(QLatin1String("%n"), process->name(&ok));

    title.replace(QLatin1String("%w"), userTitle());
    title.replace(QLatin1String("%#"), QString::number(sessionId()));

    QString dir = _reportedWorkingUrl.toLocalFile();
    ok = true;
    if (dir.isEmpty()) {
        // update current directory from process
        updateWorkingDirectory();
        // Previous process may have been freed in updateSessionProcessInfo()
        process = getProcessInfo();
        dir = process->currentDir(&ok);
    }
    if (!ok) {
        title.replace(QLatin1String("%d"), QStringLiteral("-"));
        title.replace(QLatin1String("%D"), QStringLiteral("-"));
    } else {
        // allow for shortname to have the ~ as homeDir
        const QString homeDir = process->userHomeDir();
        if (!homeDir.isEmpty()) {
            if (dir.startsWith(homeDir)) {
                dir.remove(0, homeDir.length());
                dir.prepend(QLatin1Char('~'));
            }
        }
        title.replace(QLatin1String("%D"), dir);
        title.replace(QLatin1String("%d"), process->formatShortDir(dir));
    }

    return title;
}

QUrl Session::getUrl()
{
    if (_reportedWorkingUrl.isValid()) {
        return _reportedWorkingUrl;
    }

    QString path;

    updateSessionProcessInfo();
    if (_sessionProcessInfo->isValid()) {
        bool ok = false;

        // check if foreground process is bookmark-able
        if (isForegroundProcessActive() && _foregroundProcessInfo->isValid()) {
            // for remote connections, save the user and host
            // bright ideas to get the directory at the other end are welcome :)
            if (_foregroundProcessInfo->name(&ok) == QLatin1String("ssh") && ok) {
                SSHProcessInfo sshInfo(*_foregroundProcessInfo);

                QUrl url;
                url.setScheme(QStringLiteral("ssh"));
                url.setUserName(sshInfo.userName());
                url.setHost(sshInfo.host());

                const QString port = sshInfo.port();
                if (!port.isEmpty() && port != QLatin1String("22")) {
                    url.setPort(port.toInt());
                }
                return url;
            } else {
                path = _foregroundProcessInfo->currentDir(&ok);
                if (!ok) {
                    path.clear();
                }
            }
        } else { // otherwise use the current working directory of the shell process
            path = _sessionProcessInfo->currentDir(&ok);
            if (!ok) {
                path.clear();
            }
        }
    }

    return QUrl::fromLocalFile(path);
}

void Session::setIconName(const QString &iconName)
{
    if (iconName != _iconName) {
        _iconName = iconName;
        Q_EMIT sessionAttributeChanged();
    }
}

void Session::setIconText(const QString &iconText)
{
    _iconText = iconText;
}

QString Session::iconName() const
{
    return _iconName;
}

QString Session::iconText() const
{
    return _iconText;
}

void Session::setHistoryType(const HistoryType &hType)
{
    _emulation->setHistory(hType);
}

const HistoryType &Session::historyType() const
{
    return _emulation->history();
}

void Session::clearHistory()
{
    _emulation->clearHistory();
}

QStringList Session::arguments() const
{
    return _arguments;
}

QString Session::program() const
{
    return _program;
}

bool Session::isMonitorActivity() const
{
    return _monitorActivity;
}
bool Session::isMonitorSilence() const
{
    return _monitorSilence;
}

void Session::setMonitorActivity(bool monitor)
{
    if (_monitorActivity == monitor) {
        return;
    }

    _monitorActivity = monitor;
    _notifiedActivity = false;

    // This timer is meaningful only after activity has been notified
    _activityTimer->stop();

    setPendingNotification(Notification::Activity, false);
}

void Session::setMonitorSilence(bool monitor)
{
    if (_monitorSilence == monitor) {
        return;
    }

    _monitorSilence = monitor;
    if (_monitorSilence) {
        _silenceTimer->start(_silenceSeconds * 1000);
    } else {
        _silenceTimer->stop();
    }

    setPendingNotification(Notification::Silence, false);
}

void Session::setMonitorSilenceSeconds(int seconds)
{
    _silenceSeconds = seconds;
    if (_monitorSilence) {
        _silenceTimer->start(_silenceSeconds * 1000);
    }
}

void Session::setAddToUtmp(bool add)
{
    _addToUtmp = add;
}

void Session::setAutoClose(bool close)
{
    _autoClose = close;
}

bool Session::autoClose() const
{
    return _autoClose;
}

void Session::setFlowControlEnabled(bool enabled)
{
    if (isReadOnly()) {
        return;
    }

    _flowControlEnabled = enabled;

    if (_shellProcess != nullptr) {
        _shellProcess->setFlowControlEnabled(_flowControlEnabled);
    }

    Q_EMIT flowControlEnabledChanged(enabled);
}
bool Session::flowControlEnabled() const
{
    if (_shellProcess != nullptr) {
        return _shellProcess->flowControlEnabled();
    } else {
        return _flowControlEnabled;
    }
}
void Session::fireZModemDownloadDetected()
{
    if (!_zmodemBusy) {
        QTimer::singleShot(10, this, &Konsole::Session::zmodemDownloadDetected);
        _zmodemBusy = true;
    }
}

void Session::fireZModemUploadDetected()
{
    if (!_zmodemBusy) {
        QTimer::singleShot(10, this, &Konsole::Session::zmodemUploadDetected);
    }
}

void Session::cancelZModem()
{
    _shellProcess->sendData(QByteArrayLiteral("\030\030\030\030")); // Abort
    _zmodemBusy = false;
}

void Session::startZModem(const QString &zmodem, const QString &dir, const QStringList &list)
{
    _zmodemBusy = true;
    _zmodemProc = new KProcess();
    _zmodemProc->setOutputChannelMode(KProcess::SeparateChannels);

    *_zmodemProc << zmodem << QStringLiteral("-v") << QStringLiteral("-e") << list;

    if (!dir.isEmpty()) {
        _zmodemProc->setWorkingDirectory(dir);
    }

    connect(_zmodemProc, &KProcess::readyReadStandardOutput, this, &Konsole::Session::zmodemReadAndSendBlock);
    connect(_zmodemProc, &KProcess::readyReadStandardError, this, &Konsole::Session::zmodemReadStatus);
    connect(_zmodemProc, QOverload<int, QProcess::ExitStatus>::of(&KProcess::finished), this, &Konsole::Session::zmodemFinished);

    _zmodemProc->start();

    disconnect(_shellProcess, &Konsole::Pty::receivedData, this, &Konsole::Session::onReceiveBlock);
    connect(_shellProcess, &Konsole::Pty::receivedData, this, &Konsole::Session::zmodemReceiveBlock);

    _zmodemProgress = new ZModemDialog(QApplication::activeWindow(), false, i18n("ZModem Progress"));

    connect(_zmodemProgress, &Konsole::ZModemDialog::zmodemCancel, this, &Konsole::Session::zmodemFinished);

    _zmodemProgress->show();
}

void Session::zmodemReadAndSendBlock()
{
    _zmodemProc->setReadChannel(QProcess::StandardOutput);
    QByteArray data = _zmodemProc->read(ZMODEM_BUFFER_SIZE);

    while (data.count() != 0) {
        _shellProcess->sendData(data);
        data = _zmodemProc->read(ZMODEM_BUFFER_SIZE);
    }
}

void Session::zmodemReadStatus()
{
    _zmodemProc->setReadChannel(QProcess::StandardError);
    QByteArray msg = _zmodemProc->readAll();
    while (!msg.isEmpty()) {
        int i = msg.indexOf('\015');
        int j = msg.indexOf('\012');
        QByteArray txt;
        if ((i != -1) && ((j == -1) || (i < j))) {
            msg = msg.mid(i + 1);
        } else if (j != -1) {
            txt = msg.left(j);
            msg = msg.mid(j + 1);
        } else {
            txt = msg;
            msg.truncate(0);
        }
        if (!txt.isEmpty()) {
            _zmodemProgress->addText(QString::fromLocal8Bit(txt));
        }
    }
}

void Session::zmodemReceiveBlock(const char *data, int len)
{
    static int steps = 0;
    QByteArray bytes(data, len);

    _zmodemProc->write(bytes);

    // Provide some feedback to dialog
    if (steps > 100) {
        _zmodemProgress->addProgressText(QStringLiteral("."));
        steps = 0;
    }
    steps++;
}

void Session::zmodemFinished()
{
    /* zmodemFinished() is called by QProcess's finished() and
       ZModemDialog's user1Clicked(). Therefore, an invocation by
       user1Clicked() will recursively invoke this function again
       when the KProcess is deleted! */
    if (_zmodemProc != nullptr) {
        KProcess *process = _zmodemProc;
        _zmodemProc = nullptr; // Set _zmodemProc to 0 avoid recursive invocations!
        _zmodemBusy = false;
        delete process; // Now, the KProcess may be disposed safely.

        disconnect(_shellProcess, &Konsole::Pty::receivedData, this, &Konsole::Session::zmodemReceiveBlock);
        connect(_shellProcess, &Konsole::Pty::receivedData, this, &Konsole::Session::onReceiveBlock);

        _shellProcess->sendData(QByteArrayLiteral("\030\030\030\030")); // Abort
        _shellProcess->sendData(QByteArrayLiteral("\001\013\n")); // Try to get prompt back
        _zmodemProgress->transferDone();
    }
}

void Session::onReceiveBlock(const char *buf, int len)
{
    handleActivity();
    _emulation->receiveData(buf, len);
}

QSize Session::size()
{
    return _emulation->imageSize();
}

void Session::setSize(const QSize &size)
{
    if ((size.width() <= 1) || (size.height() <= 1)) {
        return;
    }

    Q_EMIT resizeRequest(size);
}

QSize Session::preferredSize() const
{
    return _preferredSize;
}

void Session::setPreferredSize(const QSize &size)
{
    _preferredSize = size;
}

int Session::processId() const
{
    return _shellProcess->processId();
}

void Session::setTitle(int role, const QString &title)
{
    switch (role) {
    case 0:
        setTitle(Session::NameRole, title);
        break;
    case 1:
        setTitle(Session::DisplayedTitleRole, title);

        // without these, that title will be overridden by the expansion of
        // title format shortly after, which will confuses users.
        _localTabTitleFormat = title;
        _remoteTabTitleFormat = title;

        break;
    }
}

QString Session::title(int role) const
{
    switch (role) {
    case 0:
        return title(Session::NameRole);
    case 1:
        return title(Session::DisplayedTitleRole);
    default:
        return QString();
    }
}

void Session::setTabTitleFormat(int context, const QString &format)
{
    switch (context) {
    case 0:
        setTabTitleFormat(Session::LocalTabTitle, format);
        break;
    case 1:
        setTabTitleFormat(Session::RemoteTabTitle, format);
        break;
    }
}

QString Session::tabTitleFormat(int context) const
{
    switch (context) {
    case 0:
        return tabTitleFormat(Session::LocalTabTitle);
    case 1:
        return tabTitleFormat(Session::RemoteTabTitle);
    default:
        return QString();
    }
}

void Session::setHistorySize(int lines)
{
    if (isReadOnly()) {
        return;
    }

    if (lines < 0) {
        setHistoryType(HistoryTypeFile());
    } else if (lines == 0) {
        setHistoryType(HistoryTypeNone());
    } else {
        setHistoryType(CompactHistoryType(lines));
    }
}

int Session::historySize() const
{
    const HistoryType &currentHistory = historyType();

    if (currentHistory.isEnabled()) {
        if (currentHistory.isUnlimited()) {
            return -1;
        } else {
            return currentHistory.maximumLineCount();
        }
    } else {
        return 0;
    }
}

QString Session::profile()
{
    return SessionManager::instance()->sessionProfile(this)->name();
}

void Session::setProfile(const QString &profileName)
{
    const QList<Profile::Ptr> profiles = ProfileManager::instance()->allProfiles();
    for (const Profile::Ptr &profile : profiles) {
        if (profile->name() == profileName) {
            SessionManager::instance()->setSessionProfile(this, profile);
        }
    }
}

int Session::foregroundProcessId()
{
    int pid;

    bool ok = false;
    pid = getProcessInfo()->pid(&ok);
    if (!ok) {
        pid = -1;
    }

    return pid;
}

bool Session::isForegroundProcessActive()
{
    const auto pid = processId();
    const auto fgid = _shellProcess->foregroundProcessGroup();

    // On FreeBSD, after exiting the shell, the foreground GID is
    // an invalid value, and the "shell" PID is 0. Those are not equal,
    // so the check below would return true.
    if (pid == 0) {
        return false;
    }

    // This check is wrong when Konsole is started with '-e cmd'
    // as there will only be one process.
    // See BKO 134581 for no popup when closing session
    return (pid != fgid);
}

QString Session::foregroundProcessName()
{
    QString name;

    if (updateForegroundProcessInfo()) {
        bool ok = false;
        name = _foregroundProcessInfo->name(&ok);
        if (!ok) {
            name.clear();
        }
    }

    return name;
}

void Session::saveSession(KConfigGroup &group)
{
    group.writePathEntry("WorkingDir", currentWorkingDirectory());
    group.writeEntry("LocalTab", tabTitleFormat(LocalTabTitle));
    group.writeEntry("RemoteTab", tabTitleFormat(RemoteTabTitle));
    group.writeEntry("TabColor", color().isValid() ? color().name(QColor::HexArgb) : QString());
    group.writeEntry("SessionGuid", _uniqueIdentifier.toString());
    group.writeEntry("Encoding", QString::fromUtf8(codec()));
}

void Session::restoreSession(KConfigGroup &group)
{
    QString value;

    value = group.readPathEntry("WorkingDir", QString());
    if (!value.isEmpty()) {
        setInitialWorkingDirectory(value);
    }
    value = group.readEntry("LocalTab");
    if (!value.isEmpty()) {
        setTabTitleFormat(LocalTabTitle, value);
    }
    value = group.readEntry("RemoteTab");
    if (!value.isEmpty()) {
        setTabTitleFormat(RemoteTabTitle, value);
    }
    value = group.readEntry("TabColor");
    if (!value.isEmpty()) {
        setColor(QColor(value));
    }
    value = group.readEntry("SessionGuid");
    if (!value.isEmpty()) {
        _uniqueIdentifier = QUuid(value);
    }
    value = group.readEntry("Encoding");
    if (!value.isEmpty()) {
        setCodec(value.toUtf8());
    }
}

QString Session::validDirectory(const QString &dir) const
{
    QString validDir = dir;
    if (validDir.isEmpty()) {
        validDir = QDir::currentPath();
    }

    const QFileInfo fi(validDir);
    if (!fi.exists() || !fi.isDir()) {
        validDir = QDir::homePath();
    }

    return validDir;
}

void Session::setPendingNotification(Session::Notification notification, bool enable)
{
    if (enable != _activeNotifications.testFlag(notification)) {
        _activeNotifications.setFlag(notification, enable);
        Q_EMIT notificationsChanged(notification, enable);
    }
}

void Session::handleActivity()
{
    // TODO: should this hardcoded interval be user configurable?
    const int activityMaskInSeconds = 15;

    TerminalDisplay *view = nullptr;
    if (!_views.isEmpty()) {
        view = _views.first();
    }

    if (_monitorActivity && !_notifiedActivity) {
        KNotification *notification = KNotification::event(hasFocus() ? QStringLiteral("Activity") : QStringLiteral("ActivityHidden"),
                                                           i18n("Activity in '%1' (Session '%2')", _displayTitle, _nameTitle),
                                                           QPixmap(),
                                                           view,
                                                           KNotification::CloseWhenWidgetActivated);
        notification->setDefaultAction(i18n("Show session"));
        connect(notification, &KNotification::defaultActivated, this, [view, notification]() {
            view->notificationClicked(notification->xdgActivationToken());
        });

        // mask activity notification for a while to avoid flooding
        _notifiedActivity = true;
        _activityTimer->start(activityMaskInSeconds * 1000);
    }

    // reset the counter for monitoring continuous silence since there is activity
    if (_monitorSilence) {
        _silenceTimer->start(_silenceSeconds * 1000);
    }

    if (_monitorActivity) {
        setPendingNotification(Notification::Activity);
    }
}

bool Session::isReadOnly() const
{
    return _readOnly;
}

void Session::setReadOnly(bool readOnly)
{
    if (_readOnly != readOnly) {
        _readOnly = readOnly;

        // Needed to update the tab icons and all
        // attached views.
        Q_EMIT readOnlyChanged();
    }
}

void Session::setColor(const QColor &color)
{
    _tabColor = color;
    Q_EMIT sessionAttributeChanged();
}

QColor Session::color() const
{
    return _tabColor;
}

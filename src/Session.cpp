/*
    This file is part of Konsole

    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 1997,1998 by Lars Doelle <lars.doelle@on-line.de>
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
#include "Session.h"

// Standard
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

// Qt
#include <QApplication>
#include <QtGui/QColor>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QStringList>
#include <QtDBus/QtDBus>

// KDE
#include <KDebug>
#include <KLocalizedString>
#include <KNotification>
#include <KRun>
#include <KShell>
#include <KProcess>
#include <KStandardDirs>
#include <KConfigGroup>

// Konsole
#include <sessionadaptor.h>

#include "ProcessInfo.h"
#include "Pty.h"
#include "TerminalDisplay.h"
#include "ShellCommand.h"
#include "Vt102Emulation.h"
#include "ZModemDialog.h"
#include "History.h"

using namespace Konsole;

int Session::lastSessionId = 0;

// HACK This is copied out of QUuid::createUuid with reseeding forced.
// Required because color schemes repeatedly seed the RNG...
// ...with a constant.
QUuid createUuid()
{
    static const int intbits = sizeof(int) * 8;
    static int randbits = 0;
    if (!randbits) {
        int max = RAND_MAX;
        do {
            ++randbits;
        } while ((max = max >> 1));
    }

    qsrand(uint(QDateTime::currentDateTime().toTime_t()));
    qrand(); // Skip first

    QUuid result;
    uint* data = &(result.data1);
    int chunks = 16 / sizeof(uint);
    while (chunks--) {
        uint randNumber = 0;
        for (int filled = 0; filled < intbits; filled += randbits)
            randNumber |= qrand() << filled;
        *(data + chunks) = randNumber;
    }

    result.data4[0] = (result.data4[0] & 0x3F) | 0x80;        // UV_DCE
    result.data3 = (result.data3 & 0x0FFF) | 0x4000;        // UV_Random

    return result;
}

Session::Session(QObject* parent) :
    QObject(parent)
    , _shellProcess(0)
    , _emulation(0)
    , _monitorActivity(false)
    , _monitorSilence(false)
    , _notifiedActivity(false)
    , _silenceSeconds(10)
    , _autoClose(true)
    , _closePerUserRequest(false)
    , _addToUtmp(true)
    , _flowControlEnabled(true)
    , _sessionId(0)
    , _sessionProcessInfo(0)
    , _foregroundProcessInfo(0)
    , _foregroundPid(0)
    , _zmodemBusy(false)
    , _zmodemProc(0)
    , _zmodemProgress(0)
    , _hasDarkBackground(false)
{
    _uniqueIdentifier = createUuid();

    //prepare DBus communication
    new SessionAdaptor(this);
    _sessionId = ++lastSessionId;
    QDBusConnection::sessionBus().registerObject(QLatin1String("/Sessions/") + QString::number(_sessionId), this);

    //create emulation backend
    _emulation = new Vt102Emulation();

    connect(_emulation, SIGNAL(titleChanged(int,QString)),
            this, SLOT(setUserTitle(int,QString)));
    connect(_emulation, SIGNAL(stateSet(int)),
            this, SLOT(activityStateSet(int)));
    connect(_emulation, SIGNAL(zmodemDetected()),
            this, SLOT(fireZModemDetected()));
    connect(_emulation, SIGNAL(changeTabTextColorRequest(int)),
            this, SIGNAL(changeTabTextColorRequest(int)));
    connect(_emulation, SIGNAL(profileChangeCommandReceived(QString)),
            this, SIGNAL(profileChangeCommandReceived(QString)));
    connect(_emulation, SIGNAL(flowControlKeyPressed(bool)),
            this, SLOT(updateFlowControlState(bool)));
    connect(_emulation, SIGNAL(primaryScreenInUse(bool)),
            this, SLOT(onPrimaryScreenInUse(bool)));
    connect(_emulation, SIGNAL(selectionChanged(QString)),
            this, SIGNAL(selectionChanged(QString)));
    connect(_emulation, SIGNAL(imageResizeRequest(QSize)),
            this, SIGNAL(resizeRequest(QSize)));

    //create new teletype for I/O with shell process
    openTeletype(-1);

    //setup timer for monitoring session activity & silence
    _silenceTimer = new QTimer(this);
    _silenceTimer->setSingleShot(true);
    connect(_silenceTimer, SIGNAL(timeout()), this, SLOT(silenceTimerDone()));

    _activityTimer = new QTimer(this);
    _activityTimer->setSingleShot(true);
    connect(_activityTimer, SIGNAL(timeout()), this, SLOT(activityTimerDone()));
}

Session::~Session()
{
    delete _foregroundProcessInfo;
    delete _sessionProcessInfo;
    delete _emulation;
    delete _shellProcess;
    delete _zmodemProc;
}

void Session::openTeletype(int fd)
{
    if (isRunning()) {
        kWarning() << "Attempted to open teletype in a running session.";
        return;
    }

    delete _shellProcess;

    if (fd < 0)
        _shellProcess = new Pty();
    else
        _shellProcess = new Pty(fd);

    _shellProcess->setUtf8Mode(_emulation->utf8());

    // connect the I/O between emulator and pty process
    connect(_shellProcess, SIGNAL(receivedData(const char*,int)),
            this, SLOT(onReceiveBlock(const char*,int)));
    connect(_emulation, SIGNAL(sendData(const char*,int)),
            _shellProcess, SLOT(sendData(const char*,int)));

    // UTF8 mode
    connect(_emulation, SIGNAL(useUtf8Request(bool)),
            _shellProcess, SLOT(setUtf8Mode(bool)));

    // get notified when the pty process is finished
    connect(_shellProcess, SIGNAL(finished(int,QProcess::ExitStatus)),
            this, SLOT(done(int,QProcess::ExitStatus)));

    // emulator size
    connect(_emulation, SIGNAL(imageSizeChanged(int,int)),
            this, SLOT(updateWindowSize(int,int)));
    connect(_emulation, SIGNAL(imageSizeInitialized()),
            this, SLOT(run()));
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
        QWidget* window = _views.first();

        Q_ASSERT(window);

        while (window->parentWidget() != 0)
            window = window->parentWidget();

        return window->winId();
    }
}

void Session::setDarkBackground(bool darkBackground)
{
    _hasDarkBackground = darkBackground;
}

bool Session::isRunning() const
{
    return _shellProcess && (_shellProcess->state() == QProcess::Running);
}

void Session::setCodec(QTextCodec* codec)
{
    emulation()->setCodec(codec);
}

bool Session::setCodec(QByteArray name)
{
    QTextCodec* codec = QTextCodec::codecForName(name);

    if (codec) {
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

void Session::setProgram(const QString& program)
{
    _program = ShellCommand::expand(program);
}

void Session::setArguments(const QStringList& arguments)
{
    _arguments = ShellCommand::expand(arguments);
}

void Session::setInitialWorkingDirectory(const QString& dir)
{
    _initialWorkingDir = KShell::tildeExpand(ShellCommand::expand(dir));
}

QString Session::currentWorkingDirectory()
{
    // only returned cached value
    if (_currentWorkingDir.isEmpty())
        updateWorkingDirectory();

    return _currentWorkingDir;
}
ProcessInfo* Session::updateWorkingDirectory()
{
    ProcessInfo* process = getProcessInfo();

    const QString currentDir = process->validCurrentDir();
    if (currentDir != _currentWorkingDir) {
        _currentWorkingDir = currentDir;
        emit currentDirectoryChanged(_currentWorkingDir);
    }

    return process;
}

QList<TerminalDisplay*> Session::views() const
{
    return _views;
}

void Session::addView(TerminalDisplay* widget)
{
    Q_ASSERT(!_views.contains(widget));

    _views.append(widget);

    // connect emulation - view signals and slots
    connect(widget, SIGNAL(keyPressedSignal(QKeyEvent*)),
            _emulation, SLOT(sendKeyEvent(QKeyEvent*)));
    connect(widget, SIGNAL(mouseSignal(int,int,int,int)),
            _emulation, SLOT(sendMouseEvent(int,int,int,int)));
    connect(widget, SIGNAL(sendStringToEmu(const char*)),
            _emulation, SLOT(sendString(const char*)));

    // allow emulation to notify view when the foreground process
    // indicates whether or not it is interested in mouse signals
    connect(_emulation, SIGNAL(programUsesMouseChanged(bool)),
            widget, SLOT(setUsesMouse(bool)));

    widget->setUsesMouse(_emulation->programUsesMouse());

    widget->setScreenWindow(_emulation->createWindow());

    //connect view signals and slots
    connect(widget, SIGNAL(changedContentSizeSignal(int,int)),
            this, SLOT(onViewSizeChange(int,int)));

    connect(widget, SIGNAL(destroyed(QObject*)),
            this, SLOT(viewDestroyed(QObject*)));
}

void Session::viewDestroyed(QObject* view)
{
    // the received QObject has already been destroyed, so using
    // qobject_cast<> does not work here
    TerminalDisplay* display = static_cast<TerminalDisplay*>(view);

    Q_ASSERT(_views.contains(display));

    removeView(display);
}

void Session::removeView(TerminalDisplay* widget)
{
    _views.removeAll(widget);

    disconnect(widget, 0, this, 0);

    // disconnect
    //  - key presses signals from widget
    //  - mouse activity signals from widget
    //  - string sending signals from widget
    //
    //  ... and any other signals connected in addView()
    disconnect(widget, 0, _emulation, 0);

    // disconnect state change signals emitted by emulation
    disconnect(_emulation, 0, widget, 0);

    // close the session automatically when the last view is removed
    if (_views.count() == 0) {
        close();
    }
}

// Upon a KPty error, there is no description on what that error was...
// Check to see if the given program is executable.
QString Session::checkProgram(const QString& program)
{
    QString exec = program;

    if (exec.isEmpty())
        return QString();

    QFileInfo info(exec);
    if (info.isAbsolute() && info.exists() && info.isExecutable()) {
        return exec;
    }

    exec = KRun::binaryName(exec, false);
    exec = KShell::tildeExpand(exec);
    QString pexec = KStandardDirs::findExe(exec);
    if (pexec.isEmpty()) {
        kError() << i18n("Could not find binary: ") << exec;
        return QString();
    }

    return exec;
}

void Session::terminalWarning(const QString& message)
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
    friendlyUuid.remove('-').remove('{').remove('}');

    return friendlyUuid;
}

void Session::run()
{
    // extra safeguard for potential bug.
    if (isRunning()) {
        kWarning() << "Attempted to re-run an already running session.";
        return;
    }

    //check that everything is in place to run the session
    if (_program.isEmpty()) {
        kWarning() << "Program to run not set.";
    }
    if (_arguments.isEmpty()) {
        kWarning() << "No command line arguments specified.";
    }
    if (_uniqueIdentifier.isNull()) {
        _uniqueIdentifier = createUuid();
    }

    const int CHOICE_COUNT = 3;
    // if '_program' is empty , fall back to default shell. If that is not set
    // then fall back to /bin/sh
    QString programs[CHOICE_COUNT] = {_program, qgetenv("SHELL"), "/bin/sh"};
    QString exec;
    int choice = 0;
    while (choice < CHOICE_COUNT) {
        exec = checkProgram(programs[choice]);
        if (exec.isEmpty())
            choice++;
        else
            break;
    }

    // if a program was specified via setProgram(), but it couldn't be found, print a warning
    if (choice != 0 && choice < CHOICE_COUNT && !_program.isEmpty()) {
        terminalWarning(i18n("Could not find '%1', starting '%2' instead.  Please check your profile settings.", _program, exec));
        // if none of the choices are available, print a warning
    } else if (choice == CHOICE_COUNT) {
        terminalWarning(i18n("Could not find an interactive shell to start."));
        return;
    }

    // if no arguments are specified, fall back to program name
    QStringList arguments = _arguments.join(QChar(' ')).isEmpty() ?
                            QStringList() << exec :
                            _arguments;

    if (!_initialWorkingDir.isEmpty()) {
        _shellProcess->setInitialWorkingDirectory(_initialWorkingDir);
    } else {
        _shellProcess->setInitialWorkingDirectory(QDir::currentPath());
    }

    _shellProcess->setFlowControlEnabled(_flowControlEnabled);
    _shellProcess->setEraseChar(_emulation->eraseChar());
    _shellProcess->setUseUtmp(_addToUtmp);

    // this is not strictly accurate use of the COLORFGBG variable.  This does not
    // tell the terminal exactly which colors are being used, but instead approximates
    // the color scheme as "black on white" or "white on black" depending on whether
    // the background color is deemed dark or not
    const QString backgroundColorHint = _hasDarkBackground ? "COLORFGBG=15;0" : "COLORFGBG=0;15";
    addEnvironmentEntry(backgroundColorHint);

    addEnvironmentEntry(QString("SHELL_SESSION_ID=%1").arg(shellSessionId()));

    addEnvironmentEntry(QString("WINDOWID=%1").arg(QString::number(windowId())));

    const QString dbusService = QDBusConnection::sessionBus().baseService();
    addEnvironmentEntry(QString("KONSOLE_DBUS_SERVICE=%1").arg(dbusService));

    const QString dbusObject = QString("/Sessions/%1").arg(QString::number(_sessionId));
    addEnvironmentEntry(QString("KONSOLE_DBUS_SESSION=%1").arg(dbusObject));

    int result = _shellProcess->start(exec, arguments, _environment);
    if (result < 0) {
        terminalWarning(i18n("Could not start program '%1' with arguments '%2'.", exec, arguments.join(" ")));
        return;
    }

    _shellProcess->setWriteable(false);  // We are reachable via kwrited.

    emit started();
}

void Session::setUserTitle(int what, const QString& caption)
{
    //set to true if anything is actually changed (eg. old _nameTitle != new _nameTitle )
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
        QString colorString = caption.section(';', 0, 0);
        QColor color = QColor(colorString);
        if (color.isValid()) {
            if (what == TextColor)
                emit changeForegroundColorRequest(color);
            else
                emit changeBackgroundColorRequest(color);
        }
    }

    if (what == SessionName) {
        if (_localTabTitleFormat != caption) {
            _localTabTitleFormat = caption;
            setTitle(Session::DisplayedTitleRole, caption);
            modified = true;
        }
    }

    /* I don't belive this has ever worked in KDE 4.x
    if (what == 31) {
        QString cwd = caption;
        cwd = cwd.replace(QRegExp("^~"), QDir::homePath());
        emit openUrlRequest(cwd);
    }*/

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

    if (what == ProfileChange) {
        emit profileChangeCommandReceived(caption);
        return;
    }

    if (modified)
        emit titleChanged();
}

QString Session::userTitle() const
{
    return _userTitle;
}
void Session::setTabTitleFormat(TabTitleContext context , const QString& format)
{
    if (context == LocalTabTitle)
        _localTabTitleFormat = format;
    else if (context == RemoteTabTitle)
        _remoteTabTitleFormat = format;
}
QString Session::tabTitleFormat(TabTitleContext context) const
{
    if (context == LocalTabTitle)
        return _localTabTitleFormat;
    else if (context == RemoteTabTitle)
        return _remoteTabTitleFormat;

    return QString();
}

void Session::silenceTimerDone()
{
    //FIXME: The idea here is that the notification popup will appear to tell the user than output from
    //the terminal has stopped and the popup will disappear when the user activates the session.
    //
    //This breaks with the addition of multiple views of a session.  The popup should disappear
    //when any of the views of the session becomes active

    //FIXME: Make message text for this notification and the activity notification more descriptive.
    if (_monitorSilence) {
        KNotification::event("Silence", i18n("Silence in session '%1'", _nameTitle), QPixmap(),
                             QApplication::activeWindow(),
                             KNotification::CloseWhenWidgetActivated);
        emit stateChanged(NOTIFYSILENCE);
    } else {
        emit stateChanged(NOTIFYNORMAL);
    }
}

void Session::activityTimerDone()
{
    _notifiedActivity = false;
}

void Session::updateFlowControlState(bool suspended)
{
    if (suspended) {
        if (flowControlEnabled()) {
            foreach(TerminalDisplay * display, _views) {
                if (display->flowControlWarningEnabled())
                    display->outputSuspended(true);
            }
        }
    } else {
        foreach(TerminalDisplay * display, _views) {
            display->outputSuspended(false);
        }
    }
}

void Session::onPrimaryScreenInUse(bool use)
{
    emit primaryScreenInUse(use);
}

void Session::activityStateSet(int state)
{
    // TODO: should this hardcoded interval be user configurable?
    const int activityMaskInSeconds = 15;

    if (state == NOTIFYBELL) {
        emit bellRequest(i18n("Bell in session '%1'", _nameTitle));
    } else if (state == NOTIFYACTIVITY) {
        if (_monitorActivity  && !_notifiedActivity) {
            KNotification::event("Activity", i18n("Activity in session '%1'", _nameTitle), QPixmap(),
                                 QApplication::activeWindow(),
                                 KNotification::CloseWhenWidgetActivated);

            // mask activity notification for a while to avoid flooding
            _notifiedActivity = true;
            _activityTimer->start(activityMaskInSeconds * 1000);
        }

        // reset the counter for monitoring continuous silence since ther is activity
        if (_monitorSilence) {
            _silenceTimer->start(_silenceSeconds * 1000);
        }
    }

    if (state == NOTIFYACTIVITY && !_monitorActivity)
        state = NOTIFYNORMAL;
    if (state == NOTIFYSILENCE && !_monitorSilence)
        state = NOTIFYNORMAL;

    emit stateChanged(state);
}

void Session::onViewSizeChange(int /*height*/, int /*width*/)
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

    //select largest number of lines and columns that will fit in all visible views
    foreach(TerminalDisplay* view, _views) {
        if (view->isHidden() == false &&
                view->lines() >= VIEW_LINES_THRESHOLD &&
                view->columns() >= VIEW_COLUMNS_THRESHOLD) {
            minLines = (minLines == -1) ? view->lines() : qMin(minLines , view->lines());
            minColumns = (minColumns == -1) ? view->columns() : qMin(minColumns , view->columns());
            view->processFilters();
        }
    }

    // backend emulation must have a _terminal of at least 1 column x 1 line in size
    if (minLines > 0 && minColumns > 0) {
        _emulation->setImageSize(minLines , minColumns);
    }
}
void Session::updateWindowSize(int lines, int columns)
{
    Q_ASSERT(lines > 0 && columns > 0);
    _shellProcess->setWindowSize(columns, lines);
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
    _shellProcess->setWindowSize(existingSize.width() + 1, existingSize.height());
    usleep(500); // introduce small delay to avoid changing size too quickly
    _shellProcess->setWindowSize(existingSize.width(), existingSize.height());
}

void Session::sendSignal(int signal)
{
    const ProcessInfo* process = getProcessInfo();
    bool ok = false;
    int pid;
    pid = process->foregroundPid(&ok);

    if (ok) {
        ::kill(pid, signal);
    }
}

bool Session::kill(int signal)
{
    if (_shellProcess->pid() <= 0)
        return false;

    int result = ::kill(_shellProcess->pid(), signal);

    if (result == 0) {
        if (_shellProcess->waitForFinished(1000))
            return true;
        else
            return false;
    } else
        return false;
}

void Session::close()
{
    if (isRunning()) {
        if (!closeInNormalWay())
            closeInForceWay();
    } else {
        // terminal process has finished, just close the session
        QTimer::singleShot(1, this, SIGNAL(finished()));
    }
}

bool Session::closeInNormalWay()
{
    _autoClose    = true;
    _closePerUserRequest = true;

    // for the possible case where following events happen in sequence:
    //
    // 1). the terminal process crashes
    // 2). the tab stays open and displays warning message
    // 3). the user closes the tab explicitly
    //
    if (!isRunning()) {
        emit finished();
        return true;
    }

    if (kill(SIGHUP)) {
        return true;
    } else {
        kWarning() << "Process " << _shellProcess->pid() << " did not die with SIGHUP";
        _shellProcess->closePty();
        return (_shellProcess->waitForFinished(1000));
    }
}

bool Session::closeInForceWay()
{
    _autoClose    = true;
    _closePerUserRequest = true;

    if (kill(SIGKILL)) {
        return true;
    } else {
        kWarning() << "Process " << _shellProcess->pid() << " did not die with SIGKILL";
        return false;
    }
}

void Session::sendText(const QString& text) const
{
    _emulation->sendText(text);
}

void Session::runCommand(const QString& command) const
{
    _emulation->sendText(command + '\n');
}

void Session::sendMouseEvent(int buttons, int column, int line, int eventType)
{
    _emulation->sendMouseEvent(buttons, column, line, eventType);
}

void Session::done(int exitCode, QProcess::ExitStatus exitStatus)
{
    // This slot should be triggered only one time
    disconnect(_shellProcess, SIGNAL(finished(int,QProcess::ExitStatus)),
               this, SLOT(done(int,QProcess::ExitStatus)));

    if (!_autoClose) {
        _userTitle = i18nc("@info:shell This session is done", "Finished");
        emit titleChanged();
        return;
    }

    if (_closePerUserRequest) {
        emit finished();
        return;
    }

    QString message;

    if (exitCode != 0) {
        if (exitStatus != QProcess::NormalExit)
            message = i18n("Program '%1' crashed.", _program);
        else
            message = i18n("Program '%1' exited with status %2.", _program, exitCode);

        //FIXME: See comments in Session::silenceTimerDone()
        KNotification::event("Finished", message , QPixmap(),
                             QApplication::activeWindow(),
                             KNotification::CloseWhenWidgetActivated);
    }

    if (exitStatus != QProcess::NormalExit) {
        // this seeming duplicated line is for the case when exitCode is 0
        message = i18n("Program '%1' crashed.", _program);
        terminalWarning(message);
    } else {
        emit finished();
    }
}

Emulation* Session::emulation() const
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

void Session::setEnvironment(const QStringList& environment)
{
    _environment = environment;
}

void Session::addEnvironmentEntry(const QString& entry)
{
    _environment << entry;
}

int Session::sessionId() const
{
    return _sessionId;
}

void Session::setKeyBindings(const QString& name)
{
    _emulation->setKeyBindings(name);
}

void Session::setTitle(TitleRole role , const QString& newTitle)
{
    if (title(role) != newTitle) {
        if (role == NameRole)
            _nameTitle = newTitle;
        else if (role == DisplayedTitleRole)
            _displayTitle = newTitle;

        emit titleChanged();
    }
}

QString Session::title(TitleRole role) const
{
    if (role == NameRole)
        return _nameTitle;
    else if (role == DisplayedTitleRole)
        return _displayTitle;
    else
        return QString();
}

ProcessInfo* Session::getProcessInfo()
{
    ProcessInfo* process = 0;

    if (isForegroundProcessActive()) {
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
    if (!_sessionProcessInfo ||
            (processId() != 0 && processId() != _sessionProcessInfo->pid(&ok))) {
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

    if (_foregroundProcessInfo) {
        _foregroundProcessInfo->update();
        return _foregroundProcessInfo->isValid();
    } else {
        return false;
    }
}

bool Session::isRemote()
{
    ProcessInfo* process = getProcessInfo();

    bool ok = false;
    return (process->name(&ok) == "ssh" && ok);
}

QString Session::getDynamicTitle()
{
    // update current directory from process
    ProcessInfo* process = updateWorkingDirectory();

    // format tab titles using process info
    bool ok = false;
    QString title;
    if (process->name(&ok) == "ssh" && ok) {
        SSHProcessInfo sshInfo(*process);
        title = sshInfo.format(tabTitleFormat(Session::RemoteTabTitle));
    } else {
        title = process->format(tabTitleFormat(Session::LocalTabTitle));
    }

    return title;
}

KUrl Session::getUrl()
{
    QString path;

    updateSessionProcessInfo();
    if (_sessionProcessInfo->isValid()) {
        bool ok = false;

        // check if foreground process is bookmark-able
        if (isForegroundProcessActive()) {
            // for remote connections, save the user and host
            // bright ideas to get the directory at the other end are welcome :)
            if (_foregroundProcessInfo->name(&ok) == "ssh" && ok) {
                SSHProcessInfo sshInfo(*_foregroundProcessInfo);

                path = "ssh://" + sshInfo.userName() + '@' + sshInfo.host();

                QString port = sshInfo.port();
                if (!port.isEmpty() && port != "22") {
                    path.append(':' + port);
                }
            } else {
                path = _foregroundProcessInfo->currentDir(&ok);
                if (!ok)
                    path.clear();
            }
        } else { // otherwise use the current working directory of the shell process
            path = _sessionProcessInfo->currentDir(&ok);
            if (!ok)
                path.clear();
        }
    }

    return KUrl(path);
}

void Session::setIconName(const QString& iconName)
{
    if (iconName != _iconName) {
        _iconName = iconName;
        emit titleChanged();
    }
}

void Session::setIconText(const QString& iconText)
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

void Session::setHistoryType(const HistoryType& hType)
{
    _emulation->setHistory(hType);
}

const HistoryType& Session::historyType() const
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
bool Session::isMonitorSilence()  const
{
    return _monitorSilence;
}

void Session::setMonitorActivity(bool monitor)
{
    if (_monitorActivity == monitor)
        return;

    _monitorActivity  = monitor;
    _notifiedActivity = false;

    // This timer is meaningful only after activity has been notified
    _activityTimer->stop();

    activityStateSet(NOTIFYNORMAL);
}

void Session::setMonitorSilence(bool monitor)
{
    if (_monitorSilence == monitor)
        return;

    _monitorSilence = monitor;
    if (_monitorSilence) {
        _silenceTimer->start(_silenceSeconds * 1000);
    } else {
        _silenceTimer->stop();
    }

    activityStateSet(NOTIFYNORMAL);
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
    _flowControlEnabled = enabled;

    if (_shellProcess)
        _shellProcess->setFlowControlEnabled(_flowControlEnabled);

    emit flowControlEnabledChanged(enabled);
}
bool Session::flowControlEnabled() const
{
    if (_shellProcess)
        return _shellProcess->flowControlEnabled();
    else
        return _flowControlEnabled;
}
void Session::fireZModemDetected()
{
    if (!_zmodemBusy) {
        QTimer::singleShot(10, this, SIGNAL(zmodemDetected()));
        _zmodemBusy = true;
    }
}

void Session::cancelZModem()
{
    _shellProcess->sendData("\030\030\030\030", 4); // Abort
    _zmodemBusy = false;
}

void Session::startZModem(const QString& zmodem, const QString& dir, const QStringList& list)
{
    _zmodemBusy = true;
    _zmodemProc = new KProcess();
    _zmodemProc->setOutputChannelMode(KProcess::SeparateChannels);

    *_zmodemProc << zmodem << "-v" << list;

    if (!dir.isEmpty())
        _zmodemProc->setWorkingDirectory(dir);

    connect(_zmodemProc, SIGNAL(readyReadStandardOutput()),
            this, SLOT(zmodemReadAndSendBlock()));
    connect(_zmodemProc, SIGNAL(readyReadStandardError()),
            this, SLOT(zmodemReadStatus()));
    connect(_zmodemProc, SIGNAL(finished(int,QProcess::ExitStatus)),
            this, SLOT(zmodemFinished()));

    _zmodemProc->start();

    disconnect(_shellProcess, SIGNAL(receivedData(const char*,int)),
               this, SLOT(onReceiveBlock(const char*,int)));
    connect(_shellProcess, SIGNAL(receivedData(const char*,int)),
            this, SLOT(zmodemReceiveBlock(const char*,int)));

    _zmodemProgress = new ZModemDialog(QApplication::activeWindow(), false,
                                       i18n("ZModem Progress"));

    connect(_zmodemProgress, SIGNAL(user1Clicked()),
            this, SLOT(zmodemFinished()));

    _zmodemProgress->show();
}

void Session::zmodemReadAndSendBlock()
{
    _zmodemProc->setReadChannel(QProcess::StandardOutput);
    QByteArray data = _zmodemProc->readAll();

    if (data.count() == 0)
        return;

    _shellProcess->sendData(data.constData(), data.count());
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
        if (!txt.isEmpty())
            _zmodemProgress->addProgressText(QString::fromLocal8Bit(txt));
    }
}

void Session::zmodemReceiveBlock(const char* data, int len)
{
    QByteArray bytes(data, len);

    _zmodemProc->write(bytes);
}

void Session::zmodemFinished()
{
    /* zmodemFinished() is called by QProcess's finished() and
       ZModemDialog's user1Clicked(). Therefore, an invocation by
       user1Clicked() will recursively invoke this function again
       when the KProcess is deleted! */
    if (_zmodemProc) {
        KProcess* process = _zmodemProc;
        _zmodemProc = 0;   // Set _zmodemProc to 0 avoid recursive invocations!
        _zmodemBusy = false;
        delete process;    // Now, the KProcess may be disposed safely.

        disconnect(_shellProcess, SIGNAL(receivedData(const char*,int)),
                   this , SLOT(zmodemReceiveBlock(const char*,int)));
        connect(_shellProcess, SIGNAL(receivedData(const char*,int)),
                this, SLOT(onReceiveBlock(const char*,int)));

        _shellProcess->sendData("\030\030\030\030", 4); // Abort
        _shellProcess->sendData("\001\013\n", 3); // Try to get prompt back
        _zmodemProgress->transferDone();
    }
}

void Session::onReceiveBlock(const char* buf, int len)
{
    _emulation->receiveData(buf, len);
}

QSize Session::size()
{
    return _emulation->imageSize();
}

void Session::setSize(const QSize& size)
{
    if ((size.width() <= 1) || (size.height() <= 1))
        return;

    emit resizeRequest(size);
}

QSize Session::preferredSize() const
{
    return _preferredSize;
}

void Session::setPreferredSize(const QSize& size)
{
    _preferredSize = size;
}

int Session::processId() const
{
    return _shellProcess->pid();
}

void Session::setTitle(int role , const QString& title)
{
    switch (role) {
    case 0:
        this->setTitle(Session::NameRole, title);
        break;
    case 1:
        this->setTitle(Session::DisplayedTitleRole, title);

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
        return this->title(Session::NameRole);
    case 1:
        return this->title(Session::DisplayedTitleRole);
    default:
        return QString();
    }
}

void Session::setTabTitleFormat(int context , const QString& format)
{
    switch (context) {
    case 0:
        this->setTabTitleFormat(Session::LocalTabTitle, format);
        break;
    case 1:
        this->setTabTitleFormat(Session::RemoteTabTitle, format);
        break;
    }
}

QString Session::tabTitleFormat(int context) const
{
    switch (context) {
    case 0:
        return this->tabTitleFormat(Session::LocalTabTitle);
    case 1:
        return this->tabTitleFormat(Session::RemoteTabTitle);
    default:
        return QString();
    }
}

void Session::setHistorySize(int lines)
{
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
    const HistoryType& currentHistory = historyType();

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

int Session::foregroundProcessId()
{
    int pid;

    bool ok = false;
    pid = getProcessInfo()->pid(&ok);
    if (!ok)
        pid = -1;

    return pid;
}

bool Session::isForegroundProcessActive()
{
    // foreground process info is always updated after this
    return updateForegroundProcessInfo() && (processId() != _foregroundPid);
}

QString Session::foregroundProcessName()
{
    QString name;

    if (updateForegroundProcessInfo()) {
        bool ok = false;
        name = _foregroundProcessInfo->name(&ok);
        if (!ok)
            name.clear();
    }

    return name;
}

void Session::saveSession(KConfigGroup& group)
{
    group.writePathEntry("WorkingDir", currentWorkingDirectory());
    group.writeEntry("LocalTab",       tabTitleFormat(LocalTabTitle));
    group.writeEntry("RemoteTab",      tabTitleFormat(RemoteTabTitle));
    group.writeEntry("SessionGuid",    _uniqueIdentifier.toString());
    group.writeEntry("Encoding",       QString(codec()));
}

void Session::restoreSession(KConfigGroup& group)
{
    QString value;

    value = group.readPathEntry("WorkingDir", QString());
    if (!value.isEmpty()) setInitialWorkingDirectory(value);
    value = group.readEntry("LocalTab");
    if (!value.isEmpty()) setTabTitleFormat(LocalTabTitle, value);
    value = group.readEntry("RemoteTab");
    if (!value.isEmpty()) setTabTitleFormat(RemoteTabTitle, value);
    value = group.readEntry("SessionGuid");
    if (!value.isEmpty()) _uniqueIdentifier = QUuid(value);
    value = group.readEntry("Encoding");
    if (!value.isEmpty()) setCodec(value.toUtf8());
}

SessionGroup::SessionGroup(QObject* parent)
    : QObject(parent), _masterMode(0)
{
}
SessionGroup::~SessionGroup()
{
}
int SessionGroup::masterMode() const
{
    return _masterMode;
}
QList<Session*> SessionGroup::sessions() const
{
    return _sessions.keys();
}
bool SessionGroup::masterStatus(Session* session) const
{
    return _sessions[session];
}

void SessionGroup::addSession(Session* session)
{
    connect(session, SIGNAL(finished()), this, SLOT(sessionFinished()));
    _sessions.insert(session, false);
}
void SessionGroup::removeSession(Session* session)
{
    disconnect(session, SIGNAL(finished()), this, SLOT(sessionFinished()));
    setMasterStatus(session, false);
    _sessions.remove(session);
}
void SessionGroup::sessionFinished()
{
    Session* session = qobject_cast<Session*>(sender());
    Q_ASSERT(session);
    removeSession(session);
}
void SessionGroup::setMasterMode(int mode)
{
    _masterMode = mode;
}
QList<Session*> SessionGroup::masters() const
{
    return _sessions.keys(true);
}
void SessionGroup::setMasterStatus(Session* session , bool master)
{
    const bool wasMaster = _sessions[session];

    if (wasMaster == master) {
        // No status change -> nothing to do.
        return;
    }
    _sessions[session] = master;

    if (master) {
        connect(session->emulation(), SIGNAL(sendData(const char*,int)),
                this, SLOT(forwardData(const char*,int)));
    } else {
        disconnect(session->emulation(), SIGNAL(sendData(const char*,int)),
                   this, SLOT(forwardData(const char*,int)));
    }
}
void SessionGroup::forwardData(const char* data, int size)
{
    static bool _inForwardData = false;
    if (_inForwardData) {  // Avoid recursive calls among session groups!
        // A recursive call happens when a master in group A calls forwardData()
        // in group B. If one of the destination sessions in group B is also a
        // master of a group including the master session of group A, this would
        // again call forwardData() in group A, and so on.
        return;
    }

    _inForwardData = true;
    foreach(Session* other, _sessions.keys()) {
        if (!_sessions[other]) {
            other->emulation()->sendString(data, size);
        }
    }
    _inForwardData = false;
}

#include "Session.moc"


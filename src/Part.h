/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PART_H
#define PART_H

// KDE
#include <KParts/ReadOnlyPart>
#include <kde_terminal_interface.h>

// Qt
#include <QVariantList>

// Konsole
#include "config-konsole.h"
#include "session/Session.h"

class QStringList;
class QKeyEvent;

namespace Konsole
{
class Session;
class SessionController;
class ViewManager;
class ViewProperties;

/**
 * A re-usable terminal emulator component using the KParts framework which can
 * be used to embed terminal emulators into other applications.
 */
#ifdef USE_TERMINALINTERFACEV2
class Part : public KParts::ReadOnlyPart, public TerminalInterfaceV2
{
    Q_OBJECT
    Q_INTERFACES(TerminalInterface TerminalInterfaceV2)
#else
class TerminalInterfaceV2;
class Part : public KParts::ReadOnlyPart, public TerminalInterface
{
    Q_OBJECT
    Q_INTERFACES(TerminalInterface)
#endif
public:
    /** Constructs a new Konsole part with the specified parent. */
    explicit Part(QWidget *parentWidget, QObject *parent, const QVariantList &);
    ~Part() override;

    /** Reimplemented from TerminalInterface. */
    void startProgram(const QString &program, const QStringList &arguments) override;
    /** Reimplemented from TerminalInterface. */
    void showShellInDir(const QString &dir) override;
    /** Reimplemented from TerminalInterface. */
    void sendInput(const QString &text) override;

    /** Reimplemented from TerminalInterface. */
    int terminalProcessId() override;

    /** Reimplemented from TerminalInterface. */
    int foregroundProcessId() override;

    /** Reimplemented from TerminalInterface. */
    QString foregroundProcessName() override;

    /** Reimplemented from TerminalInterface. */
    QString currentWorkingDirectory() const override;

#ifdef USE_TERMINALINTERFACEV2
    /** Reimplemented from TerminalInterfaceV2 */
    QStringList availableProfiles() const override;

    /** Reimplemented from TerminalInterfaceV2 */
    QString currentProfileName() const override;

    /** Reimplemented from TerminalInterfaceV2 */
    bool setCurrentProfile(const QString &profileName) override;

    /** Reimplemented from TerminalInterfaceV2 */
    QVariant profileProperty(const QString &profileProperty) const override;
#endif

public Q_SLOTS:
    /**
     * creates and run a session using the specified profile and directory
     *
     * @param profileName Specifies the  name of the profile to create session
     * @param directory specifies The initial working directory of the created session
     *
     * This is highly experimental. Do not use it at the moment
     */
    void createSession(const QString &profileName = QString(), const QString &directory = QString());

    void showManageProfilesDialog(QWidget *parent);

    /**
     * Shows the dialog used to edit the profile used by the active session.  The
     * dialog will be non-modal and will delete itself when it is closed.
     *
     * This is experimental API and not guaranteed to be present in later KDE 4
     * releases.
     *
     * @param parent The parent widget of the new dialog.
     */
    void showEditCurrentProfileDialog(QWidget *parent);
    /**
     * Sends a profile change command to the active session.  This is equivalent to using
     * the konsoleprofile tool within the session to change its settings.  The @p text string
     * is a semi-colon separated list of property=value pairs, eg. "colors=Linux Colors"
     *
     * See the documentation for konsoleprofile for information on the format of @p text
     *
     * This is experimental API and not guaranteed to be present in later KDE 4 releases.
     */
    void changeSessionSettings(const QString &text);

    /**
     * Connects to an existing pseudo-teletype. See Session::openTeletype().
     * This must be called before the session is started by startProgram(),
     * or showShellInDir()
     *
     * @param ptyMasterFd The file descriptor of the pseudo-teletype (pty) master
     * @param runShell When true (default, legacy), runs the teletype in a shell
     * session environment. When false, the session is not run, so that the
     * KPtyProcess can be standalone, which may be useful for interactive programs.
     */
    void openTeletype(int ptyMasterFd, bool runShell = true);

    /**
     * Toggles monitoring for silence in the active session. If silence is detected,
     * the silenceDetected() signal is emitted.
     *
     * @param enabled Whether to enable or disable monitoring for silence.
     * */
    void setMonitorSilenceEnabled(bool enabled);

    /**
     * Toggles monitoring for activity in the active session. If activity is detected,
     * the activityDetected() signal is emitted.
     *
     * @param enabled Whether to enable or disable monitoring for activity.
     * */
    void setMonitorActivityEnabled(bool enabled);

    /**
     * Returns the status of the blur of the current profile.
     *
     * @return True if blur is enabled for the current active Konsole color profile.
     * */
    bool isBlurEnabled();

Q_SIGNALS:
    /**
     * Emitted when the key sequence for a shortcut, which is also a valid terminal key sequence,
     * is pressed while the terminal has focus.  By responding to this signal, the
     * controlling application can choose whether to execute the action associated with
     * the shortcut or ignore the shortcut and send the key
     * sequence to the terminal application.
     *
     * In the embedded terminal, shortcuts are overridden and sent to the terminal by default.
     * Set @p override to false to prevent this happening and allow the shortcut to be triggered
     * normally.
     *
     * overrideShortcut() is not called for shortcuts which are not valid terminal key sequences,
     * eg. shortcuts with two or more modifiers.
     *
     * @param event Describes the keys that were pressed.
     * @param override Set this to false to prevent the terminal display from overriding the shortcut
     */
    void overrideShortcut(QKeyEvent *event, bool &override);

    /**
     * Emitted when silence has been detected in the active session. Monitoring
     * for silence has to be enabled first using setMonitorSilenceEnabled().
     */
    void silenceDetected();

    /**
     * Emitted when activity has been detected in the active session. Monitoring
     * for activity has to be enabled first using setMonitorActivityEnabled().
     */
    void activityDetected();

    /**
     * Emitted when the current working directory of the active session has changed.
     */
    void currentDirectoryChanged(const QString &dir);

protected:
    /** Reimplemented from KParts::PartBase. */
    bool openFile() override;
    bool openUrl(const QUrl &url) override;

private Q_SLOTS:
    void activeViewChanged(SessionController *controller);
    void activeViewTitleChanged(ViewProperties *properties);
    void terminalExited();
    void newTab();
    void overrideTerminalShortcut(QKeyEvent *, bool &override);
    void notificationChanged(Session::Notification notification, bool enabled);

private:
    Session *activeSession() const;

private:
    ViewManager *_viewManager;
    SessionController *_pluggedController;
};
}

#endif // PART_H

/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>

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

#ifndef PART_H
#define PART_H

// KDE
#include <KParts/Part>
#include <kde_terminal_interface.h>

// Qt
#include <QtCore/QVariantList>

// Konsole
#include "Profile.h"

class QAction;
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
class Part : public KParts::ReadOnlyPart , public TerminalInterface
{
    Q_OBJECT
    Q_INTERFACES(TerminalInterface)
public:
    /** Constructs a new Konsole part with the specified parent. */
    explicit Part(QWidget* parentWidget , QObject* parent, const QVariantList&);
    virtual ~Part();

    /** Reimplemented from TerminalInterface. */
    virtual void startProgram(const QString& program,
                              const QStringList& arguments) Q_DECL_OVERRIDE;
    /** Reimplemented from TerminalInterface. */
    virtual void showShellInDir(const QString& dir) Q_DECL_OVERRIDE;
    /** Reimplemented from TerminalInterface. */
    virtual void sendInput(const QString& text) Q_DECL_OVERRIDE;

    /** Reimplemented from TerminalInterface. */
    virtual int terminalProcessId() Q_DECL_OVERRIDE;

    /** Reimplemented from TerminalInterface. */
    virtual int foregroundProcessId() Q_DECL_OVERRIDE;

    /** Reimplemented from TerminalInterface. */
    virtual QString foregroundProcessName() Q_DECL_OVERRIDE;

public slots:
    /**
     * creates and run a session using the specified profile and directory
     *
     * @param profileName Specifies the  name of the profile to create session
     * @param directory specifies The initial working directory of the created session
     *
     * This is highly experimental. Do not use it at the moment
     */
    void createSession(const QString& profileName = QString(), const QString& directory = QString());

    /**
     * Returns a list of names of all available profiles
     *
     * This is highly experimental. Do not use it at the moment
     */
    QStringList profileNameList() const;

    /**
     * Shows the dialog used to manage profiles in Konsole.  The dialog
     * will be non-modal and will delete itself when it is closed.
     *
     * This is experimental API and not guaranteed to be present in later
     * KDE 4 releases.
     *
     * @param parent The parent widget of the new dialog.
     */
    void showManageProfilesDialog(QWidget* parent);
    /**
     * Shows the dialog used to edit the profile used by the active session.  The
     * dialog will be non-modal and will delete itself when it is closed.
     *
     * This is experimental API and not guaranteed to be present in later KDE 4
     * releases.
     *
     * @param parent The parent widget of the new dialog.
     */
    void showEditCurrentProfileDialog(QWidget* parent);
    /**
     * Sends a profile change command to the active session.  This is equivalent to using
     * the konsoleprofile tool within the session to change its settings.  The @p text string
     * is a semi-colon separated list of property=value pairs, eg. "colors=Linux Colors"
     *
     * See the documentation for konsoleprofile for information on the format of @p text
     *
     * This is experimental API and not guaranteed to be present in later KDE 4 releases.
     */
    void changeSessionSettings(const QString& text);

    /**
     * Connects to an existing pseudo-teletype. See Session::openTeletype().
     * This must be called before the session is started by startProgram(),
     * or showShellInDir()
     *
     * @param ptyMasterFd The file descriptor of the pseudo-teletype (pty) master
     */
    void openTeletype(int ptyMasterFd);

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
    * Returns the current working directory of the active session
    *
    * TODO: this should better be moved into TerminalInterface.
    */
    QString currentWorkingDirectory() const;

signals:
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
    void overrideShortcut(QKeyEvent* event, bool& override);

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
    void currentDirectoryChanged(const QString& dir);

protected:
    /** Reimplemented from KParts::PartBase. */
    virtual bool openFile() Q_DECL_OVERRIDE;
    virtual bool openUrl(const QUrl& url) Q_DECL_OVERRIDE;

private slots:
    void activeViewChanged(SessionController* controller);
    void activeViewTitleChanged(ViewProperties* properties);
    void showManageProfilesDialog();
    void terminalExited();
    void newTab();
    void overrideTerminalShortcut(QKeyEvent*, bool& override);
    void sessionStateChanged(int state);
private:
    Session* activeSession() const;
    void createGlobalActions();
    void setupActionsForSession(SessionController*);

private:
    ViewManager* _viewManager;
    SessionController* _pluggedController;
    QAction* _manageProfilesAction;
};
}

#endif // PART_H

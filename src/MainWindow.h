/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// Qt
#include <QExplicitlySharedDataPointer>
#include <QPointer>
#include <QUrl>

// KDE
#include <KXmlGuiWindow>

// Konsole
#include "widgets/ViewSplitter.h"

#include "pluginsystem/IKonsolePlugin.h"

#include "konsole_export.h"

#include <vector>

class QAction;
class KActionMenu;
class KToggleAction;

namespace Konsole
{
class ViewManager;
class ViewProperties;
class Session;
class SessionController;
class Profile;
class ProfileList;
class BookmarkHandler;

/**
 * The main window.  This contains the menus and an area which contains the terminal displays.
 *
 * The main window does not create the views or the container widgets which hold the views.
 * This is done by the ViewManager class.  When a MainWindow is instantiated, it creates
 * a new ViewManager.  The ViewManager can then be used to create new terminal displays
 * inside the window.
 *
 * Do not construct new main windows directly, use Application's newMainWindow() method.
 */
class KONSOLE_EXPORT MainWindow : public KXmlGuiWindow
{
    Q_OBJECT

public:
    /**
     * Constructs a new main window.  Do not create new main windows directly, use Application's
     * newMainWindow() method instead.
     */
    MainWindow();

    /**
     * Returns the view manager associated with this window.  The view manager can be used to
     * create new views on particular session objects inside this window.
     */
    ViewManager *viewManager() const;

    /**
     * Create a new session.
     *
     * @param profile The profile to use to create the new session.
     * @param directory Initial working directory for the new session or empty
     * if the default working directory associated with the profile should be used.
     */
    Session *createSession(QExplicitlySharedDataPointer<Profile> profile, const QString &directory);

    /**
     * create a new SSH session.
     *
     * @param profile The profile to use to create the new session.
     * @param url the URL representing the new SSH connection
     */
    Session *createSSHSession(QExplicitlySharedDataPointer<Profile> profile, const QUrl &url);

    /**
     * Helper method to make this window get input focus
     */
    void setFocus();

    /**
     * Set the initial visibility of the menubar.
     */
    void setMenuBarInitialVisibility(bool showMenuBar);

    /**
     * @brief Set the frameless state
     *
     * @param frameless If true, no titlebar or frame is displayed.
     */
    void setRemoveWindowTitleBarAndFrame(bool frameless);

    /**
     * A reference to a plugin on the system.
     */
    void addPlugin(IKonsolePlugin *plugin);

    /**
     * creates a new tab for the main window
     */
    void newTab();

Q_SIGNALS:

    /**
     * Emitted by the main window to request the creation of a
     * new session in a new window.
     *
     * @param profile The profile to use to create the
     * first session in the new window.
     * @param directory Initial working directory for the new window or empty
     * if the default working directory associated with the profile should
     * be used.
     */
    void newWindowRequest(const QExplicitlySharedDataPointer<Profile> &profile, const QString &directory);

    /**
     * Emitted when a view for one session is detached from this window
     */
    void terminalsDetached(ViewSplitter *splitter, QHash<TerminalDisplay *, Session *> sessionsMap);

protected:
    // Reimplemented for internal reasons.
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

    // reimplemented from KMainWindow
    bool queryClose() override;
    void saveProperties(KConfigGroup &group) override;
    void readProperties(const KConfigGroup &group) override;
    void saveGlobalProperties(KConfig *config) override;
    void readGlobalProperties(KConfig *config) override;

    // reimplemented from QWidget
    bool focusNextPrevChild(bool next) override;

private Q_SLOTS:
    void cloneTab();
    void newWindow();
    void showManageProfilesDialog();
    void activateMenuBar();
    void showSettingsDialog(bool showProfilePage = false);
    void showShortcutsDialog();
    void newFromProfile(const QExplicitlySharedDataPointer<Profile> &profile);
    void activeViewChanged(SessionController *controller);
    void disconnectController(SessionController *controller);
    void activeViewTitleChanged(ViewProperties *);

    void profileListChanged(const QList<QAction *> &sessionActions);
    void configureNotifications();
    void setBlur(bool blur);

    void updateWindowIcon();
    void updateWindowCaption();
    void openUrls(const QList<QUrl> &urls);

    // Sets the list of profiles to be displayed under the "New Tab" action
    void setProfileList(ProfileList *list);

    void applyKonsoleSettings();

    void updateUseTransparency();

public Q_SLOTS:
    void viewFullScreen(bool fullScreen);

private:
    void applyMainWindowSettings(const KConfigGroup &config) override;

    /**
     * Returns true if the window geometry was previously saved to the
     * config file, false otherwise.
     */
    bool wasWindowGeometrySaved() const;

    void correctStandardShortcuts();
    void rememberMenuAccelerators();
    void removeMenuAccelerators();
    void restoreMenuAccelerators();
    void setupActions();
    QString activeSessionDir() const;
    void triggerAction(const QString &name) const;

    /**
     * Returns the bookmark handler associated with this window.
     */
    BookmarkHandler *bookmarkHandler() const;

    // sets the active shortcuts of actions in 'dest' to the shortcuts of actions
    // with the same name in 'source' (see QAction::ActiveShortcut)
    static void syncActiveShortcuts(KActionCollection *dest, const KActionCollection *source);

private:
    ViewManager *_viewManager;
    BookmarkHandler *_bookmarkHandler;
    KToggleAction *_toggleMenuBarAction;
    KActionMenu *_newTabMenuAction;

    QPointer<SessionController> _pluggedController;
    std::vector<IKonsolePlugin *> _plugins;
    bool _blurEnabled = false;
    bool _firstShowEvent = true;

    struct {
        bool enabled = false; // indicates that we got a command line argument for menubar
        bool showMenuBar = true;
    } _windowArgsMenuBarVisible;
};
}

#endif // MAINWINDOW_H

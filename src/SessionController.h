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

#ifndef SESSIONCONTROLLER_H
#define SESSIONCONTROLLER_H

// Qt
#include <QSet>
#include <QPointer>
#include <QString>
#include <QRegularExpression>

// KDE
#include <KXMLGUIClient>

// Konsole
#include "ViewProperties.h"
#include "Profile.h"
#include "Enumeration.h"

class QAction;
class QTextCodec;
class QKeyEvent;
class QTimer;
class QUrl;

class KCodecAction;
class QAction;
class KActionMenu;

namespace Konsole {
class Session;
class SessionGroup;
class ScreenWindow;
class TerminalDisplay;
class IncrementalSearchBar;
class ProfileList;
class RegExpFilter;
class UrlFilter;
class FileFilter;
class EditProfileDialog;

using SessionPtr = QPointer<Session>;

/**
 * Provides the menu actions to manipulate a single terminal session and view pair.
 * The actions provided by this class are defined in the sessionui.rc XML file.
 *
 * SessionController monitors the session and provides access to basic information
 * about the session such as title(), icon() and currentDir().  SessionController
 * provides notifications of activity in the session via the activity() signal.
 *
 * When the controlled view receives the focus, the focused() signal is emitted
 * with a pointer to the controller.  This can be used by main application window
 * which contains the view to plug the controller's actions into the menu when
 * the view is focused.
 */
class KONSOLEPRIVATE_EXPORT SessionController : public ViewProperties, public KXMLGUIClient
{
    Q_OBJECT

public:
    enum CopyInputToEnum {
        /** Copy keyboard input to all the other tabs in current window */
        CopyInputToAllTabsMode = 0,

        /** Copy keyboard input to user selected tabs in current window */
        CopyInputToSelectedTabsMode = 1,

        /** Do not copy keyboard input to other tabs */
        CopyInputToNoneMode = 2
    };

    /**
     * Constructs a new SessionController which operates on @p session and @p view.
     */
    SessionController(Session *session, TerminalDisplay *view, QObject *parent);
    ~SessionController() Q_DECL_OVERRIDE;

    /** Returns the session associated with this controller */
    QPointer<Session> session()
    {
        return _session;
    }

    /** Returns the view associated with this controller */
    QPointer<TerminalDisplay> view()
    {
        return _view;
    }

    /**
     * Returns the "window title" of the associated session.
     */
    QString userTitle() const;

    /**
     * Returns true if the controller is valid.
     * A valid controller is one which has a non-null session() and view().
     *
     * Equivalent to "!session().isNull() && !view().isNull()"
     */
    bool isValid() const;

    /** Set the start line from which the next search will be done **/
    void setSearchStartTo(int line);

    /** set start line to the first or last line (depending on the reverse search
     * setting) in the terminal display **/
    void setSearchStartToWindowCurrentLine();

    /**
     * Sets the action displayed in the session's context menu to hide or
     * show the menu bar.
     */
    void setShowMenuAction(QAction *action);

    EditProfileDialog *profileDialogPointer();

    // reimplemented
    QUrl url() const Q_DECL_OVERRIDE;
    QString currentDir() const Q_DECL_OVERRIDE;
    void rename() Q_DECL_OVERRIDE;
    bool confirmClose() const Q_DECL_OVERRIDE;
    virtual bool confirmForceClose() const;

    // Reimplemented to watch for events happening to the view
    bool eventFilter(QObject *watched, QEvent *event) Q_DECL_OVERRIDE;

    /** Returns the set of all controllers that exist. */
    static QSet<SessionController *> allControllers()
    {
        return _allControllers;
    }

    /* Returns true if called within a KPart; false if called within Konsole. */
    bool isKonsolePart() const;
    bool isReadOnly() const;

Q_SIGNALS:
    /**
     * Emitted when the view associated with the controller is focused.
     * This can be used by other classes to plug the controller's actions into a window's
     * menus.
     */
    void focused(SessionController *controller);

    void rawTitleChanged();

    /**
     * Emitted when the current working directory of the session associated with
     * the controller is changed.
     */
    void currentDirectoryChanged(const QString &dir);

    /**
     * Emitted when the user changes the tab title.
     */
    void tabRenamedByUser(bool renamed) const;

public Q_SLOTS:
    /**
     * Issues a command to the session to navigate to the specified URL.
     * This may not succeed if the foreground program does not understand
     * the command sent to it ( 'cd path' for local URLs ) or is not
     * responding to input.
     *
     * openUrl() currently supports urls for local paths and those
     * using the 'ssh' protocol ( eg. "ssh://joebloggs@hostname" )
     */
    void openUrl(const QUrl &url);

    /**
     * update actions which are meaningful only when primary screen is in use.
     */
    void setupPrimaryScreenSpecificActions(bool use);

    /**
     * update actions which are closely related with the selected text.
     */
    void selectionChanged(const QString &selectedText);

    /**
     * close the associated session. This might involve user interaction for
     * confirmation.
     */
    void closeSession();

    /**  Increase font size */
    void increaseFontSize();

    /**  Decrease font size */
    void decreaseFontSize();

    /** Reset font size */
    void resetFontSize();

    /** Close the incremental search */
    void searchClosed(); // called when the user clicks on the

private Q_SLOTS:
    // menu item handlers
    void openBrowser();
    void copy();
    void paste();
    void selectAll();
    void selectLine();
    void pasteFromX11Selection(); // shortcut only
    void copyInputActionsTriggered(QAction *action);
    void copyInputToAllTabs();
    void copyInputToSelectedTabs();
    void copyInputToNone();
    void editCurrentProfile();
    void changeCodec(QTextCodec *codec);
    void enableSearchBar(bool showSearchBar);
    void searchHistory(bool showSearchBar);
    void searchBarEvent();
    void searchFrom();
    void findNextInHistory();
    void findPreviousInHistory();
    void changeSearchMatch();
    void print_screen();
    void saveHistory();
    void showHistoryOptions();
    void clearHistory();
    void clearHistoryAndReset();
    void monitorActivity(bool monitor);
    void monitorSilence(bool monitor);
    void renameSession();
    void switchProfile(const Profile::Ptr &profile);
    void handleWebShortcutAction();
    void configureWebShortcuts();
    void sendSignal(QAction *action);
    void sendBackgroundColor();
    void toggleReadOnly();

    // other
    void setupSearchBar();
    void prepareSwitchProfileMenu();
    void updateCodecAction();
    void showDisplayContextMenu(const QPoint &position);
    void movementKeyFromSearchBarReceived(QKeyEvent *event);
    void sessionStateChanged(int state);
    void sessionAttributeChanged();
    void sessionReadOnlyChanged();
    void searchTextChanged(const QString &text);
    void searchCompleted(bool success);

    void updateFilterList(Profile::Ptr profile); // Called when the profile has changed, so we might need to change the list of filters

    void interactionHandler();
    void snapshot(); // called periodically as the user types
    // to take a snapshot of the state of the
    // foreground process in the terminal

    void highlightMatches(bool highlight);
    void scrollBackOptionsChanged(int mode, int lines);
    void sessionResizeRequest(const QSize &size);
    void trackOutput(QKeyEvent *event);  // move view to end of current output
    // when a key press occurs in the
    // display area

    void updateSearchFilter();

    void zmodemDownload();
    void zmodemUpload();

    // update actions related with selected text
    void updateCopyAction(const QString &selectedText);
    void updateWebSearchMenu();

private:
    Q_DISABLE_COPY(SessionController)

    // begins the search
    // text - pattern to search for
    // direction - value from SearchHistoryTask::SearchDirection enum to specify
    //             the search direction
    void beginSearch(const QString &text, Enum::SearchDirection direction);
    QRegularExpression regexpFromSearchBarOptions() const;
    bool reverseSearchChecked() const;
    void setupCommonActions();
    void setupExtraActions();
    void removeSearchFilter(); // remove and delete the current search filter if set
    void setFindNextPrevEnabled(bool enabled);
    void listenForScreenWindowUpdates();

private:
    void updateSessionIcon();
    void updateReadOnlyActionStates();

    QPointer<Session> _session;
    QPointer<TerminalDisplay> _view;
    SessionGroup *_copyToGroup;

    ProfileList *_profileList;

    QIcon _sessionIcon;
    QString _sessionIconName;
    int _previousState;

    RegExpFilter *_searchFilter;
    UrlFilter *_urlFilter;
    FileFilter *_fileFilter;

    QAction *_copyInputToAllTabsAction;

    QAction *_findAction;
    QAction *_findNextAction;
    QAction *_findPreviousAction;

    QTimer *_interactionTimer;

    int _searchStartLine;
    int _prevSearchResultLine;

    KCodecAction *_codecAction;

    KActionMenu *_switchProfileMenu;
    KActionMenu *_webSearchMenu;

    bool _listenForScreenWindowUpdates;
    bool _preventClose;

    bool _keepIconUntilInteraction;

    QString _selectedText;

    QAction *_showMenuAction;

    static QSet<SessionController *> _allControllers;
    static int _lastControllerId;

    QStringList _bookmarkValidProgramsToClear;

    bool _isSearchBarEnabled;
    QPointer<EditProfileDialog> _editProfileDialog;

    QString _searchText;
    QPointer<IncrementalSearchBar> _searchBar;
};
inline bool SessionController::isValid() const
{
    return !_session.isNull() && !_view.isNull();
}

}

#endif //SESSIONCONTROLLER_H

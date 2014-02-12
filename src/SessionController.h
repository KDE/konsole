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
#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QHash>

// KDE
#include <KIcon>
#include <KXMLGUIClient>

// Konsole
#include "ViewProperties.h"
#include "Profile.h"

namespace KIO
{
class Job;
}

class QAction;
class QTextCodec;
class QKeyEvent;
class QTimer;

class KCodecAction;
class KUrl;
class KJob;
class KAction;
class KActionMenu;

namespace Konsole
{
class Session;
class SessionGroup;
class ScreenWindow;
class TerminalDisplay;
class IncrementalSearchBar;
class ProfileList;
class UrlFilter;
class RegExpFilter;
class EditProfileDialog;

// SaveHistoryTask
class TerminalCharacterDecoder;

typedef QPointer<Session> SessionPtr;

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
class KONSOLEPRIVATE_EXPORT SessionController : public ViewProperties , public KXMLGUIClient
{
    Q_OBJECT

public:
    enum CopyInputToEnum {
        /** Copy keyboard input to all the other tabs in current window */
        CopyInputToAllTabsMode = 0 ,

        /** Copy keyboard input to user selected tabs in current window */
        CopyInputToSelectedTabsMode = 1 ,

        /** Do not copy keyboard input to other tabs */
        CopyInputToNoneMode = 2
    };

    /**
     * Constructs a new SessionController which operates on @p session and @p view.
     */
    SessionController(Session* session , TerminalDisplay* view, QObject* parent);
    ~SessionController();

    /** Returns the session associated with this controller */
    QPointer<Session> session() {
        return _session;
    }
    /** Returns the view associated with this controller */
    QPointer<TerminalDisplay>  view()    {
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
     * Sets the widget used for searches through the session's output.
     *
     * When the user clicks on the "Search Output" menu action the @p searchBar 's
     * show() method will be called.  The SessionController will then connect to the search
     * bar's signals to update the search when the widget's controls are pressed.
     */
    void setSearchBar(IncrementalSearchBar* searchBar);
    /**
     * see setSearchBar()
     */
    IncrementalSearchBar* searchBar() const;

    /**
     * Sets the action displayed in the session's context menu to hide or
     * show the menu bar.
     */
    void setShowMenuAction(QAction* action);

    EditProfileDialog* profileDialogPointer();

    // reimplemented
    virtual KUrl url() const;
    virtual QString currentDir() const;
    virtual void rename();
    virtual bool confirmClose() const;
    virtual bool confirmForceClose() const;

    // Reimplemented to watch for events happening to the view
    virtual bool eventFilter(QObject* watched , QEvent* event);

    /** Returns the set of all controllers that exist. */
    static QSet<SessionController*> allControllers() {
        return _allControllers;
    }

signals:
    /**
     * Emitted when the view associated with the controller is focused.
     * This can be used by other classes to plug the controller's actions into a window's
     * menus.
     */
    void focused(SessionController* controller);

    void rawTitleChanged();

    /**
     * Emitted when the current working directory of the session associated with
     * the controller is changed.
     */
    void currentDirectoryChanged(const QString& dir);

public slots:
    /**
     * Issues a command to the session to navigate to the specified URL.
     * This may not succeed if the foreground program does not understand
     * the command sent to it ( 'cd path' for local URLs ) or is not
     * responding to input.
     *
     * openUrl() currently supports urls for local paths and those
     * using the 'ssh' protocol ( eg. "ssh://joebloggs@hostname" )
     */
    void openUrl(const KUrl& url);

    /**
     * update actions which are meaningful only when primary screen is in use.
     */
    void setupPrimaryScreenSpecificActions(bool use);

    /**
     * update actions which are closely related with the selected text.
     */
    void selectionChanged(const QString& selectedText);

    /**
     * close the associated session. This might involve user interaction for
     * confirmation.
     */
    void closeSession();

    /**  Increase font size */
    void increaseFontSize();

    /**  Decrease font size */
    void decreaseFontSize();

private slots:
    // menu item handlers
    void openBrowser();
    void copy();
    void paste();
    void selectAll();
    void selectLine();
    void pasteFromX11Selection(); // shortcut only
    void copyInputActionsTriggered(QAction* action);
    void copyInputToAllTabs();
    void copyInputToSelectedTabs();
    void copyInputToNone();
    void editCurrentProfile();
    void changeCodec(QTextCodec* codec);
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
    void switchProfile(Profile::Ptr profile);
    void handleWebShortcutAction();
    void configureWebShortcuts();
    void sendSignal(QAction* action);

    // other
    void prepareSwitchProfileMenu();
    void updateCodecAction();
    void showDisplayContextMenu(const QPoint& position);
    void movementKeyFromSearchBarReceived(QKeyEvent *event);
    void sessionStateChanged(int state);
    void sessionTitleChanged();
    void searchTextChanged(const QString& text);
    void searchCompleted(bool success);
    void searchClosed(); // called when the user clicks on the
    // history search bar's close button

    void interactionHandler();
    void snapshot(); // called periodically as the user types
    // to take a snapshot of the state of the
    // foreground process in the terminal

    void requireUrlFilterUpdate();
    void highlightMatches(bool highlight);
    void scrollBackOptionsChanged(int mode , int lines);
    void sessionResizeRequest(const QSize& size);
    void trackOutput(QKeyEvent* event);  // move view to end of current output
    // when a key press occurs in the
    // display area

    void updateSearchFilter();

    void zmodemDownload();
    void zmodemUpload();

    /* Returns true if called within a KPart; false if called within Konsole. */
    bool isKonsolePart() const;

    // update actions related with selected text
    void updateCopyAction(const QString& selectedText);
    void updateWebSearchMenu();

private:
    // begins the search
    // text - pattern to search for
    // direction - value from SearchHistoryTask::SearchDirection enum to specify
    //             the search direction
    void beginSearch(const QString& text , int direction);
    QRegExp regexpFromSearchBarOptions();
    bool reverseSearchChecked() const;
    void setupCommonActions();
    void setupExtraActions();
    void removeSearchFilter(); // remove and delete the current search filter if set
    void setFindNextPrevEnabled(bool enabled);
    void listenForScreenWindowUpdates();

private:
    void updateSessionIcon();

    QPointer<Session>         _session;
    QPointer<TerminalDisplay> _view;
    SessionGroup*               _copyToGroup;

    ProfileList* _profileList;

    KIcon      _sessionIcon;
    QString    _sessionIconName;
    int        _previousState;

    UrlFilter*      _viewUrlFilter;
    RegExpFilter*   _searchFilter;

    KAction* _copyInputToAllTabsAction;

    KAction* _findAction;
    KAction* _findNextAction;
    KAction* _findPreviousAction;

    QTimer* _interactionTimer;

    bool _urlFilterUpdateRequired;

    int _searchStartLine;
    int _prevSearchResultLine;
    QPointer<IncrementalSearchBar> _searchBar;

    KCodecAction* _codecAction;

    KActionMenu* _switchProfileMenu;
    KActionMenu* _webSearchMenu;

    bool _listenForScreenWindowUpdates;
    bool _preventClose;

    bool _keepIconUntilInteraction;

    QString _selectedText;

    QAction* _showMenuAction;

    static QSet<SessionController*> _allControllers;
    static int _lastControllerId;
    static const KIcon _activityIcon;
    static const KIcon _silenceIcon;
    static const KIcon _broadcastIcon;

    QStringList _bookmarkValidProgramsToClear;

    bool _isSearchBarEnabled;
    QWeakPointer<EditProfileDialog> _editProfileDialog;

    QString _searchText;
};
inline bool SessionController::isValid() const
{
    return !_session.isNull() && !_view.isNull();
}

/**
 * Abstract class representing a task which can be performed on a group of sessions.
 *
 * Create a new instance of the appropriate sub-class for the task you want to perform and
 * call the addSession() method to add each session which needs to be processed.
 *
 * Finally, call the execute() method to perform the sub-class specific action on each
 * of the sessions.
 */
class SessionTask : public QObject
{
    Q_OBJECT

public:
    explicit SessionTask(QObject* parent = 0);

    /**
     * Sets whether the task automatically deletes itself when the task has been finished.
     * Depending on whether the task operates synchronously or asynchronously, the deletion
     * may be scheduled immediately after execute() returns or it may happen some time later.
     */
    void setAutoDelete(bool enable);
    /** Returns true if the task automatically deletes itself.  See setAutoDelete() */
    bool autoDelete() const;

    /** Adds a new session to the group */
    void addSession(Session* session);

    /**
     * Executes the task on each of the sessions in the group.
     * The completed() signal is emitted when the task is finished, depending on the specific sub-class
     * execute() may be synchronous or asynchronous
     */
    virtual void execute() = 0;

signals:
    /**
     * Emitted when the task has completed.
     * Depending on the task this may occur just before execute() returns, or it
     * may occur later
     *
     * @param success Indicates whether the task completed successfully or not
     */
    void completed(bool success);

protected:
    /** Returns a list of sessions in the group */
    QList< SessionPtr > sessions() const;

private:
    bool _autoDelete;
    QList< SessionPtr > _sessions;
};

/**
 * A task which prompts for a URL for each session and saves that session's output
 * to the given URL
 */
class SaveHistoryTask : public SessionTask
{
    Q_OBJECT

public:
    /** Constructs a new task to save session output to URLs */
    explicit SaveHistoryTask(QObject* parent = 0);
    virtual ~SaveHistoryTask();

    /**
     * Opens a save file dialog for each session in the group and begins saving
     * each session's history to the given URL.
     *
     * The data transfer is performed asynchronously and will continue after execute() returns.
     */
    virtual void execute();

private slots:
    void jobDataRequested(KIO::Job* job , QByteArray& data);
    void jobResult(KJob* job);

private:
    class SaveJob // structure to keep information needed to process
        // incoming data requests from jobs
    {
    public:
        SessionPtr session; // the session associated with a history save job
        int lastLineFetched; // the last line processed in the previous data request
        // set this to -1 at the start of the save job

        TerminalCharacterDecoder* decoder;  // decoder used to convert terminal characters
        // into output
    };

    QHash<KJob*, SaveJob> _jobSession;
};

//class SearchHistoryThread;
/**
 * A task which searches through the output of sessions for matches for a given regular expression.
 * SearchHistoryTask operates on ScreenWindow instances rather than sessions added by addSession().
 * A screen window can be added to the list to search using addScreenWindow()
 *
 * When execute() is called, the search begins in the direction specified by searchDirection(),
 * starting at the position of the current selection.
 *
 * FIXME - This is not a proper implementation of SessionTask, in that it ignores sessions specified
 * with addSession()
 *
 * TODO - Implementation requirements:
 *          May provide progress feedback to the user when searching very large output logs.
 */
class SearchHistoryTask : public SessionTask
{
    Q_OBJECT

public:
    /**
     * This enum describes the strategies available for searching through the
     * session's output.
     */
    enum SearchDirection {
        /** Searches forwards through the output, starting at the current selection. */
        ForwardsSearch,
        /** Searches backwards through the output, starting at the current selection. */
        BackwardsSearch
    };

    /**
     * Constructs a new search task.
     */
    explicit SearchHistoryTask(QObject* parent = 0);

    /** Adds a screen window to the list to search when execute() is called. */
    void addScreenWindow(Session* session , ScreenWindow* searchWindow);

    /** Sets the regular expression which is searched for when execute() is called */
    void setRegExp(const QRegExp& regExp);
    /** Returns the regular expression which is searched for when execute() is called */
    QRegExp regExp() const;

    /** Specifies the direction to search in when execute() is called. */
    void setSearchDirection(SearchDirection direction);
    /** Returns the current search direction.  See setSearchDirection(). */
    SearchDirection searchDirection() const;

    /** The line from which the search will be done **/
    void setStartLine(int startLine);

    /**
     * Performs a search through the session's history, starting at the position
     * of the current selection, in the direction specified by setSearchDirection().
     *
     * If it finds a match, the ScreenWindow specified in the constructor is
     * scrolled to the position where the match occurred and the selection
     * is set to the matching text.  execute() then returns immediately.
     *
     * To continue the search looking for further matches, call execute() again.
     */
    virtual void execute();

private:
    typedef QPointer<ScreenWindow> ScreenWindowPtr;

    void executeOnScreenWindow(SessionPtr session , ScreenWindowPtr window);
    void highlightResult(ScreenWindowPtr window , int position);

    QMap< SessionPtr , ScreenWindowPtr > _windows;
    QRegExp _regExp;
    SearchDirection _direction;
    int _startLine;

    //static QPointer<SearchHistoryThread> _thread;
};
}

#endif //SESSIONCONTROLLER_H


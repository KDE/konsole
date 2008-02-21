/*
    Copyright (C) 2006-2007 by Robert Knight <robertknight@gmail.com>

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
#include <QtGui/QIcon>
#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QThread>
#include <QtCore/QHash>

// KDE
#include <KActionCollection>
#include <KIcon>
#include <KXMLGUIClient>

// Konsole
#include "HistorySizeDialog.h"
#include "ViewProperties.h"

namespace KIO
{
    class Job;
}

class QAction;
class QTextCodec;
class KCodecAction;
class KMenu;
class KUrl;
class KJob;

namespace Konsole
{

class Session;
class ScreenWindow;
class TerminalDisplay;
class IncrementalSearchBar;
class ProfileList;
class UrlFilter;
class RegExpFilter;

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
class SessionController : public ViewProperties , public KXMLGUIClient
{
Q_OBJECT
    
public:
    /**
     * Constructs a new SessionController which operates on @p session and @p view.
     */
    SessionController(Session* session , TerminalDisplay* view, QObject* parent);
    ~SessionController();

    
    /** Returns the session associated with this controller */
    QPointer<Session> session() { return _session; }
    /** Returns the view associated with this controller */
    QPointer<TerminalDisplay>  view()    { return _view;    }
	
	/** 
	 * Returns true if the controller is valid.
	 * A valid controller is one which has a non-null session() and view().
	 *
	 * Equivalent to "!session().isNull() && !view().isNull()"
	 */
	bool isValid() const;

    /** 
     * Sets the widget used for searches through the session's output.
     *
     * When the user clicks on the "Search Output" menu action the @p searchBar 's 
     * show() method will be called.  The SessionController will then connect to the search 
     * bar's signals to update the search when the widget's controls are pressed.
     */
    void setSearchBar( IncrementalSearchBar* searchBar );
    /** 
     * see setSearchBar()
     */
    IncrementalSearchBar* searchBar() const;

    /**
     * Sets the action displayed in the session's context menu to hide or
     * show the menu bar. 
     */
    void setShowMenuAction(QAction* action);

    // reimplemented
    virtual KUrl url() const;
    virtual QString currentDir() const;
    virtual void rename();

    // Reimplemented to watch for events happening to the view
    virtual bool eventFilter(QObject* watched , QEvent* event);

signals:
    /**
     * Emitted when the view associated with the controller is focused.  
     * This can be used by other classes to plug the controller's actions into a window's
     * menus. 
     */
    void focused( SessionController* controller );

    /**
     * Emitted when the user enables the "Send Input to All" menu
     * item associated with this session.
     */
    void sendInputToAll(bool sendToAll);

public slots:
    /**
     * Issues a command to the session to navigate to the specified URL.
     * This may not succeed if the foreground program does not understand 
     * the command sent to it ( 'cd path' for local URLs ) or is not 
     * responding to input.
     *
     * openUrl() currently supports urls for local paths and those
     * using the 'ssh' protocol ( eg. ssh://joebloggs@hostname )
     */
    void openUrl( const KUrl& url ); 

private slots:
    // menu item handlers
    void openBrowser();
    void copy();
    void paste();
	void pasteSelection(); // shortcut only
    void clear();
    void clearAndReset();
    void editCurrentProfile();
    void changeCodec(QTextCodec* codec);
    //void searchHistory();
    void searchHistory(bool showSearchBar);
    void findNextInHistory();
    void findPreviousInHistory();
    void saveHistory();
    void showHistoryOptions();
    void clearHistory();
    void clearHistoryAndReset();
    void closeSession();
    void monitorActivity(bool monitor);
    void monitorSilence(bool monitor);
    void increaseTextSize();
    void decreaseTextSize();
    void renameSession();
    void saveSession();
    void changeProfile(const QString& key);

    // other
    void prepareChangeProfileMenu();
    void updateCodecAction();
    void showDisplayContextMenu(TerminalDisplay* display , int state , const QPoint& position);
    void sessionStateChanged(int state);
    void sessionTitleChanged();
    void searchTextChanged(const QString& text);
    void searchCompleted(bool success);
    void searchClosed(); // called when the user clicks on the
                         // history search bar's close button 

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

    // debugging slots
    void debugProcess();

private:
    // begins the search
    // text - pattern to search for
    // direction - value from SearchHistoryTask::SearchDirection enum to specify
    //             the search direction
    void beginSearch(const QString& text , int direction);
    void setupActions();
    void removeSearchFilter(); // remove and delete the current search filter if set
    void setFindNextPrevEnabled(bool enabled);
	void listenForScreenWindowUpdates();

private:
    static KIcon _activityIcon;
    static KIcon _silenceIcon;

    QPointer<Session>         _session;
    QPointer<TerminalDisplay> _view;
    
    ProfileList* _profileList;

    KIcon      _sessionIcon;
    QString    _sessionIconName;
    int        _previousState;

    UrlFilter*      _viewUrlFilter;
    RegExpFilter*   _searchFilter; 

    KAction* _searchToggleAction;
    KAction* _findNextAction;
    KAction* _findPreviousAction;
    
    
    bool _urlFilterUpdateRequired;

    QPointer<IncrementalSearchBar> _searchBar;

    KCodecAction* _codecAction;

    KMenu* _changeProfileMenu;

	bool _listenForScreenWindowUpdates;
	bool _preventClose;
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
   SessionTask(QObject* parent = 0);

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
    SaveHistoryTask(QObject* parent = 0);
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

    QHash<KJob*,SaveJob> _jobSession;
};

class SearchHistoryThread;
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
    enum SearchDirection
    {
        /** Searches forwards through the output, starting at the current selection. */
        ForwardsSearch,
        /** Searches backwars through the output, starting at the current selection. */
        BackwardsSearch  
    };

    /** 
     * Constructs a new search task. 
     */
    explicit SearchHistoryTask(QObject* parent = 0);

    /** Adds a screen window to the list to search when execute() is called. */
    void addScreenWindow( Session* session , ScreenWindow* searchWindow); 

    /** Sets the regular expression which is searched for when execute() is called */
    void setRegExp(const QRegExp& regExp);
    /** Returns the regular expression which is searched for when execute() is called */
    QRegExp regExp() const;
   
    /** Specifies the direction to search in when execute() is called. */ 
    void setSearchDirection( SearchDirection direction );
    /** Returns the current search direction.  See setSearchDirection(). */
    SearchDirection searchDirection() const;

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
    
    void executeOnScreenWindow( SessionPtr session , ScreenWindowPtr window );
    void highlightResult( ScreenWindowPtr window , int position);

    QMap< SessionPtr , ScreenWindowPtr > _windows;
    QRegExp _regExp;
    SearchDirection _direction;

    static QPointer<SearchHistoryThread> _thread;
};

}

#endif //SESSIONCONTROLLER_H

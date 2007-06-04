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
 * Provides the actions associated with a session in the Konsole main menu
 * and exposes information such as the title and icon associated with the session to view containers.
 *
 * Each view should have one SessionController associated with it
 *
 * The SessionController will delete itself if either the view or the session is destroyed, for 
 * this reason it is recommended that other classes which need a pointer to a SessionController
 * use QPointer<SessionController> rather than SessionController* 
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
     * Sets the widget used for searches through the session's history. 
     * The widget will be shown when the user clicks on the "Search History" menu action.
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
     * TODO: Only handles URLs using the file:/// protocol at present.
     */
    void openUrl( const KUrl& url ); 

private slots:
    // menu item handlers
    void copy();
    void paste();
    void clear();
    void clearAndReset();
    void editCurrentProfile();
    void changeCodec(QTextCodec* codec);
    void searchHistory();
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
    void showDisplayContextMenu(TerminalDisplay* display , int state , int x , int y);
    void sessionStateChanged(int state);
    void sessionTitleChanged();
    void searchTextChanged(const QString& text);
    void searchClosed(); // called when the user clicks on the
                         // history search bar's close button 

    void snapshot(); // called periodically as the user types
                     // to take a snapshot of the state of the
                     // foreground process in the terminal

    void requireUrlFilterUpdate();
    void highlightMatches(bool highlight);

    void scrollBackOptionsChanged(int mode , int lines);

    void sessionResizeRequest(const QSize& size);

    void trackOutput();  // move view to end of current output

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

private:
    QPointer<Session>         _session;
    QPointer<TerminalDisplay> _view;
    
    ProfileList* _profileList;

    KIcon      _sessionIcon;
    QString    _sessionIconName;
    int        _previousState;

    UrlFilter*      _viewUrlFilter;
    RegExpFilter*   _searchFilter; 

    KAction* _searchToggleAction;
    QAction* _findNextAction;
    QAction* _findPreviousAction;
    
    static KIcon _activityIcon;
    static KIcon _silenceIcon;

    bool _urlFilterUpdateRequired;

    QPointer<IncrementalSearchBar> _searchBar;

    KCodecAction* _codecAction;

    KMenu* _changeProfileMenu;
};

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
    */
   void completed();

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
 * 
 * TODO - Implementation requirements:
 *          Must provide progress feedback to the user when searching very large output logs.
 *
 *          - Remember where the search got to when it reaches the end of the output in each session
 *            calling execute() subsequently should continue the search.
 *            This allows the class to be used for both the "Search history for text" 
 *            and new-in-KDE-4 "Monitor output for text" actions   
 *
 * TODO:  Implement this
 */
class SearchHistoryTask : public SessionTask
{
Q_OBJECT

public:
    enum SearchDirection
    {
        Forwards,
        Backwards  
    };

    explicit SearchHistoryTask(ScreenWindow* window , QObject* parent = 0);

    /** Sets the regular expression which is searched for when execute() is called */
    void setRegExp(const QRegExp& regExp);
    /** Returns the regular expression which is searched for when execute() is called */
    QRegExp regExp() const;
    
    void setMatchCase(bool matchCase);
    bool matchCase() const;
    void setMatchRegExp(bool matchRegExp);
    bool matchRegExp() const;
    void setSearchDirection( SearchDirection direction );
    SearchDirection searchDirection() const;

    virtual void execute();

signals:
    /** 
     * Emitted when a match for the regular expression is found in a session's output.
     * The line numbers are given as offsets from the start of the history
     *
     * @param session The session in which a match for regExp() was found.
     * @param startLine The line in the output where the matched text starts
     * @param startColumn The column in the output where the matched text starts
     * @param endLine The line in the output where the matched text ends
     * @param endColumn The column in the output where the matched text ends 
     */
    void foundMatch(Session* session , 
                    int startLine , 
                    int startColumn , 
                    int endLine , 
                    int endColumn );
   
private:
    void highlightResult(int position);

    QRegExp _regExp;
    bool _matchRegExp;
    bool _matchCase;
    SearchDirection _direction;

    ScreenWindow* _screenWindow;

    static QPointer<SearchHistoryThread> _thread;
};

}

#endif //SESSIONCONTROLLER_H

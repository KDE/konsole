#ifndef SESSIONCONTROLLER_H
#define SESSIONCONTROLLER_H

// Qt
#include <QIcon>
#include <QList>
#include <QString>

// KDE
#include <KActionCollection>
#include <KXMLGUIClient>

// Konsole
#include "ViewProperties.h"
class QAction;
class TESession;
class TEWidget;
class UrlFilter;

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
    SessionController(TESession* session , TEWidget* view, QObject* parent);
    ~SessionController();

    /** Reimplemented to watch for events happening to the view */
    virtual bool eventFilter(QObject* watched , QEvent* event);

    /** Returns the session associated with this controller */
    TESession* session() { return _session; }
    /** Returns the view associated with this controller */
    TEWidget*  view()    { return _view;    }

signals:
    /**
     * Emitted when the view associated with the controller is focused.  
     * This can be used by other classes to plug the controller's actions into a window's
     * menus. 
     */
    void focused( SessionController* controller );

private slots:
    // menu item handlers
    void copy();
    void paste();
    void clear();
    void clearAndReset();
    void searchHistory();
    void findNextInHistory();
    void findPreviousInHistory();
    void saveHistory();
    void clearHistory();
    void closeSession();
    void monitorActivity(bool monitor);
    void monitorSilence(bool monitor);

    void sessionStateChanged(TESession* session,int state);
    void sessionTitleChanged();

private:
    void setupActions();

private:
    TESession* _session;
    TEWidget*  _view;
    KIcon      _sessionIcon;
    QString    _sessionIconName;
    int        _previousState;
    
    UrlFilter* _viewUrlFilter;

    static KIcon _activityIcon;
    static KIcon _silenceIcon;

};

#endif //SESSIONCONTROLLER_H

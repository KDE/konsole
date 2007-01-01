#ifndef SESSIONCONTROLLER_H
#define SESSIONCONTROLLER_H

// Qt
#include <QIcon>
#include <QList>
#include <QString>

// KDE
#include <KActionCollection>
#include <KXMLGUIClient>

class QAction;

class TESession;
class TEWidget;

/** 
 * Provides information (such as the title and icon) associated with  
 */
/*class ViewProperties : public QObject 
{
    
public:
    ViewProperties(QObject* parent) : QObject(parent) {}

    QIcon icon();
    QString title();

signals:
    void iconChanged(const QIcon& icon);
    void titleChanged(const QString& title);

protected:
    void setTitle(const QString& title);
    void setIcon(const QIcon& icon);

private:
    QIcon _icon;
    QString _title;
};*/

/**
 * Provides the actions associated with a session in the Konsole main menu
 * and exposes information such as the title and icon associated with the session to view containers.
 *
 * Each view should have one SessionController associated with it
 */
class SessionController : /*public ViewProperties ,*/ public QObject , public KXMLGUIClient
{
Q_OBJECT
    
public:
    /**
     * Constructs a new SessionController which operates on @p session and @p view.
     */
    SessionController(TESession* session , TEWidget* view, QObject* parent);

    /** Reimplemented to watch for events happening to the view */
    virtual bool eventFilter(QObject* watched , QEvent* event);

    TESession* session() { return _session; }
    TEWidget*  view()    { return _view;    }

signals:
    /**
     * Emitted when the view associated with the controller is focused.  
     * This can be used by other classes to plug the controller's actions into a window's
     * menus. 
     */
    void focused( SessionController* controller );

private slots:
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

private:
    void setupActions();

private:
    TESession* _session;
    TEWidget*  _view;
};

#endif //SESSIONCONTROLLER_H

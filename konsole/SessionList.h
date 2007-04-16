
#ifndef SESSIONLIST_H
#define SESSIONLIST_H

#include <QList>
#include <QObject>

class QAction;
class QActionGroup;

namespace Konsole
{

/** 
 * SessionList provides a list of actions which represent types of session that a SessionManager can 
 * create.  These actions can be plugged into a GUI 
 *
 * The user-data associated with each session can be passed to the createSession() method of the 
 * SessionManager to create a new terminal session. 
 */
class SessionList : public QObject
{
Q_OBJECT

public:
    /** Constructs a new session list which displays sessions that can be created by @p manager */
    SessionList(QObject* parent);

    /** 
     * Returns a list of actions representing the types of sessions which can be created by
     * manager().  
     * The user-data associated with each action is the string key that can be passed to
     * the manager to request creation of a new session. 
     */
    QList<QAction*> actions();

signals:
   /** 
    * Emitted when the user selects an action from the list.
    * 
    * @param key The session type key associated with the selected action.
    */
   void sessionSelected(const QString& key);
   /**
    * Emitted when the list of actions changes.
    */
   void actionsChanged(const QList<QAction*>& actions);

private slots:
    void triggered(QAction* action);
    void favoriteChanged(const QString& key , bool isFavorite);

private:
    QActionGroup*   _group;
}; 

};

#endif


#ifndef SESSIONLIST_H
#define SESSIONLIST_H

#include <QList>
#include <QObject>

class QAction;
class QActionGroup;

namespace Konsole
{

class SessionManager;

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
    SessionList(SessionManager* manager , QObject* parent);

    /** Returns the session manager */
    SessionManager* manager();

    /** 
     * Returns a list of actions representing the types of sessions which can be created by
     * manager().  
     * The user-data associated with each action is the string key that can be passed to
     * the manager to request creation of a new session. 
     */
    QList<QAction*> actions();

signals:
   void sessionSelected(const QString& key);

private slots:
    void triggered(QAction* action);

private:
    SessionManager* _manager;
    QActionGroup*   _group;
}; 

};

#endif

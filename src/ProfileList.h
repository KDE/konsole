
#ifndef SESSIONLIST_H
#define SESSIONLIST_H

#include <QList>
#include <QObject>

class QAction;
class QActionGroup;

namespace Konsole
{

class Profile;

/** 
 * ProfileList provides a list of actions which represent session profiles that a SessionManager 
 * can create a session from.  These actions can be plugged into a GUI 
 *
 * The user-data associated with each session can be passed to the createProfile() method of the 
 * SessionManager to create a new terminal session. 
 */
class ProfileList : public QObject
{
Q_OBJECT

public:
    /** Constructs a new session list which displays sessions that can be created by @p manager */
    ProfileList(QObject* parent);

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
    * @param key The profile key associated with the selected action.
    */
   void profileSelected(const QString& key);
   /**
    * Emitted when the list of actions changes.
    */
   void actionsChanged(const QList<QAction*>& actions);

private slots:
    void triggered(QAction* action);
    void favoriteChanged(const QString& key , bool isFavorite);
    void profileChanged(const QString& key);

private:
    QAction* actionForKey(const QString& key) const;
    void updateAction(QAction* action , Profile* profile);

    QActionGroup*   _group;
}; 

};

#endif

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

#ifndef PROFILELIST_H
#define PROFILELIST_H

#include <QtCore/QList>
#include <QtCore/QObject>

class QAction;
class QActionGroup;
class QKeySequence;

namespace Konsole
{

class Profile;

/** 
 * ProfileList provides a list of actions which represent session profiles 
 * that a SessionManager can create a session from.  
 *
 * These actions can be plugged into a GUI.
 *
 * Currently only profiles marked as favorites in the SessionManager are included. 
 *
 * The user-data associated with each session can be passed to the createProfile() method of the 
 * SessionManager to create a new terminal session. 
 */
class ProfileList : public QObject
{
Q_OBJECT

public:
    /** 
     * Constructs a new session list which displays sessions 
     * that can be created by @p manager 
     *
     * @param addShortcuts True if the shortcuts associated with profiles
     * in the session manager should be added to the actions
     */
    ProfileList(bool addShortcuts , QObject* parent);

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
	void shortcutChanged(const QString& key, const QKeySequence& sequence);

private:
    QAction* actionForKey(const QString& key) const;
    void updateAction(QAction* action , Profile* profile);
    void updateEmptyAction();

    QActionGroup*   _group;
    bool _addShortcuts;
    
    // action to show when the list is empty
    QAction* _emptyListAction;
}; 

}

#endif // PROFILELIST_H

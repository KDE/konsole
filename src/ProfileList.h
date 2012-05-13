/*
    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>

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

// Qt
#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QObject>

// Konsole
#include "Profile.h"
#include "konsole_export.h"

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
class KONSOLEPRIVATE_EXPORT ProfileList : public QObject
{
    Q_OBJECT

public:
    /**
     * Constructs a new profile list which displays profiles
     * that can be used to create session
     *
     * @param addShortcuts True if the shortcuts associated with profiles
     * in the session manager should be added to the actions
     * @param parent The parent GUI object
     */
    ProfileList(bool addShortcuts , QObject* parent);

    /**
     * Returns a list of actions representing profiles
     *
     * The user-data associated with each action is the corresonding profile
     */
    QList<QAction*> actions();

    /** TODO: Document me */
    void syncWidgetActions(QWidget* widget, bool sync);
signals:
    /**
     * Emitted when the user selects an action from the list.
     *
     * @param profile The profile to select
     */
    void profileSelected(Profile::Ptr profile);
    /**
     * Emitted when the list of actions changes.
     */
    void actionsChanged(const QList<QAction*>& actions);

private slots:
    void triggered(QAction* action);
    void favoriteChanged(Profile::Ptr profile, bool isFavorite);
    void profileChanged(Profile::Ptr profile);
    void shortcutChanged(Profile::Ptr profile, const QKeySequence& sequence);

private:
    QAction* actionForProfile(Profile::Ptr profile) const;
    void updateAction(QAction* action , Profile::Ptr profile);
    void updateEmptyAction();

    QActionGroup*   _group;
    bool _addShortcuts;

    // action to show when the list is empty
    QAction* _emptyListAction;
    QSet<QWidget*> _registeredWidgets;
};
}

#endif // PROFILELIST_H

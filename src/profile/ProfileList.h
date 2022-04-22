/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PROFILELIST_H
#define PROFILELIST_H

// Qt
#include <QAction>
#include <QList>
#include <QObject>
#include <QSet>

// Konsole
#include "Profile.h"
#include "konsoleprofile_export.h"

class QActionGroup;
class QKeySequence;

namespace Konsole
{
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
class KONSOLEPROFILE_EXPORT ProfileList : public QObject
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
    ProfileList(bool addShortcuts, QObject *parent);

    /**
     * Returns a list of actions representing profiles, sorted by profile
     * name.
     *
     * The user-data associated with each action is the corresponding profile.
     */
    QList<QAction *> actions();

    /** TODO: Document me */
    void syncWidgetActions(QWidget *widget, bool sync);
Q_SIGNALS:
    /**
     * Emitted when the user selects an action from the list.
     *
     * @param profile The profile to select
     */
    void profileSelected(const Profile::Ptr &profile);
    /**
     * Emitted when the list of actions changes.
     */
    void actionsChanged(const QList<QAction *> &actions);

private Q_SLOTS:
    void triggered(QAction *action);
    void profileChanged(const Profile::Ptr &profile);
    void shortcutChanged(const Profile::Ptr &profile, const QKeySequence &sequence);
    void addShortcutAction(const Profile::Ptr &profile);
    void removeShortcutAction(const Profile::Ptr &profile);

private:
    Q_DISABLE_COPY(ProfileList)

    QAction *actionForProfile(const Profile::Ptr &profile) const;
    void updateAction(QAction *action, Profile::Ptr profile);
    void updateEmptyAction();

    QActionGroup *_group;
    bool _addShortcuts;

    // action to show when the list is empty
    QAction *_emptyListAction;
    QSet<QWidget *> _registeredWidgets;
};
}

#endif // PROFILELIST_H

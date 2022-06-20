/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "ProfileList.h"

// Qt
#include <QActionGroup>
#include <QWidget>

// KDE
#include <KLocalizedString>

// Konsole
#include "ProfileManager.h"

#include <algorithm>

using Konsole::Profile;
using Konsole::ProfileList;

ProfileList::ProfileList(bool addShortcuts, QObject *parent)
    : QObject(parent)
    , _group(nullptr)
    , _addShortcuts(addShortcuts)
    , _emptyListAction(nullptr)
    , _registeredWidgets(QSet<QWidget *>())
{
    // construct the list of favorite profiles
    _group = new QActionGroup(this);

    // Even when there are no favorite profiles, allow user to
    // create new tabs using the default profile from the menu
    _emptyListAction = new QAction(i18n("Default profile"), _group);

    connect(_group, &QActionGroup::triggered, this, &ProfileList::triggered);

    for (const auto &profile : ProfileManager::instance()->allProfiles()) {
        addShortcutAction(profile);
    }

    // TODO - Handle re-sorts when user changes profile names
    ProfileManager *manager = ProfileManager::instance();
    connect(manager, &ProfileManager::shortcutChanged, this, &ProfileList::shortcutChanged);
    connect(manager, &ProfileManager::profileChanged, this, &ProfileList::profileChanged);
    connect(manager, &ProfileManager::profileRemoved, this, &ProfileList::removeShortcutAction);
    connect(manager, &ProfileManager::profileAdded, this, &ProfileList::addShortcutAction);
}

void ProfileList::updateEmptyAction()
{
    Q_ASSERT(_group);
    Q_ASSERT(_emptyListAction);

    // show this action only when it is the only action in the group
    const bool showEmptyAction = (_group->actions().count() == 1);

    if (showEmptyAction != _emptyListAction->isVisible()) {
        _emptyListAction->setVisible(showEmptyAction);
    }
}
QAction *ProfileList::actionForProfile(const Profile::Ptr &profile) const
{
    const QList<QAction *> actionsList = _group->actions();
    for (QAction *action : actionsList) {
        if (action->data().value<Profile::Ptr>() == profile) {
            return action;
        }
    }
    return nullptr; // not found
}

void ProfileList::profileChanged(const Profile::Ptr &profile)
{
    QAction *action = actionForProfile(profile);
    if (action != nullptr) {
        updateAction(action, profile);
    }
}

void ProfileList::updateAction(QAction *action, Profile::Ptr profile)
{
    Q_ASSERT(action);
    Q_ASSERT(profile);

    action->setText(profile->name());
    action->setIcon(QIcon::fromTheme(profile->icon()));

    Q_EMIT actionsChanged(actions());
}
void ProfileList::shortcutChanged(const Profile::Ptr &profile, const QKeySequence &sequence)
{
    if (!_addShortcuts) {
        return;
    }

    QAction *action = actionForProfile(profile);

    if (action != nullptr) {
        action->setShortcut(sequence);
    }
}
void ProfileList::syncWidgetActions(QWidget *widget, bool sync)
{
    if (!sync) {
        _registeredWidgets.remove(widget);
        return;
    }

    _registeredWidgets.insert(widget);

    const QList<QAction *> currentActions = widget->actions();
    for (QAction *currentAction : currentActions) {
        widget->removeAction(currentAction);
    }

    widget->addActions(actions());
}

void ProfileList::addShortcutAction(const Profile::Ptr &profile)
{
    ProfileManager *manager = ProfileManager::instance();

    auto action = new QAction(_group);
    action->setData(QVariant::fromValue(profile));

    if (_addShortcuts) {
        action->setShortcut(manager->shortcut(profile));
    }

    updateAction(action, profile);

    for (QWidget *widget : qAsConst(_registeredWidgets)) {
        widget->addAction(action);
    }
    Q_EMIT actionsChanged(actions());

    updateEmptyAction();
}

void ProfileList::removeShortcutAction(const Profile::Ptr &profile)
{
    QAction *action = actionForProfile(profile);

    if (action != nullptr) {
        _group->removeAction(action);
        for (QWidget *widget : qAsConst(_registeredWidgets)) {
            widget->removeAction(action);
        }
        Q_EMIT actionsChanged(actions());
    }
    updateEmptyAction();
}

void ProfileList::triggered(QAction *action)
{
    Q_EMIT profileSelected(action->data().value<Profile::Ptr>());
}

QList<QAction *> ProfileList::actions()
{
    auto actionsList = _group->actions();
    std::sort(actionsList.begin(), actionsList.end(), [](QAction *a, QAction *b) {
        // Remove '&', which is added by KAcceleratorManager
        const QString aName = a->text().remove(QLatin1Char('&'));
        const QString bName = b->text().remove(QLatin1Char('&'));

        if (aName == QLatin1String("Default")) {
            return true;
        } else if (bName == QLatin1String("Default")) {
            return false;
        }

        return QString::localeAwareCompare(aName, bName) < 0;
    });

    return actionsList;
}

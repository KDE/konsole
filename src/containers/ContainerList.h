/*
    SPDX-FileCopyrightText: 2026 Konsole Developers

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CONTAINERLIST_H
#define CONTAINERLIST_H

// Qt
#include <QList>
#include <QObject>

// Konsole
#include "ContainerInfo.h"

class QAction;
class QActionGroup;
class QMenu;

namespace Konsole
{

/**
 * ContainerList provides a submenu of available containers (Toolbox, Distrobox, etc.)
 * that can be used to open a new tab directly inside a container.
 *
 * The list is populated dynamically when the menu is about to be shown,
 * by calling ContainerRegistry::listAllContainers().
 *
 * This class is designed to be plugged into the "New Tab" dropdown menu
 * alongside the existing ProfileList.
 */
class ContainerList : public QObject
{
    Q_OBJECT

public:
    /**
     * Constructs a new container list.
     *
     * @param parent The parent object
     */
    explicit ContainerList(QObject *parent);

    /**
     * Returns the submenu containing the container actions.
     * The caller should add this menu as a submenu to their
     * parent menu (e.g., the "New Tab" dropdown).
     *
     * The menu is owned by this ContainerList instance.
     */
    QMenu *menu() const;

Q_SIGNALS:
    /**
     * Emitted when the user selects a container from the list.
     *
     * @param container The selected container's info
     */
    void containerSelected(const ContainerInfo &container);

private Q_SLOTS:
    void refreshContainers();
    void triggered(QAction *action);

private:
    Q_DISABLE_COPY(ContainerList)

    QMenu *_menu;
    QActionGroup *_group;
};

} // namespace Konsole

#endif // CONTAINERLIST_H

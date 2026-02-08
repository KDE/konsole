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
 * ContainerList provides a list of actions representing available containers
 * (Toolbox, Distrobox, etc.) that can be used to open a new tab directly
 * inside a container.
 *
 * Unlike a submenu, this class is designed to add its actions inline into
 * an existing menu using addSection() headers grouped by detector type,
 * e.g.:
 *
 *   ── Distrobox ──────
 *     fedora-39
 *   ── Toolbox ────────
 *     ubuntu-22
 *
 * When no containers are available, nothing is added and the menu appears
 * unchanged (no section headers, no empty state).
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
     * Returns true if container support is enabled and at least one
     * container is available.
     */
    bool hasContainers() const;

    /**
     * Refresh the cached container list from ContainerRegistry.
     *
     * This returns immediately using cached data from ContainerRegistry,
     * and also triggers an asynchronous refresh in the background so
     * that the next call will have up-to-date results.
     *
     * Call this before hasContainers() / addContainerSections() to
     * ensure the data is up-to-date.
     */
    void refreshContainers();

    /**
     * Adds per-detector container sections to the given menu.
     * Each detector's containers are preceded by an addSection() header
     * using the detector's display name (e.g., "Distrobox", "Toolbox").
     *
     * Does nothing if no containers are available.
     */
    void addContainerSections(QMenu *menu);

Q_SIGNALS:
    /**
     * Emitted when the user selects a container from the list.
     *
     * @param container The selected container's info
     */
    void containerSelected(const ContainerInfo &container);

private Q_SLOTS:
    void triggered(QAction *action);

private:
    Q_DISABLE_COPY(ContainerList)

    QActionGroup *_group;
    QList<ContainerInfo> _containers;
};

} // namespace Konsole

#endif // CONTAINERLIST_H

/*
    SPDX-FileCopyrightText: 2026 Konsole Developers

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef ICONTAINERDETECTOR_H
#define ICONTAINERDETECTOR_H

#include "ContainerInfo.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace Konsole
{

/**
 * Interface for container type detectors.
 *
 * Each implementation handles detection and entry for a specific container
 * technology (Toolbox, Distrobox, systemd-nspawn, etc.)
 */
class IContainerDetector
{
public:
    virtual ~IContainerDetector() = default;

    /**
     * Unique identifier for this container type (e.g., "toolbox", "distrobox")
     */
    virtual QString typeId() const = 0;

    /**
     * Human-readable name for display in UI (e.g., "Toolbox", "Distrobox")
     */
    virtual QString displayName() const = 0;

    /**
     * Icon name for UI representation
     */
    virtual QString iconName() const = 0;

    /**
     * Detect if the given process is running inside this container type.
     *
     * @param pid Process ID to check
     * @return ContainerInfo if detected, std::nullopt otherwise
     */
    virtual std::optional<ContainerInfo> detect(int pid) const = 0;

    /**
     * Get the command and arguments needed to enter a specific container.
     *
     * @param containerName Name of the container to enter
     * @return Command and arguments (e.g., ["toolbox", "enter", "fedora-39"])
     */
    virtual QStringList entryCommand(const QString &containerName) const = 0;

    /**
     * List all available containers of this type.
     *
     * @return List of available containers (for Phase 3 UI)
     */
    virtual QList<ContainerInfo> listContainers() const = 0;
};

} // namespace Konsole

#endif // ICONTAINERDETECTOR_H

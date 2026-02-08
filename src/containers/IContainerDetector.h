/*
    SPDX-FileCopyrightText: 2026 Konsole Developers

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef ICONTAINERDETECTOR_H
#define ICONTAINERDETECTOR_H

#include "ContainerInfo.h"

#include <QList>
#include <QObject>
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
 *
 * Container listing is asynchronous: call startListContainers() and connect
 * to the listContainersFinished() signal to receive results.
 */
class IContainerDetector : public QObject
{
    Q_OBJECT

public:
    explicit IContainerDetector(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~IContainerDetector() override = default;

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
     * Start an asynchronous listing of all available containers of this type.
     *
     * When complete, the listContainersFinished() signal is emitted with the
     * results. If the tool is not installed or fails, the signal is emitted
     * with an empty list.
     */
    virtual void startListContainers() = 0;

Q_SIGNALS:
    /**
     * Emitted when startListContainers() has finished.
     *
     * @param containers The list of discovered containers (may be empty)
     */
    void listContainersFinished(const QList<ContainerInfo> &containers);
};

} // namespace Konsole

#endif // ICONTAINERDETECTOR_H

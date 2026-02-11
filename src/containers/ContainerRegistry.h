/*
    SPDX-FileCopyrightText: 2026 Konsole Developers

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CONTAINERREGISTRY_H
#define CONTAINERREGISTRY_H

#include "ContainerInfo.h"
#include "IContainerDetector.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

namespace Konsole
{

/**
 * Singleton registry for container detection and management.
 *
 * Manages registered container detectors and provides a unified interface
 * for detecting containers, getting entry commands, and listing available
 * containers.
 *
 * Container listing is asynchronous. Call refreshContainers() to start a
 * background refresh; when all detectors have reported back, the
 * containersUpdated() signal is emitted and cachedContainers() returns
 * the new results. A refresh is also triggered automatically at startup.
 *
 * Container support is automatically disabled when running inside Flatpak,
 * as the sandboxing prevents reliable process inspection.
 */
class ContainerRegistry : public QObject
{
    Q_OBJECT

public:
    /**
     * Get the singleton instance.
     */
    static ContainerRegistry *instance();

    /**
     * Returns true if container support is enabled.
     * Container support is disabled when running inside Flatpak.
     */
    bool isEnabled() const
    {
        return _enabled;
    }

    /**
     * Returns a user-friendly explanation of why container support is disabled.
     * Empty string if container support is enabled.
     */
    QString disabledReason() const
    {
        return _disabledReason;
    }

    /**
     * Register a container detector.
     * Detectors are tried in registration order when detecting containers.
     */
    void registerDetector(std::unique_ptr<IContainerDetector> detector);

    /**
     * Detect if the given process is running inside a container.
     *
     * Tries all registered detectors in order and returns the first match.
     * Returns an invalid ContainerInfo if not in any container or if
     * container support is disabled.
     *
     * @param pid Process ID to check
     * @return ContainerInfo (check isValid() for success)
     */
    ContainerInfo detectContainer(int pid) const;

    /**
     * Get the command to enter a specific container.
     *
     * @param container Container to enter
     * @return Command and arguments, or empty list if not found
     */
    QStringList entryCommand(const ContainerInfo &container) const;

    /**
     * Returns the most recently cached list of all containers.
     *
     * This returns immediately without blocking. The list may be empty
     * if no refresh has completed yet.
     */
    QList<ContainerInfo> cachedContainers() const;

    /**
     * Start an asynchronous refresh of the container list.
     *
     * Each registered detector is asked to list its containers in the
     * background. When all detectors have finished, the cached list is
     * updated and containersUpdated() is emitted.
     *
     * If a refresh is already in progress, this call is ignored.
     */
    void refreshContainers();

    /**
     * Parse OSC 777 container parameters and return appropriate ContainerInfo.
     *
     * Handles container;push;NAME;TYPE and container;pop;; commands
     * emitted by toolbox, distrobox, and similar tools via vte.sh.
     *
     * For "push" commands, iterates through registered detectors to find
     * one with matching typeId() and uses it to build a proper ContainerInfo.
     * If no detector matches, returns a basic ContainerInfo with the provided values.
     *
     * For "pop" commands, returns an invalid (empty) ContainerInfo to clear context.
     *
     * For non-container OSC 777 commands (notify, precmd, etc.), returns nullopt
     * to indicate no container context change is needed.
     *
     * @param params The OSC 777 parameters split by semicolons
     * @return ContainerInfo if context should be updated, nullopt otherwise
     */
    std::optional<ContainerInfo> containerInfoFromOsc777(const QStringList &params) const;

    ContainerRegistry();
    ~ContainerRegistry() override = default;

Q_SIGNALS:
    /**
     * Emitted when an asynchronous refresh has completed and the
     * cached container list has been updated.
     */
    void containersUpdated();

private:
    void onDetectorFinished(const QList<ContainerInfo> &containers);

    bool _enabled = true;
    int _pendingDetectors = 0;
    QString _disabledReason;
    std::vector<std::unique_ptr<IContainerDetector>> _detectors;
    QList<ContainerInfo> _cachedContainers;
    QList<ContainerInfo> _pendingResults;

    Q_DISABLE_COPY(ContainerRegistry)
};

} // namespace Konsole

#endif // CONTAINERREGISTRY_H

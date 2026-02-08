/*
    SPDX-FileCopyrightText: 2026 Konsole Developers

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CONTAINERINFO_H
#define CONTAINERINFO_H

#include <QMetaType>
#include <QString>

#include <optional>

namespace Konsole
{

class IContainerDetector;

/**
 * Represents information about a container environment (Toolbox, Distrobox, etc.)
 */
struct ContainerInfo {
    /**
     * Pointer to the detector for this container type.
     * Null for invalid/empty containers.
     */
    const IContainerDetector *detector = nullptr;

    /** Container name/identifier */
    QString name;

    /** Human-readable display name (e.g., "Toolbox: fedora-39") */
    QString displayName;

    /** Icon name for UI display */
    QString iconName;

    /**
     * The foreground PID at the time the container was entered via OSC 777.
     * Only set for OSC 777-detected containers; nullopt for polling-detected containers.
     * Used to:
     *  - detect when the user has exited the container (foreground returns to this PID).
     *  - avoid polling-based detectors clearing OSC 777-detected contexts.
     */
    std::optional<int> hostPid;

    /** Returns true if this represents a valid container */
    bool isValid() const
    {
        return detector != nullptr && !name.isEmpty();
    }

    bool operator==(const ContainerInfo &other) const
    {
        return detector == other.detector && name == other.name;
    }

    bool operator!=(const ContainerInfo &other) const
    {
        return !(*this == other);
    }
};

} // namespace Konsole

Q_DECLARE_METATYPE(Konsole::ContainerInfo)

#endif // CONTAINERINFO_H

/*
    SPDX-FileCopyrightText: 2026 Konsole Developers

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef DISTROBOXDETECTOR_H
#define DISTROBOXDETECTOR_H

#include "IContainerDetector.h"

namespace Konsole
{

/**
 * Detector for Distrobox containers (https://distrobox.it/)
 *
 * Detection methods (in order of preference):
 * 1. Traverse /proc/<pid>/children to find podman/docker process and parse --env= args
 * 2. Check for /run/.containerenv marker file via /proc/<pid>/root/
 *    (Note: This file is also used by Podman, so we check content for distrobox specifics)
 */
class DistroboxDetector : public IContainerDetector
{
    Q_OBJECT

public:
    explicit DistroboxDetector(QObject *parent = nullptr);

    QString typeId() const override;
    QString displayName() const override;
    QString iconName() const override;

    std::optional<ContainerInfo> detect(int pid) const override;
    QStringList entryCommand(const QString &containerName) const override;
    void startListContainers() override;

private:
    /**
     * Try to detect container from podman/docker command line --env= arguments
     */
    std::optional<ContainerInfo> detectFromCmdline(int pid) const;

    /**
     * Find the deepest child process by traversing /proc/<pid>/task/<tid>/children
     * This is used to find the podman/docker process spawned by distrobox-enter
     */
    int findDeepestChild(int pid) const;

    /**
     * Build a ContainerInfo for the given container name
     */
    ContainerInfo buildContainerInfo(const QString &name) const;
};

} // namespace Konsole

#endif // DISTROBOXDETECTOR_H

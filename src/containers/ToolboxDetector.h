/*
    SPDX-FileCopyrightText: 2026 Konsole Developers

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TOOLBOXDETECTOR_H
#define TOOLBOXDETECTOR_H

#include "IContainerDetector.h"

namespace Konsole
{

/**
 * Detector for Toolbox containers (https://containertoolbx.org/)
 *
 * Container detection is handled via OSC 777 escape sequences
 * (container;push/pop) emitted by toolbox and vte.sh when entering/exiting
 * containers. The detect() method is deprecated and returns nullopt.
 *
 * This class still provides:
 * - startListContainers() - Asynchronously lists available toolbox containers
 * - entryCommand() - Returns the command to enter a container
 */
class ToolboxDetector : public IContainerDetector
{
    Q_OBJECT

public:
    explicit ToolboxDetector(QObject *parent = nullptr);

    QString typeId() const override;
    QString displayName() const override;
    QString iconName() const override;

    /**
     * @deprecated Detection now uses OSC 777 escape sequences.
     * This method always returns nullopt.
     */
    std::optional<ContainerInfo> detect(int pid) const override;

    QStringList entryCommand(const QString &containerName) const override;
    void startListContainers() override;

private:
    /**
     * Build a ContainerInfo for the given container name
     */
    ContainerInfo buildContainerInfo(const QString &name) const;
};

} // namespace Konsole

#endif // TOOLBOXDETECTOR_H

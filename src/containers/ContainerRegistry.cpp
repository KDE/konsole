/*
    SPDX-FileCopyrightText: 2026 Konsole Developers

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ContainerRegistry.h"
#include "ContainerInfo.h"
#include "DistroboxDetector.h"
#include "ToolboxDetector.h"
#include "src/konsoledebug.h"

#include <KLocalizedString>
#include <KSandbox>
#include <optional>
#include <qglobalstatic.h>

namespace Konsole
{
Q_GLOBAL_STATIC(ContainerRegistry, registry)

ContainerRegistry *ContainerRegistry::instance()
{
    return registry;
}

ContainerRegistry::ContainerRegistry()
{
    qDebug(KonsoleDebug) << "ContainerRegistry created";
    // Check for Flatpak environment - disable container support if detected
    // Note: this can do a blocking operation that checks for a `/.flatpak-info`
    //       file, which is less than ideal to do in a constructor.
    //       But Q_GLOBAL_STATIC only constructs on first use, so it should be
    //       cached long before container detection is needed.
    if (KSandbox::isFlatpak()) {
        _enabled = false;
        _disabledReason = i18n("Container support is not available when Konsole is running inside Flatpak.");
        return;
    }

    // Register built-in detectors
    // Order matters - first match wins
    registerDetector(std::make_unique<ToolboxDetector>());
    registerDetector(std::make_unique<DistroboxDetector>());
}

void ContainerRegistry::registerDetector(std::unique_ptr<IContainerDetector> detector)
{
    if (detector) {
        qDebug(KonsoleDebug) << "Registering container detector:" << detector->typeId();
        _detectors.push_back(std::move(detector));
    }
}

ContainerInfo ContainerRegistry::detectContainer(int pid) const
{
    if (!_enabled || pid <= 0) {
        return ContainerInfo{};
    }

    for (const auto &detector : _detectors) {
        auto result = detector->detect(pid);
        if (result.has_value()) {
            return result.value();
        }
    }

    return ContainerInfo{};
}

QStringList ContainerRegistry::entryCommand(const ContainerInfo &container) const
{
    if (!_enabled || !container.isValid()) {
        return QStringList{};
    }

    return container.detector->entryCommand(container.name);
}

QList<ContainerInfo> ContainerRegistry::listAllContainers() const
{
    QList<ContainerInfo> result;

    if (!_enabled) {
        return result;
    }

    for (const auto &detector : _detectors) {
        result.append(detector->listContainers());
    }

    return result;
}

std::optional<ContainerInfo> ContainerRegistry::containerInfoFromOsc777(const QStringList &params) const
{
    // Check for container command: container;push;NAME;TYPE or container;pop;;
    if (params.isEmpty() || params[0] != QLatin1String("container")) {
        return std::nullopt;
    }

    if (params.size() < 2) {
        return std::nullopt;
    }

    const QString &command = params[1];

    if (command == QLatin1String("pop")) {
        qCDebug(KonsoleDebug) << "OSC 777 container pop";
        return ContainerInfo{};
    }

    if (command != QLatin1String("push") || params.size() < 4) {
        return std::nullopt;
    }

    // container;push;NAME;TYPE
    const QString &containerName = params[2];
    const QString &containerType = params[3];

    qCDebug(KonsoleDebug) << "OSC 777 container push:" << containerName << "type:" << containerType;

    // Find a detector with matching typeId to build a proper ContainerInfo
    for (const auto &detector : _detectors) {
        if (detector->typeId() == containerType) {
            return ContainerInfo{.detector = detector.get(),
                                 .name = containerName,
                                 .displayName = QStringLiteral("%1: %2").arg(detector->displayName(), containerName),
                                 .iconName = detector->iconName(),
                                 // will get populated in Session::handleOsc777()
                                 .hostPid = std::nullopt};
        }
    }

    // No matching detector found, meaning we can't do anything useful with this info
    return ContainerInfo{};
}

} // namespace Konsole

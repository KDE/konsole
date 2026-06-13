/*
    SPDX-FileCopyrightText: 2026 Konsole Developers

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "KapsuleDetector.h"

#include <KLocalizedString>

#include <Kapsule/KapsuleClient>

#include <QCoro/QCoroTask>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <qhashfunctions.h>

namespace Konsole
{

KapsuleDetector::KapsuleDetector(QObject *parent)
    : IContainerDetector(parent)
    , _client(new Kapsule::KapsuleClient(this))
{
    connect(_client, &Kapsule::KapsuleClient::containersChanged, this, &KapsuleDetector::containersChanged);
}

QString KapsuleDetector::typeId() const
{
    return QStringLiteral("kapsule");
}

QString KapsuleDetector::displayName() const
{
    return i18n("Kapsule");
}

QString KapsuleDetector::iconName() const
{
    return QStringLiteral("utilities-terminal");
}

std::optional<ContainerInfo> KapsuleDetector::detect(int pid) const
{
    if (pid <= 0) {
        return std::nullopt;
    }

    // All Kapsule container images include a /.kapsule/ directory.
    // This is the most reliable marker that distinguishes Kapsule containers
    // from other LXC/Incus containers on the same host.
    // May change as Kapsule evolves; revisit if detection breaks.
    if (!QFileInfo(QStringLiteral("/proc/%1/root/.kapsule").arg(pid)).isDir()) {
        return std::nullopt;
    }

    // Incus sets the container hostname to the container name by default.
    QFile hostnameFile(QStringLiteral("/proc/%1/root/etc/hostname").arg(pid));
    if (!hostnameFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return buildContainerInfo(QString(), i18n("Kapsule Container"));
    }
    const QString name = QString::fromUtf8(hostnameFile.readAll()).trimmed();
    return buildContainerInfo(name, name.isEmpty() ? i18n("Kapsule Container") : name);
}

ContainerInfo KapsuleDetector::buildContainerInfo(const QString &name, const QString &displayName, const QString &icon) const
{
    return ContainerInfo{.detector = this, .name = name, .displayName = displayName, .iconName = icon.isEmpty() ? iconName() : icon, .hostPid = std::nullopt};
}

QStringList KapsuleDetector::entryCommand(const QString &containerName) const
{
    return {QStringLiteral("kapsule"), QStringLiteral("enter"), containerName};
}

void KapsuleDetector::startListContainers()
{
    fetchContainerList();
}

constexpr QLatin1StringView DEFAULT_CONTAINER_KEY("default_container");

QCoro::Task<void> KapsuleDetector::fetchContainerList()
{
    QList<ContainerInfo> results;
    const auto containers = co_await _client->listContainers();

    for (const auto &c : containers) {
        results.append(buildContainerInfo(c.name(), c.name()));
    }

    if (results.isEmpty()) {
        auto def = QStringLiteral("default");
        auto config = co_await _client->config();
        if (config.contains(DEFAULT_CONTAINER_KEY)) {
            const QString defaultContainer = config[DEFAULT_CONTAINER_KEY].toString();
            if (!defaultContainer.isEmpty()) {
                def = defaultContainer % i18n(" [default]");
            }
        }

        results.append(buildContainerInfo(QString(), def, QStringLiteral("list-add")));
    }

    Q_EMIT listContainersFinished(results);
}

} // namespace Konsole

#include "moc_KapsuleDetector.cpp"

/*
    SPDX-FileCopyrightText: 2026 Konsole Developers

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "KapsuleDetector.h"

#include <KLocalizedString>

#include <Kapsule/KapsuleClient>

#include <QCoro/QCoroTask>
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
    Q_UNUSED(pid)
    return std::nullopt;
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

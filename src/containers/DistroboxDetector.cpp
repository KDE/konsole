/*
    SPDX-FileCopyrightText: 2026 Konsole Developers

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "DistroboxDetector.h"
#include "src/konsoledebug.h"

#include <KLocalizedString>

#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>

bool isContainerRuntime(const QList<QByteArray> &args)
{
    for (const QByteArray &arg : args) {
        if (arg.endsWith("podman") || arg.endsWith("docker") || arg == "podman" || arg == "docker") {
            return true;
        }
    }
    return false;
}

std::tuple<QString, bool> parseDistroboxArgs(const QList<QByteArray> &args)
{
    QString containerName;
    bool isDistrobox = false;

    for (const QByteArray &arg : args) {
        if (arg.startsWith("--env=DISTROBOX_ENTER_PATH=")) {
            isDistrobox = true;
        }
        if (arg.startsWith("--env=CONTAINER_ID=")) {
            containerName = QString::fromUtf8(arg.mid(19)); // Skip "--env=CONTAINER_ID="
        }
        if (arg.startsWith("--env=DBX_CONTAINER_NAME=")) {
            containerName = QString::fromUtf8(arg.mid(25)); // Skip "--env=DBX_CONTAINER_NAME="
        }
    }

    return {containerName, isDistrobox};
}

const QString getContainerHostname(const QList<QByteArray> &args)
{
    for (const QByteArray &arg : args) {
        if (arg.startsWith("--env=HOSTNAME=")) {
            return QString::fromUtf8(arg.mid(15));
        }
    }
    return QString();
}

namespace Konsole
{

QString DistroboxDetector::typeId() const
{
    return QStringLiteral("distrobox");
}

QString DistroboxDetector::displayName() const
{
    return i18n("Distrobox");
}

QString DistroboxDetector::iconName() const
{
    // TODO: Find appropriate icon
    return QStringLiteral("utilities-terminal");
}

std::optional<ContainerInfo> DistroboxDetector::detect(int pid) const
{
    if (pid <= 0) {
        return std::nullopt;
    }

    // The distrobox-enter script spawns podman/docker with --env= arguments
    // We need to find the deepest child process (podman/docker) and parse its cmdline
    int containerPid = findDeepestChild(pid);
    if (containerPid <= 0) {
        return std::nullopt;
    }

    return detectFromCmdline(containerPid);
}

std::optional<ContainerInfo> DistroboxDetector::detectFromCmdline(int pid) const
{
    const QString cmdlinePath = QStringLiteral("/proc/%1/cmdline").arg(pid);
    QFile cmdlineFile(cmdlinePath);

    if (!cmdlineFile.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }

    // Command line arguments are null-separated in /proc/pid/cmdline
    const QByteArray cmdlineData = cmdlineFile.readAll();
    const QList<QByteArray> args = cmdlineData.split('\0');

    if (!isContainerRuntime(args)) {
        return std::nullopt;
    }

    auto [containerName, isDistrobox] = parseDistroboxArgs(args);

    if (!isDistrobox) {
        return std::nullopt;
    }

    if (containerName.isEmpty()) {
        qDebug(KonsoleDebug) << "Distrobox detector: container name not found in arguments. Checking HOSTNAME...";
        containerName = getContainerHostname(args);
    }

    if (containerName.isEmpty()) {
        qDebug(KonsoleDebug) << "Distrobox detector: container name still not found. Detection failed.";
        return std::nullopt;
    }

    qDebug(KonsoleDebug) << "Distrobox container detected (from HOSTNAME):" << containerName;
    return buildContainerInfo(containerName);
}

int DistroboxDetector::findDeepestChild(int pid) const
{
    // Read /proc/<pid>/task/<tid>/children to find child processes
    // We traverse down the tree to find the deepest child (usually podman/docker)
    const QString childrenPath = QStringLiteral("/proc/%1/task/%1/children").arg(pid);
    QFile childrenFile(childrenPath);

    if (!childrenFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return -1;
    }

    const QString content = QString::fromUtf8(childrenFile.readAll()).trimmed();
    if (content.isEmpty()) {
        // No children, this is the deepest process
        return pid;
    }

    // Children are space-separated PIDs
    const QStringList childPids = content.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (childPids.isEmpty()) {
        return pid;
    }

    // Take the first child and recurse (distrobox typically has a linear process tree)
    bool ok;
    int childPid = childPids.first().toInt(&ok);
    if (!ok || childPid <= 0) {
        return pid;
    }

    int deepest = findDeepestChild(childPid);
    return (deepest > 0) ? deepest : childPid;
}

ContainerInfo DistroboxDetector::buildContainerInfo(const QString &name) const
{
    return ContainerInfo{.detector = this,
                         .name = name,
                         .displayName = i18n("Distrobox: %1", name),
                         .iconName = iconName(),
                         // only used when entering via OSC777
                         .hostPid = std::nullopt};
}

QStringList DistroboxDetector::entryCommand(const QString &containerName) const
{
    return {QStringLiteral("distrobox"), QStringLiteral("enter"), containerName};
}

QList<ContainerInfo> DistroboxDetector::listContainers() const
{
    QList<ContainerInfo> containers;

    // Run: distrobox list --no-color
    QProcess process;
    process.setProgram(QStringLiteral("distrobox"));
    process.setArguments({QStringLiteral("list"), QStringLiteral("--no-color")});
    process.start();

    if (!process.waitForFinished(5000)) {
        return containers;
    }

    if (process.exitCode() != 0) {
        return containers;
    }

    // Parse output - format is typically:
    // ID           | NAME                 | STATUS          | IMAGE
    // abc123def456 | ubuntu-22            | Up 2 hours      | ubuntu:22.04
    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    // Skip header line
    for (int i = 1; i < lines.size(); ++i) {
        const QString &line = lines[i];
        // Split by |, container name is second column
        const QStringList columns = line.split(QLatin1Char('|'));
        if (columns.size() >= 2) {
            const QString containerName = columns[1].trimmed();
            if (!containerName.isEmpty()) {
                containers.append(buildContainerInfo(containerName));
            }
        }
    }

    return containers;
}

} // namespace Konsole

/*
    SPDX-FileCopyrightText: 2026 Konsole Developers

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "DistroboxDetector.h"
#include "DistroboxListParser.h"
#include "konsoledebug.h"

#include <KLocalizedString>
#include <KSandbox>

#include <QFile>
#include <QProcess>
#include <QSharedPointer>
#include <QTextStream>
#include <QStandardPaths>

namespace Konsole
{
static bool hasDistroboxMarkers(const QHash<QByteArray, QByteArray> &env)
{
    return env.contains("DISTROBOX_ENTER_PATH") || env.contains("DBX_CONTAINER_NAME") || env.contains("DISTROBOX_HOST_HOME")
        || env.contains("CONTAINER_ID");
}

static QString containerNameFromDistroboxEnv(const QHash<QByteArray, QByteArray> &env)
{
    QString containerName = QString::fromUtf8(env.value("DBX_CONTAINER_NAME"));
    if (containerName.isEmpty()) {
        containerName = QString::fromUtf8(env.value("DISTROBOX_CONTAINER_NAME"));
    }
    if (containerName.isEmpty()) {
        containerName = QString::fromUtf8(env.value("CONTAINER_ID"));
    }
    return containerName;
}

static bool isContainerRuntime(const QList<QByteArray> &args)
{
    for (const QByteArray &arg : args) {
        if (arg.endsWith("podman") || arg.endsWith("docker") || arg == "podman" || arg == "docker") {
            return true;
        }
    }
    return false;
}

static std::tuple<QString, bool> parseDistroboxArgs(const QList<QByteArray> &args)
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

static QHash<QByteArray, QByteArray> readProcEnvironment(int pid)
{
    QHash<QByteArray, QByteArray> env;
    QFile file(QStringLiteral("/proc/%1/environ").arg(pid));
    if (!file.open(QIODevice::ReadOnly)) {
        return env;
    }

    const QList<QByteArray> entries = file.readAll().split('\0');
    for (const QByteArray &entry : entries) {
        const int sep = entry.indexOf('=');
        if (sep <= 0) {
            continue;
        }
        env.insert(entry.left(sep), entry.mid(sep + 1));
    }
    return env;
}

static QString readContainerNameFromContainerEnv(int pid)
{
    QFile file(QStringLiteral("/proc/%1/root/run/.containerenv").arg(pid));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (!line.startsWith(QStringLiteral("name="))) {
            continue;
        }
        QString value = line.mid(5).trimmed();
        if (value.size() >= 2 && value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"'))) {
            value = value.mid(1, value.size() - 2);
        }
        return value;
    }

    return {};
}

DistroboxDetector::DistroboxDetector(QObject *parent)
    : IContainerDetector(parent)
{
}

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

    // Fast-path for sessions started already inside distrobox.
    const auto env = readProcEnvironment(pid);
    if (hasDistroboxMarkers(env)) {
        QString containerName = containerNameFromDistroboxEnv(env);
        if (containerName.isEmpty()) {
            containerName = readContainerNameFromContainerEnv(pid);
        }
        if (!containerName.isEmpty()) {
            return buildContainerInfo(containerName);
        }
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
        qDebug(KonsoleDebug) << "Distrobox detector: container name not found in arguments. Checking /run/.containerenv...";
        containerName = readContainerNameFromContainerEnv(pid);
    }

    if (containerName.isEmpty()) {
        qDebug(KonsoleDebug) << "Distrobox detector: container name still not found. Detection failed.";
        return std::nullopt;
    }

    qDebug(KonsoleDebug) << "Distrobox container detected:" << containerName;
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
                         .displayName = name,
                         .iconName = iconName(),
                         // only used when entering via OSC777
                         .hostPid = std::nullopt};
}

QStringList DistroboxDetector::entryCommand(const QString &containerName) const
{
    return {QStringLiteral("distrobox"), QStringLiteral("enter"), containerName};
}

void DistroboxDetector::startListContainers()
{
    // Return if distrobox is not found
    if (QStandardPaths::findExecutable(QStringLiteral("distrobox")).isEmpty()) {
        Q_EMIT listContainersFinished({});
        return;
    }
    auto *process = new QProcess(this);
    process->setProgram(QStringLiteral("distrobox"));
    process->setArguments({QStringLiteral("list")});
    auto done = QSharedPointer<bool>::create(false);

    connect(process, &QProcess::finished, this, [this, process, done](int exitCode, QProcess::ExitStatus exitStatus) {
        if (*done) {
            return;
        }
        *done = true;
        QList<ContainerInfo> containers;
        process->deleteLater();

        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            Q_EMIT listContainersFinished(containers);
            return;
        }

        const QString output = QString::fromUtf8(process->readAllStandardOutput());
        const QStringList names = parseDistroboxContainerNames(output);
        for (const QString &name : names) {
            containers.append(buildContainerInfo(name));
        }

        Q_EMIT listContainersFinished(containers);
    });

    connect(process, &QProcess::errorOccurred, this, [this, process, done](QProcess::ProcessError) {
        if (*done) {
            return;
        }
        *done = true;
        process->deleteLater();
        Q_EMIT listContainersFinished({});
    });

    KSandbox::startHostProcess(*process, QProcess::ReadOnly);
}

} // namespace Konsole

#include "moc_DistroboxDetector.cpp"

/*
    SPDX-FileCopyrightText: 2026 Konsole Developers

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ToolboxDetector.h"

#include <KLocalizedString>

#include <QProcess>
#include <QRegularExpression>

namespace Konsole
{

QString ToolboxDetector::typeId() const
{
    return QStringLiteral("toolbox");
}

QString ToolboxDetector::displayName() const
{
    return i18n("Toolbox");
}

QString ToolboxDetector::iconName() const
{
    // TODO: Find appropriate icon, perhaps "utilities-terminal" or custom
    return QStringLiteral("utilities-terminal");
}

std::optional<ContainerInfo> ToolboxDetector::detect(int pid) const
{
    Q_UNUSED(pid)
    // Detection is handled via OSC 777 escape sequences (container;push/pop)
    // emitted by toolbox/vte.sh when entering/exiting containers.
    // Since toolbox has been emitting these for a while, it didn't make
    // sense to build a process inspection-based detection here.
    return std::nullopt;
}

ContainerInfo ToolboxDetector::buildContainerInfo(const QString &name) const
{
    return ContainerInfo{.detector = this, .name = name, .displayName = i18n("Toolbox: %1", name), .iconName = iconName(), .hostPid = std::nullopt};
}

QStringList ToolboxDetector::entryCommand(const QString &containerName) const
{
    return {QStringLiteral("toolbox"), QStringLiteral("enter"), containerName};
}

QList<ContainerInfo> ToolboxDetector::listContainers() const
{
    QList<ContainerInfo> containers;

    // Run: toolbox list --containers
    QProcess process;
    process.setProgram(QStringLiteral("toolbox"));
    process.setArguments({QStringLiteral("list"), QStringLiteral("--containers")});
    process.start();

    if (!process.waitForFinished(5000)) {
        return containers;
    }

    if (process.exitCode() != 0) {
        return containers;
    }

    // Parse output - format is typically:
    // CONTAINER ID  CONTAINER NAME  CREATED       STATUS   IMAGE NAME
    // abc123def456  fedora-39       2 weeks ago   running  registry.fedoraproject.org/fedora-toolbox:39
    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    // Skip header line
    for (int i = 1; i < lines.size(); ++i) {
        const QString &line = lines[i];
        // Split by whitespace, container name is second column
        static QRegularExpression whitespace(QStringLiteral("\\s+"));
        const QStringList columns = line.split(whitespace, Qt::SkipEmptyParts);
        if (columns.size() >= 2) {
            const QString containerName = columns[1];
            containers.append(buildContainerInfo(containerName));
        }
    }

    return containers;
}

} // namespace Konsole

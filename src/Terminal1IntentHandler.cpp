/*
 *    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *    SPDX-FileCopyrightText: 2026 David Redondo <kde@david-redondo.de>
 */

#include "Terminal1IntentHandler.h"

#if HAVE_DBUS
#include <QDBusMetaType>
#include <QDebug>
#include <QProcessEnvironment>
#include <QVariant>

#include <ranges>

using namespace Qt::StringLiterals;

QDebug operator<<(QDebug debug, const Terminal1IntentHandler::Command &c)
{
    QDebugStateSaver saver(debug);
    return debug << c.exec << " working_dir:" << c.workingDir << " env:" << c.env;
}

void Terminal1IntentHandler::LaunchCommand(const QList<QVariantMap> &commands,
                                           const QByteArray &desktopEntry,
                                           const QVariantMap &options,
                                           const QVariantMap &platformData)
{
    auto parseCommand = [](const QVariantMap &command) {
        const auto exec = qdbus_cast<QList<QByteArray>>(command.value(u"exec"_s).value<QDBusArgument>()) | std::views::transform([](const auto &bytes) {
                              return QString::fromUtf8(bytes);
                          });
        const auto workingDir = QString::fromUtf8(command.value(u"working_directory"_s).toByteArray());
        const auto rawEnv = qdbus_cast<QList<QByteArray>>(command.value(u"env"_s).value<QDBusArgument>());
        const auto env = rawEnv | std::views::transform([](const auto &bytes) {
                             return QString::fromUtf8(bytes);
                         });
        return Command{.exec = {exec.begin(), exec.end()}, .env = {env.begin(), env.end()}, .workingDir = workingDir};
    };

    auto parsedCommandsView = commands | std::views::transform(parseCommand);
    QList<Command> parsedCommands(parsedCommandsView.begin(), parsedCommandsView.end());

    qDebug() << "LaunchCommand" << parsedCommands << desktopEntry << options << platformData;

    Q_EMIT LaunchCommandRequested(parsedCommands, QString::fromUtf8(desktopEntry), options, platformData);
}

#include "moc_Terminal1IntentHandler.cpp"
#endif

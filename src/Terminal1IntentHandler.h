/*
 *    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 *    SPDX-FileCopyrightText: 2026 David Redondo <kde@david-redondo.de>
 */

#pragma once

#include "config-konsole.h"
#include "konsoleapp_export.h"

#if HAVE_DBUS
#include <QDBusAbstractAdaptor>
#include <QDBusMetaType>
#include <QProcessEnvironment>

class KONSOLEAPP_EXPORT Terminal1IntentHandler : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Terminal1")
public:
    Terminal1IntentHandler(QObject *parent)
        : QDBusAbstractAdaptor(parent)
    {
        qDBusRegisterMetaType<QList<QVariantMap>>();
        qDBusRegisterMetaType<QList<QByteArray>>();
    }
    ~Terminal1IntentHandler() override = default;
    struct Command {
        QList<QString> exec;
        QHash<QString, QString> envTweaks;
        QString workingDir;
    };

public Q_SLOTS:
    void LaunchCommand(const QList<QVariantMap> &commands, const QByteArray &desktop_entry, const QVariantMap &options, const QVariantMap &platformData);
Q_SIGNALS:
    // TODO this is not clean this signal leaks to dbus, but if we give it an unregistered type parameter it doesn't
    void LaunchCommandRequested(const QList<Command> &commands, const QString &desktopEntry, const QVariantMap &options, const QVariantMap &platformData);
};
#endif

#pragma once

#include <config-konsole.h>

#include <QDBusAbstractAdaptor>
#include <qdbusabstractadaptor.h>

#if HAVE_DBUS
class Terminal1IntentHandler : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Terminal1")
public:
    Terminal1IntentHandler(QObject *parent)
        : QDBusAbstractAdaptor(parent)
    {
    }
    ~Terminal1IntentHandler() override = default;
public Q_SLOTS:
    void LaunchCommand(const QList<QVariantMap> &commands, const QByteArray &workingDir, const QVariantMap &options, const QVariantMap &platformData);
Q_SIGNALS:
    // TODO this is not clean this signal leaks to dbus
    void LaunchCommandRequested();
};
#endif

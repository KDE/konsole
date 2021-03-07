#include "sshmanagerplugin.h"

K_PLUGIN_CLASS_WITH_JSON(SSHManagerPlugin, "konsole_sshmanager.json")

SSHManagerPlugin::SSHManagerPlugin(QObject *object, const QVariantList &args) :
Konsole::IKonsolePlugin(object, args)
{
    setName(QStringLiteral("SshManager"));
}

#include "sshmanagerplugin.moc"

#include "sshmanagerplugin.h"

#include "sshmanagerpluginwidget.h"

#include <QMainWindow>
#include <QDockWidget>
#include <QListView>

#include <KLocalizedString>

K_PLUGIN_CLASS_WITH_JSON(SSHManagerPlugin, "konsole_sshmanager.json")

SSHManagerPlugin::SSHManagerPlugin(QObject *object, const QVariantList &args) :
Konsole::IKonsolePlugin(object, args)
{
    setName(QStringLiteral("SshManager"));
}

void SSHManagerPlugin::createWidgetsForMainWindow(QMainWindow *mainWindow)
{
    auto *sshDockWidget = new QDockWidget(mainWindow);
    auto *managerWidget = new SSHManagerTreeWidget();
    sshDockWidget->setWidget(managerWidget);
    sshDockWidget->setWindowTitle(i18n("SSH Manager"));
    sshDockWidget->setObjectName(QStringLiteral("SSHManagerDock"));
    mainWindow->addDockWidget(Qt::LeftDockWidgetArea, sshDockWidget);
}

#include "sshmanagerplugin.moc"

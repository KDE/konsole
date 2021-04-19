#include "sshmanagerplugin.h"

#include "sshmanagerpluginwidget.h"
#include "sshmanagermodel.h"

#include "session/SessionController.h"

#include <QMainWindow>
#include <QDockWidget>
#include <QListView>

#include <KLocalizedString>

K_PLUGIN_CLASS_WITH_JSON(SSHManagerPlugin, "konsole_sshmanager.json")

struct SSHManagerPlugin::Private {
    SSHManagerModel model;
    SSHManagerTreeWidget *window = nullptr;
};

SSHManagerPlugin::SSHManagerPlugin(QObject *object, const QVariantList &args)
: Konsole::IKonsolePlugin(object, args)
, d(std::make_unique<SSHManagerPlugin::Private>())
{
    setName(QStringLiteral("SshManager"));
}

SSHManagerPlugin::~SSHManagerPlugin() = default;

void SSHManagerPlugin::createWidgetsForMainWindow(QMainWindow *mainWindow)
{
    auto *sshDockWidget = new QDockWidget(mainWindow);
    auto *managerWidget = new SSHManagerTreeWidget();
    managerWidget->setModel(&d->model);
    sshDockWidget->setWidget(managerWidget);
    sshDockWidget->setWindowTitle(i18n("SSH Manager"));
    sshDockWidget->setObjectName(QStringLiteral("SSHManagerDock"));
    mainWindow->addDockWidget(Qt::LeftDockWidgetArea, sshDockWidget);
    d->window = managerWidget;
}

void SSHManagerPlugin::sessionControllerChanged(Konsole::SessionController *controller)
{
    d->window->setCurrentController(controller);
}

#include "sshmanagerplugin.moc"

/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sshmanagerplugin.h"

#include "sshmanagermodel.h"
#include "sshmanagerpluginwidget.h"

#include "konsoledebug.h"
#include "session/SessionController.h"

#include <QDockWidget>
#include <QListView>
#include <QMainWindow>
#include <QMenuBar>
#include <QTimer>

#include <KActionCollection>
#include <KLocalizedString>

#include "MainWindow.h"

K_PLUGIN_CLASS_WITH_JSON(SSHManagerPlugin, "konsole_sshmanager.json")

struct SSHManagerPluginPrivate {
    SSHManagerModel model;

    QMap<Konsole::MainWindow *, SSHManagerTreeWidget *> widgetForWindow;
    QMap<Konsole::MainWindow *, QDockWidget *> dockForWindow;
};

SSHManagerPlugin::SSHManagerPlugin(QObject *object, const QVariantList &args)
    : Konsole::IKonsolePlugin(object, args)
    , d(std::make_unique<SSHManagerPluginPrivate>())
{
    setName(QStringLiteral("SshManager"));
}

SSHManagerPlugin::~SSHManagerPlugin() = default;

void SSHManagerPlugin::createWidgetsForMainWindow(Konsole::MainWindow *mainWindow)
{
    auto *sshDockWidget = new QDockWidget(mainWindow);
    auto *managerWidget = new SSHManagerTreeWidget();
    managerWidget->setModel(&d->model);
    sshDockWidget->setWidget(managerWidget);
    sshDockWidget->setWindowTitle(i18n("SSH Manager"));
    sshDockWidget->setObjectName(QStringLiteral("SSHManagerDock"));
    sshDockWidget->setVisible(false);

    mainWindow->addDockWidget(Qt::LeftDockWidgetArea, sshDockWidget);

    d->widgetForWindow[mainWindow] = managerWidget;
    d->dockForWindow[mainWindow] = sshDockWidget;

    connect(managerWidget, &SSHManagerTreeWidget::requestNewTab, this, [mainWindow] {
        mainWindow->newTab();
    });
}

QList<QAction *> SSHManagerPlugin::menuBarActions(Konsole::MainWindow *mainWindow) const
{
    Q_UNUSED(mainWindow);

    QAction *toggleVisibilityAction = new QAction(i18n("Show SSH Manager"), mainWindow);
    toggleVisibilityAction->setCheckable(true);

    connect(toggleVisibilityAction, &QAction::triggered, d->dockForWindow[mainWindow], &QDockWidget::setVisible);
    connect(d->dockForWindow[mainWindow], &QDockWidget::visibilityChanged, toggleVisibilityAction, &QAction::setChecked);

    return {toggleVisibilityAction};
}

void SSHManagerPlugin::activeViewChanged(Konsole::SessionController *controller, Konsole::MainWindow *mainWindow)
{
    Q_ASSERT(controller);
    Q_ASSERT(mainWindow);

    if (mainWindow) {
        d->widgetForWindow[mainWindow]->setCurrentController(controller);
    }
}

#include "sshmanagerplugin.moc"

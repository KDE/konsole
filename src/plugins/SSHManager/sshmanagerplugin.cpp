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

#include <KLocalizedString>

#include "MainWindow.h"
#include "sshquickaccesswidget.h"
#include "terminalDisplay/TerminalDisplay.h"

K_PLUGIN_CLASS_WITH_JSON(SSHManagerPlugin, "konsole_sshmanager.json")

struct SSHManagerPluginPrivate {
    SSHManagerModel model;

    QMap<Konsole::MainWindow *, SSHManagerTreeWidget *> widgetForWindow;
    QMap<Konsole::MainWindow *, QDockWidget *> dockForWindow;
    SSHQuickAccessWidget *quickAccess = nullptr;
    QAction *showQuickAccess = nullptr;
};

SSHManagerPlugin::SSHManagerPlugin(QObject *object, const QVariantList &args)
    : Konsole::IKonsolePlugin(object, args)
    , d(std::make_unique<SSHManagerPluginPrivate>())
{
    d->quickAccess = new SSHQuickAccessWidget(&d->model);
    connect(d->quickAccess, &QObject::destroyed, this, [this] {
        // quick access can be destroyed if a terminal display is destroyed.
        // so, just create a new one.
        d->quickAccess = new SSHQuickAccessWidget(&d->model);
    });

    d->showQuickAccess = new QAction();

    setName(QStringLiteral("SshManager"));
}

SSHManagerPlugin::~SSHManagerPlugin()
{
    disconnect(d->quickAccess);
}

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
    toggleVisibilityAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_F2));
    connect(toggleVisibilityAction, &QAction::triggered, d->dockForWindow[mainWindow], &QDockWidget::setVisible);
    connect(d->dockForWindow[mainWindow], &QDockWidget::visibilityChanged, toggleVisibilityAction, &QAction::setChecked);

    return {toggleVisibilityAction};
}

#include <iostream>

void SSHManagerPlugin::activeViewChanged(Konsole::SessionController *controller, Konsole::MainWindow *mainWindow)
{
    Q_ASSERT(controller);
    Q_ASSERT(mainWindow);

    auto terminalDisplay = controller->view();

    d->quickAccess->hide();
    d->quickAccess->setParent(terminalDisplay);
    d->quickAccess->setSessionController(controller);

    d->showQuickAccess->deleteLater();
    d->showQuickAccess = new QAction(i18n("Show Quick Access for SSH Actions"));

    d->showQuickAccess->setShortcut(QKeySequence(Qt::CTRL + Qt::ALT + Qt::Key_H));
    terminalDisplay->addAction(d->showQuickAccess);

    connect(d->showQuickAccess, &QAction::triggered, this, [this, terminalDisplay] {
        d->quickAccess->show();
        d->quickAccess->setFocus();
        terminalDisplay->installEventFilter(d->quickAccess);
    });

    if (mainWindow) {
        d->widgetForWindow[mainWindow]->setCurrentController(controller);
    }
}

#include "sshmanagerplugin.moc"

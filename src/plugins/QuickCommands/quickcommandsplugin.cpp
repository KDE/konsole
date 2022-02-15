// This file was part of the KDE libraries
// SPDX-FileCopyrightText: 2022 Tao Guo <guotao945@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "quickcommandsplugin.h"
#include "konsoledebug.h"
#include "quickcommandsmodel.h"
#include "quickcommandswidget.h"

#include <KActionCollection>
#include <QMainWindow>

#include "MainWindow.h"
#include <KLocalizedString>

K_PLUGIN_CLASS_WITH_JSON(QuickCommandsPlugin, "konsole_quickcommands.json")

struct QuickCommandsPlugin::Private {
    QuickCommandsModel model;

    QMap<Konsole::MainWindow *, QuickCommandsWidget *> widgetForWindow;
    QMap<Konsole::MainWindow *, QDockWidget *> dockForWindow;
};

QuickCommandsPlugin::QuickCommandsPlugin(QObject *object, const QVariantList &args)
    : Konsole::IKonsolePlugin(object, args)
    , priv(std::make_unique<Private>())
{
    setName(QStringLiteral("QuickCommands"));
}

QuickCommandsPlugin::~QuickCommandsPlugin() = default;

void QuickCommandsPlugin::createWidgetsForMainWindow(Konsole::MainWindow *mainWindow)
{
    auto *qcDockWidget = new QDockWidget(mainWindow);
    auto *qcWidget = new QuickCommandsWidget(mainWindow);
    qcWidget->setModel(&priv->model);
    qcDockWidget->setWindowTitle(i18n("Quick Commands"));
    qcDockWidget->setWidget(qcWidget);
    qcDockWidget->setObjectName(QStringLiteral("QuickCommandsDock"));
    qcDockWidget->setVisible(false);

    mainWindow->addDockWidget(Qt::LeftDockWidgetArea, qcDockWidget);

    priv->widgetForWindow[mainWindow] = qcWidget;
    priv->dockForWindow[mainWindow] = qcDockWidget;
}

void QuickCommandsPlugin::activeViewChanged(Konsole::SessionController *controller, Konsole::MainWindow *mainWindow)
{
    if (mainWindow)
        priv->widgetForWindow[mainWindow]->setCurrentController(controller);
}

QList<QAction *> QuickCommandsPlugin::menuBarActions(Konsole::MainWindow *mainWindow) const
{
    QAction *toggleVisibilityAction = new QAction(i18n("Show Quick Commands"), mainWindow);
    toggleVisibilityAction->setCheckable(true);

    connect(toggleVisibilityAction, &QAction::triggered, priv->dockForWindow[mainWindow], &QDockWidget::setVisible);
    connect(priv->dockForWindow[mainWindow], &QDockWidget::visibilityChanged, toggleVisibilityAction, &QAction::setChecked);

    return {toggleVisibilityAction};
}

#include "quickcommandsplugin.moc"

// This file was part of the KDE libraries
// SPDX-FileCopyrightText: 2022 Tao Guo <guotao945@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "quickcommandsplugin.h"
#include "konsoledebug.h"
#include "quickcommandsmodel.h"
#include "quickcommandswidget.h"

#include <KActionCollection>
#include <KCommandBar>
#include <QMainWindow>
#include <QSettings>

#include "MainWindow.h"
#include <KLocalizedString>
#include <KMessageBox>

#include <QDockWidget>

K_PLUGIN_CLASS_WITH_JSON(QuickCommandsPlugin, "konsole_quickcommands.json")

struct QuickCommandsPlugin::Private {
    QuickCommandsModel model;
    QAction *showQuickAccess = nullptr;
    QMap<Konsole::MainWindow *, QuickCommandsWidget *> widgetForWindow;
    QMap<Konsole::MainWindow *, QDockWidget *> dockForWindow;
};

QuickCommandsPlugin::QuickCommandsPlugin(QObject *object, const QVariantList &args)
    : Konsole::IKonsolePlugin(object, args)
    , priv(std::make_unique<Private>())
{
    priv->showQuickAccess = new QAction();
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
    qcDockWidget->setAllowedAreas(Qt::DockWidgetArea::LeftDockWidgetArea | Qt::DockWidgetArea::RightDockWidgetArea);

    mainWindow->addDockWidget(Qt::LeftDockWidgetArea, qcDockWidget);
    connect(qcWidget, &QuickCommandsWidget::quickAccessShortcutChanged, this, [this, mainWindow](QKeySequence s) {
        mainWindow->actionCollection()->setDefaultShortcut(priv->showQuickAccess, s);

        QString sequenceText = s.toString();
        QSettings settings;
        settings.beginGroup(QStringLiteral("plugins"));
        settings.beginGroup(QStringLiteral("quickcommands"));
        settings.setValue(QStringLiteral("shortcut"), sequenceText);
        settings.sync();
    });

    priv->widgetForWindow[mainWindow] = qcWidget;
    priv->dockForWindow[mainWindow] = qcDockWidget;
}

void QuickCommandsPlugin::activeViewChanged(Konsole::SessionController *controller, Konsole::MainWindow *mainWindow)
{
    priv->showQuickAccess->deleteLater();
    priv->showQuickAccess = new QAction(i18n("Show Quick Access"));

    QSettings settings;
    settings.beginGroup(QStringLiteral("plugins"));
    settings.beginGroup(QStringLiteral("quickcommands"));

    const QKeySequence def(Qt::CTRL | Qt::ALT | Qt::Key_G);
    const QString defText = def.toString();
    const QString entry = settings.value(QStringLiteral("shortcut"), defText).toString();
    const QKeySequence shortcutEntry(entry);

    mainWindow->actionCollection()->setDefaultShortcut(priv->showQuickAccess, shortcutEntry);

    controller->view()->addAction(priv->showQuickAccess);

    Konsole::TerminalDisplay *terminalDisplay = controller->view();
    connect(priv->showQuickAccess, &QAction::triggered, this, [this, terminalDisplay, controller] {
        KCommandBar bar(terminalDisplay->topLevelWidget());
        QList<QAction *> actions;
        for (int i = 0; i < priv->model.rowCount(); i++) {
            QModelIndex folder = priv->model.index(i, 0);
            for (int e = 0; e < priv->model.rowCount(folder); e++) {
                QModelIndex idx = priv->model.index(e, 0, folder);
                QAction *act = new QAction(idx.data().toString());
                connect(act, &QAction::triggered, this, [this, idx, controller] {
                    const auto item = priv->model.itemFromIndex(idx);
                    const auto data = item->data(QuickCommandsModel::QuickCommandRole).value<QuickCommandData>();
                    controller->session()->sendTextToTerminal(data.command, QLatin1Char('\r'));
                });
                actions.append(act);
            }
        }

        if (actions.isEmpty()) // no quick commands found, must give feedback to the user about that
        {
            const QString feedbackMessage = i18n("No quick commands found. You can add one on Plugins -> Quick Commands");
            const QString feedbackTitle = i18n("Plugins - Quick Commands");
            KMessageBox::error(terminalDisplay->topLevelWidget(), feedbackMessage, feedbackTitle);
            return;
        }

        QVector<KCommandBar::ActionGroup> groups;
        groups.push_back(KCommandBar::ActionGroup{i18n("Quick Commands"), actions});
        bar.setActions(groups);
        bar.exec();
    });

    if (mainWindow) {
        priv->widgetForWindow[mainWindow]->setCurrentController(controller);
    }
}

QList<QAction *> QuickCommandsPlugin::menuBarActions(Konsole::MainWindow *mainWindow) const
{
    QAction *toggleVisibilityAction = new QAction(i18n("Show Quick Commands"), mainWindow);
    toggleVisibilityAction->setCheckable(true);
    mainWindow->actionCollection()->setDefaultShortcut(toggleVisibilityAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F1));
    connect(toggleVisibilityAction, &QAction::triggered, priv->dockForWindow[mainWindow], &QDockWidget::setVisible);
    connect(priv->dockForWindow[mainWindow], &QDockWidget::visibilityChanged, toggleVisibilityAction, &QAction::setChecked);

    return {toggleVisibilityAction};
}

#include "quickcommandsplugin.moc"

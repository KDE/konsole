// This file was part of the KDE libraries
// SPDX-FileCopyrightText: 2022 Tao Guo <guotao945@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef QUICKCOMMANDSPLUGIN_H
#define QUICKCOMMANDSPLUGIN_H

#include <pluginsystem/IKonsolePlugin.h>

namespace Konsole
{
class SessionController;
class MainWindow;
}

class QuickCommandsPlugin : public Konsole::IKonsolePlugin
{
    Q_OBJECT

public:
    QuickCommandsPlugin(QObject *object, const QVariantList &args);
    ~QuickCommandsPlugin() override;

    void createWidgetsForMainWindow(Konsole::MainWindow *mainWindow) override;
    void activeViewChanged(Konsole::SessionController *controller, Konsole::MainWindow *mainWindow) override;
    QList<QAction *> menuBarActions(Konsole::MainWindow *mainWindow) const override;

private:
    struct Private;
    std::unique_ptr<Private> priv;
};

#endif // QUICKCOMMANDSPLUGIN_H

/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SSHMANAGERPLUGIN_H
#define SSHMANAGERPLUGIN_H

#include <pluginsystem/IKonsolePlugin.h>

#include <memory>

namespace Konsole
{
class SessionController;
class MainWindow;
}

struct SSHManagerPluginPrivate;

class SSHManagerPlugin : public Konsole::IKonsolePlugin
{
    Q_OBJECT
public:
    SSHManagerPlugin(QObject *object, const QVariantList &args);
    ~SSHManagerPlugin() override;

    void createWidgetsForMainWindow(Konsole::MainWindow *mainWindow) override;
    void activeViewChanged(Konsole::SessionController *controller) override;
    QList<QAction *> menuBarActions(Konsole::MainWindow *mainWindow) const override;

private Q_SLOTS:
    void activeViewChangedInternal(QPointer<Konsole::SessionController> controller);

private:
    std::unique_ptr<SSHManagerPluginPrivate> d;
};

#endif

/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sshmanagerplugin.h"

#include "sshmanagermodel.h"
#include "sshmanagerpluginwidget.h"

#include "ProcessInfo.h"
#include "konsoledebug.h"
#include "session/SessionController.h"
#include "session/Session.h"

#include <QDockWidget>
#include <QListView>
#include <QMainWindow>
#include <QMenuBar>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTimer>

#include <KActionCollection>
#include <KCommandBar>
#include <KCrash>
#include <KLocalizedString>
#include <KMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <kcommandbar.h>
#include <QDir>
#include <QProcess>
#include <QUuid>
#include <QTextStream>

#include "MainWindow.h"
#include "ViewManager.h"
#include "terminalDisplay/TerminalDisplay.h"

K_PLUGIN_CLASS_WITH_JSON(SSHManagerPlugin, "konsole_sshmanager.json")

struct SSHManagerPluginPrivate {
    SSHManagerModel model;

    QMap<Konsole::MainWindow *, SSHManagerTreeWidget *> widgetForWindow;
    QMap<Konsole::MainWindow *, QDockWidget *> dockForWindow;
    QAction *showQuickAccess = nullptr;
    
    QPointer<Konsole::MainWindow> currentMainWindow;

    // Track active SSHFS mounts by SSH entry name.
    // Stores the ref count (number of sessions using this mount) and the socket path.
    struct SshfsMount {
        int refCount = 0;
        QString socketPath;
        QString mountPoint;
    };
    QHash<QString, SshfsMount> activeSshfsMounts;
};

SSHManagerPlugin::SSHManagerPlugin(QObject *object, const QVariantList &args)
    : Konsole::IKonsolePlugin(object, args)
    , d(std::make_unique<SSHManagerPluginPrivate>())
{
    d->showQuickAccess = new QAction();

    setName(QStringLiteral("SshManager"));
    KCrash::initialize();
}

SSHManagerPlugin::~SSHManagerPlugin()
{
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
    sshDockWidget->setAllowedAreas(Qt::DockWidgetArea::LeftDockWidgetArea | Qt::DockWidgetArea::RightDockWidgetArea);

    mainWindow->addDockWidget(Qt::LeftDockWidgetArea, sshDockWidget);

    d->widgetForWindow[mainWindow] = managerWidget;
    d->dockForWindow[mainWindow] = sshDockWidget;
    d->currentMainWindow = mainWindow;

    connect(managerWidget, &SSHManagerTreeWidget::requestNewTab, this, [mainWindow] {
        mainWindow->newTab();
    });
    
    connect(managerWidget, &SSHManagerTreeWidget::requestConnection, this, &SSHManagerPlugin::requestConnection);

    connect(managerWidget, &SSHManagerTreeWidget::quickAccessShortcutChanged, this, [this, mainWindow](QKeySequence s) {
        mainWindow->actionCollection()->setDefaultShortcut(d->showQuickAccess, s);

        QString sequenceText = s.toString();
        QSettings settings;
        settings.beginGroup(QStringLiteral("plugins"));
        settings.beginGroup(QStringLiteral("sshplugin"));
        settings.setValue(QStringLiteral("ssh_shortcut"), sequenceText);
        settings.sync();
    });
}

QList<QAction *> SSHManagerPlugin::menuBarActions(Konsole::MainWindow *mainWindow) const
{
    Q_UNUSED(mainWindow)

    QAction *toggleVisibilityAction = new QAction(i18n("Show SSH Manager"), mainWindow);
    toggleVisibilityAction->setCheckable(true);
    mainWindow->actionCollection()->setDefaultShortcut(toggleVisibilityAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F2));
    connect(toggleVisibilityAction, &QAction::triggered, d->dockForWindow[mainWindow], &QDockWidget::setVisible);
    connect(d->dockForWindow[mainWindow], &QDockWidget::visibilityChanged, toggleVisibilityAction, &QAction::setChecked);

    return {toggleVisibilityAction};
}

void SSHManagerPlugin::activeViewChanged(Konsole::SessionController *controller, Konsole::MainWindow *mainWindow)
{
    Q_ASSERT(controller);
    Q_ASSERT(mainWindow);

    auto terminalDisplay = controller->view();

    d->showQuickAccess->deleteLater();
    d->showQuickAccess = new QAction(i18n("Show Quick Access for SSH Actions"));

    QSettings settings;
    settings.beginGroup(QStringLiteral("plugins"));
    settings.beginGroup(QStringLiteral("sshplugin"));

    const QKeySequence def(Qt::CTRL | Qt::ALT | Qt::Key_H);
    const QString defText = def.toString();
    const QString entry = settings.value(QStringLiteral("ssh_shortcut"), defText).toString();
    const QKeySequence shortcutEntry(entry);

    mainWindow->actionCollection()->setDefaultShortcut(d->showQuickAccess, shortcutEntry);
    terminalDisplay->addAction(d->showQuickAccess);

    connect(d->showQuickAccess, &QAction::triggered, this, [this, terminalDisplay, controller] {
        auto bar = new KCommandBar(terminalDisplay->topLevelWidget());
        QList<QAction *> actions;
        for (int i = 0; i < d->model.rowCount(); i++) {
            QModelIndex folder = d->model.index(i, 0);
            for (int e = 0; e < d->model.rowCount(folder); e++) {
                QModelIndex idx = d->model.index(e, 0, folder);
                QAction *act = new QAction(idx.data().toString());
                connect(act, &QAction::triggered, this, [this, idx, controller] {
                    requestConnection(idx, controller);
                });
                actions.append(act);
            }
        }

        if (actions.isEmpty()) // no ssh config found, must give feedback to the user about that
        {
            const QString feedbackMessage = i18n("No saved SSH config found. You can add one on Plugins -> SSH Manager");
            const QString feedbackTitle = i18n("Plugins - SSH Manager");
            KMessageBox::error(terminalDisplay->topLevelWidget(), feedbackMessage, feedbackTitle);
            return;
        }

        QVector<KCommandBar::ActionGroup> groups;
        groups.push_back(KCommandBar::ActionGroup{i18n("SSH Entries"), actions});
        bar->setActions(groups);
        bar->show();
    });

    if (mainWindow) {
        d->widgetForWindow[mainWindow]->setCurrentController(controller);
        d->currentMainWindow = mainWindow;
    }
    
}

void SSHManagerPlugin::requestConnection(const QModelIndex &idx, Konsole::SessionController *controller)
{
    if (!controller) {
        return;
    }
    
    // Index should already be from source model
    if (idx.parent() == d->model.invisibleRootItem()->index()) {
        return;
    }

    auto item = d->model.itemFromIndex(idx);
    auto data = item->data(SSHManagerModel::SSHRole).value<SSHConfigurationData>();

#ifndef Q_OS_WIN
    // Check if the current shell is idle (running a known shell process)
    bool shellIsIdle = false;
    Konsole::ProcessInfo *info = controller->session()->getProcessInfo();
    bool ok = false;
    if (info) {
        QString processName = info->name(&ok);
        if (ok) {
            const QStringList shells = {QStringLiteral("fish"),
                                        QStringLiteral("bash"),
                                        QStringLiteral("dash"),
                                        QStringLiteral("sh"),
                                        QStringLiteral("csh"),
                                        QStringLiteral("ksh"),
                                        QStringLiteral("zsh")};
            shellIsIdle = shells.contains(processName);
        }
    }

    if (!shellIsIdle) {
        // Shell is busy (running vim, another ssh, etc.) or PTY not ready.
        // Open a new tab and connect there once the session's shell is ready.
        Konsole::MainWindow *mainWindow = d->currentMainWindow;
        if (!mainWindow && controller->view()) {
            mainWindow = qobject_cast<Konsole::MainWindow *>(controller->view()->window());
        }
        if (mainWindow) {
            // Create the new tab — this triggers activeViewChanged, but we do NOT
            // use pendingConnectionIndex. Instead we connect to the new session's
            // started() signal so we wait for the PTY to be ready.
            mainWindow->newTab();

            // The new tab's controller is now the active one
            auto *viewManager = mainWindow->viewManager();
            auto *newController = viewManager->activeViewController();
            if (newController && newController != controller) {
                // Wait for the session's shell to actually start before sending commands
                connect(newController->session(), &Konsole::Session::started, this,
                        [this, data, newController]() {
                            startConnection(data, newController);
                        }, Qt::SingleShotConnection);
            }
        }
        return;
    }
#else
    // FIXME: Can we do this on windows?
#endif

    startConnection(data, controller);
}

void SSHManagerPlugin::startConnection(const SSHConfigurationData &data, Konsole::SessionController *controller)
{
    if (!controller || !controller->session()) {
        return;
    }

    QString sshCommand = QStringLiteral("ssh ");
    if (data.useSshConfig) {
        sshCommand += data.name;
    } else {
        if (!data.password.isEmpty()) {
            sshCommand = QStringLiteral("sshpass -p '%1' ").arg(data.password) + sshCommand;
        } else if (!data.sshKeyPassphrase.isEmpty()) {
            // Use sshpass with -P to match the "Enter passphrase" prompt from ssh
            sshCommand = QStringLiteral("sshpass -P 'passphrase' -p '%1' ").arg(data.sshKeyPassphrase) + sshCommand;
        }

        if (data.autoAcceptKeys) {
             sshCommand += QStringLiteral("-o StrictHostKeyChecking=no ");
        }

        if (data.useProxy && !data.proxyIp.isEmpty() && !data.proxyPort.isEmpty()) {
             QString proxyCmd = QStringLiteral("ncat --proxy-type socks5 ");
             if (!data.proxyUsername.isEmpty()) {
                 proxyCmd += QStringLiteral("--proxy-auth %1:%2 ").arg(data.proxyUsername, data.proxyPassword);
             }
             proxyCmd += QStringLiteral("--proxy %1:%2 %h %p").arg(data.proxyIp, data.proxyPort);
             
             sshCommand += QStringLiteral("-o ProxyCommand='%1' ").arg(proxyCmd);
        }

        if (data.sshKey.length()) {
            sshCommand += QStringLiteral("-i %1 ").arg(data.sshKey);
        }

        if (data.port.length()) {
            sshCommand += QStringLiteral("-p %1 ").arg(data.port);
        }

        if (!data.username.isEmpty()) {
            sshCommand += data.username + QLatin1Char('@');
        }
        
        if (!data.host.isEmpty()) {
            sshCommand += data.host;
        }
    }

    // SSHFS mount handling: reuse existing mount if one is already active for this entry
    const bool sshfsAlreadyMounted = data.enableSshfs && d->activeSshfsMounts.contains(data.name);
    const QString uuid = QUuid::createUuid().toString(QUuid::Id128);
    const QString socketPath = sshfsAlreadyMounted
        ? d->activeSshfsMounts[data.name].socketPath
        : QStringLiteral("/tmp/konsole_ssh_socket_") + uuid;

    if (data.enableSshfs && !sshfsAlreadyMounted) {
        sshCommand += QStringLiteral(" -M -S %1 -o ControlPersist=5s ").arg(socketPath);
    }
    
    QString mountPoint;
    if (data.enableSshfs) {
        const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        mountPoint = home + QStringLiteral("/sshfs_mounts/") + data.name;
    }

    if (data.enableSshfs && !sshfsAlreadyMounted) {
        // First session for this host — set up the mount
        QDir().mkpath(mountPoint);
        
        auto *timer = new QTimer(controller->session());
        timer->setInterval(500);
        
        auto *counter = new int(0); 
        
        QObject::connect(timer, &QTimer::timeout, controller->session(), [timer, counter, socketPath, data, mountPoint]() {
             (*counter)++;
             if (*counter > 60) {
                 timer->stop();
                 timer->deleteLater();
                 delete counter;
                 return;
             }

             if (QFile::exists(socketPath)) {
                 timer->stop();
                 timer->deleteLater();
                 delete counter;

                 QString rcloneExe = QStandardPaths::findExecutable(QStringLiteral("rclone"));
                 if (rcloneExe.isEmpty()) {
                     const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
                     QString localRclone = home + QStringLiteral("/.local/bin/rclone");
                     if (QFile::exists(localRclone)) {
                         rcloneExe = localRclone;
                     } else {
                         rcloneExe = QStringLiteral("rclone"); 
                     }
                 }

                 QString mountCmd = rcloneExe + QStringLiteral(" mount");

                 mountCmd += QStringLiteral(" --vfs-cache-mode full");
                 mountCmd += QStringLiteral(" --vfs-cache-max-age 1h");
                 
                 QString sshWrapper;
                 
                 if (data.useSshConfig) {
                      sshWrapper = QStringLiteral("ssh -S %1 %2").arg(socketPath, data.name);
                 } else {
                      sshWrapper = QStringLiteral("ssh -S %1").arg(socketPath);
                      if (data.port.length()) {
                           sshWrapper += QStringLiteral(" -p %1").arg(data.port);
                      }
                      if (!data.username.isEmpty()) {
                           sshWrapper += QStringLiteral(" %1@%2").arg(data.username, data.host);
                      } else {
                           sshWrapper += QStringLiteral(" %1").arg(data.host);
                      }
                 }
                 
                 mountCmd += QStringLiteral(" --sftp-ssh '%1'").arg(sshWrapper);
                 
                 QString rcloneTarget;
                 if (data.useSshConfig) {
                     rcloneTarget = QStringLiteral(":sftp,host=%1:/").arg(data.name);
                 } else {
                     if (!data.username.isEmpty()) {
                         rcloneTarget = QStringLiteral(":sftp,host=%1,user=%2:/").arg(data.host, data.username);
                     } else {
                         rcloneTarget = QStringLiteral(":sftp,host=%1:/").arg(data.host);
                     }
                 }
                 mountCmd += QStringLiteral(" ") + rcloneTarget;
                 
                 mountCmd += QStringLiteral(" ") + mountPoint;
                 
                 mountCmd += QStringLiteral(" --volname %1").arg(data.name);
                 
                 QString logFile = QStringLiteral("/tmp/konsole_rclone_%1.log").arg(data.name);
                 mountCmd += QStringLiteral(" --log-file=\"%1\" -vv").arg(logFile);
                 
                 mountCmd += QStringLiteral(" --daemon");

                 QProcess::startDetached(QStringLiteral("sh"), {QStringLiteral("-c"), mountCmd});
             }
        });
        
        timer->start();

        // Register this mount
        d->activeSshfsMounts[data.name] = {1, socketPath, mountPoint};
    } else if (data.enableSshfs) {
        // Another session connecting to the same host — just bump the ref count
        d->activeSshfsMounts[data.name].refCount++;
    }

    if (data.enableSshfs) {
        // When session finishes, decrement ref count; only unmount when last session closes
        const QString entryName = data.name;
        QObject::connect(controller->session(), &Konsole::Session::finished, this, [this, entryName]() {
            if (!d->activeSshfsMounts.contains(entryName)) {
                return;
            }
            auto &mount = d->activeSshfsMounts[entryName];
            mount.refCount--;
            if (mount.refCount <= 0) {
                QProcess::execute(QStringLiteral("fusermount"), {QStringLiteral("-u"), QStringLiteral("-z"), mount.mountPoint});
                QDir().rmdir(mount.mountPoint);
                QFile::remove(mount.socketPath);
                d->activeSshfsMounts.remove(entryName);
            }
        });
    }

    // Set tab title to the SSH identifier, or hostname if no name was set
    const QString tabTitle = data.name.isEmpty() ? data.host : data.name;
    controller->session()->setTitle(Konsole::Session::NameRole, tabTitle);
    controller->session()->setTabTitleFormat(Konsole::Session::LocalTabTitle, tabTitle);
    controller->session()->setTabTitleFormat(Konsole::Session::RemoteTabTitle, tabTitle);
    controller->session()->tabTitleSetByUser(true);

    // Hide the SSH command from the user entirely.
    // We disable PTY echo so the typed command line is not displayed,
    // then send a single compound command that:
    // - clears the screen to remove the shell prompt
    // - prints "Connecting to <name>..." (without newline)
    // - runs the SSH command with stderr suppressed
    // - on success: prints green "OK" (via ssh's LocalCommand, runs after auth)
    // - on failure: prints red "FAILED" and stays on that line (no error spam)
    // The leading space prevents the command from being saved in shell history.
    //
    // LocalCommand is executed by ssh right after successful authentication,
    // so "OK" appears before the remote shell prompt. If ssh fails (connection
    // refused, auth error, etc.), it exits with non-zero and the shell prints
    // "FAILED" instead. stderr is redirected to /dev/null to hide ssh's own
    // error messages (the user only sees our clean "FAILED").
    const QString greenOk = QStringLiteral("printf ' \\033[32mOK\\033[0m\\n'");
    const QString localCmdOpts = QStringLiteral("-o PermitLocalCommand=yes -o LocalCommand=\"%1\" ").arg(greenOk);

    // Insert the LocalCommand options into the ssh arguments
    // sshCommand starts with "ssh " or "sshpass ... ssh ", find "ssh " and insert after it
    int sshPos = sshCommand.lastIndexOf(QStringLiteral("ssh "));
    if (sshPos >= 0) {
        sshCommand.insert(sshPos + 4, localCmdOpts);
    }

    const QString redFailed = QStringLiteral("printf ' \\033[31mFAILED\\033[0m\\n'");
    // Run ssh with stderr hidden; if it fails (non-zero exit), print FAILED
    QString wrappedCommand = QStringLiteral(" clear; printf 'Connecting to %1...'; %2 2>/dev/null || { %3; exec bash; }")
        .arg(tabTitle, sshCommand, redFailed);

    QPointer<Konsole::Session> session = controller->session();
    session->setEchoEnabled(false);
    session->sendTextToTerminal(wrappedCommand, QLatin1Char('\r'));

    // Re-enable echo after SSH starts (the remote shell manages its own echo)
    QTimer::singleShot(500, controller->session(), [session]() {
        if (session) {
            session->setEchoEnabled(true);
        }
    });

    if (controller->session()->views().count()) {
        controller->session()->views().at(0)->setFocus();
    }
}

#include "moc_sshmanagerplugin.cpp"
#include "sshmanagerplugin.moc"

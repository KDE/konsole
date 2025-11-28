/*
 SPDX-FileCopyrightText: 2017 Kurt Hindenburg <kurt.hindenburg@gmail.com>

 SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "demo_konsolepart.h"

#include <QApplication>
#include <QDebug>
#include <QMenu>
#include <QMenuBar>

#include <KPluginFactory>
#include <KWindowEffects>

demo_konsolepart::demo_konsolepart()
    : KMainWindow()
    , _terminalPart(nullptr)
    , _terminal(nullptr)
{
    // Set the WA_NativeWindow attribute to force the creation of the QWindow.
    // Without this QWidget::windowHandle() returns 0.
    // See https://phabricator.kde.org/D23108
    setAttribute(Qt::WA_NativeWindow);

    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, false);

    // Create terminal part and embed in into the main window
    _terminalPart = createPart();
    if (_terminalPart == nullptr) {
        return;
    }

    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("File"));
    QAction *manageProfilesAction = fileMenu->addAction(QStringLiteral("Manage Profiles..."));
    connect(manageProfilesAction, &QAction::triggered, this, &demo_konsolepart::manageProfiles);
    QAction *quitAction = fileMenu->addAction(QStringLiteral("Quit"));
    connect(quitAction, &QAction::triggered, this, &demo_konsolepart::quit);

    connect(_terminalPart, &KParts::ReadOnlyPart::destroyed, this, &demo_konsolepart::quit);

    setCentralWidget(_terminalPart->widget());
    _terminal = qobject_cast<TerminalInterface *>(_terminalPart);

    // Test if blur is enabled for profile
    bool blurEnabled = false;
    bool invoked = QMetaObject::invokeMethod(_terminalPart, "isBlurEnabled", Qt::DirectConnection, Q_RETURN_ARG(bool, blurEnabled));
    if (!invoked) {
        qWarning() << "isBlurEnabled invocation failed; using default:" << blurEnabled;
    } else {
        qWarning() << "blur enabled: " << blurEnabled;
    }
    KWindowEffects::enableBlurBehind(windowHandle(), blurEnabled);
}

demo_konsolepart::~demo_konsolepart()
{
    if (_terminalPart != nullptr) {
        disconnect(_terminalPart, &KParts::ReadOnlyPart::destroyed, this, &demo_konsolepart::quit);
    }
}

KParts::ReadOnlyPart *demo_konsolepart::createPart()
{
    const KPluginMetaData metaData(QStringLiteral("kf6/parts/konsolepart"), KPluginMetaData::AllowEmptyMetaData);
    Q_ASSERT(metaData.isValid());

    KPluginFactory::Result<KParts::ReadOnlyPart> result = KPluginFactory::instantiatePlugin<KParts::ReadOnlyPart>(metaData, this);
    Q_ASSERT(result);

    return result.plugin;
}

void demo_konsolepart::manageProfiles()
{
    QMetaObject::invokeMethod(_terminalPart, "showManageProfilesDialog", Qt::QueuedConnection, Q_ARG(QWidget *, QApplication::activeWindow()));
}

void demo_konsolepart::quit()
{
    close();
}

#include "moc_demo_konsolepart.cpp"

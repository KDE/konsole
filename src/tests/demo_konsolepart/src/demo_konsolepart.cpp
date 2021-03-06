/*
 SPDX-FileCopyrightText: 2017 Kurt Hindenburg <kurt.hindenburg@gmail.com>

 SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "demo_konsolepart.h"

#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QDebug>

#include <KPluginLoader>
#include <KPluginFactory>
#include <KService>
#include <KWindowSystem>
#include <KWindowEffects>

#include <kwindowsystem_version.h>

// see below notes
//#include "../../../WindowSystemInfo.h"

demo_konsolepart::demo_konsolepart()
    : KMainWindow(),
      _mainWindow(nullptr),
      _terminalPart(nullptr),
      _terminal(nullptr)
{
    const bool useTranslucency = KWindowSystem::compositingActive();

    setAttribute(Qt::WA_TranslucentBackground, useTranslucency);
    setAttribute(Qt::WA_NoSystemBackground, false);

    // This is used in EditProfileDialog to show the warnings about
    // transparency issues - needs refactoring as the above
    // include does not work
//    WindowSystemInfo::HAVE_TRANSPARENCY = useTranslucency;

    // Create terminal part and embed in into the main window
    _terminalPart = createPart();
    if (_terminalPart == nullptr) {
        return;
    }

    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("File"));
    QAction* manageProfilesAction = fileMenu->addAction(QStringLiteral("Manage Profiles..."));
    connect(manageProfilesAction, &QAction::triggered, this, &demo_konsolepart::manageProfiles);
    QAction* quitAction = fileMenu->addAction(QStringLiteral("Quit"));
    connect(quitAction, &QAction::triggered, this, &demo_konsolepart::quit);

    connect(_terminalPart, &KParts::ReadOnlyPart::destroyed, this, &demo_konsolepart::quit);

    setCentralWidget(_terminalPart->widget());
    _terminal = qobject_cast<TerminalInterface*>(_terminalPart);

    // Test if blur is enabled for profile
    bool blurEnabled;
    QMetaObject::invokeMethod(_terminalPart, "isBlurEnabled",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(bool, blurEnabled));
    qWarning()<<"blur enabled: "<<blurEnabled;
#if KWINDOWSYSTEM_VERSION < QT_VERSION_CHECK(5,82,0)
    KWindowEffects::enableBlurBehind(winId(), blurEnabled);
#else
    KWindowEffects::enableBlurBehind(windowHandle(), blurEnabled);
#endif
}

demo_konsolepart::~demo_konsolepart()
{
    if (_terminalPart != nullptr) {
        disconnect(_terminalPart, &KParts::ReadOnlyPart::destroyed, this, &demo_konsolepart::quit);
    }
}

KParts::ReadOnlyPart* demo_konsolepart::createPart()
{
    KService::Ptr service = KService::serviceByDesktopName(QStringLiteral("konsolepart"));
    Q_ASSERT(service);
    KPluginFactory* factory = KPluginLoader(service->library()).factory();
    Q_ASSERT(factory);

    auto* terminalPart = factory->create<KParts::ReadOnlyPart>(this);

    return terminalPart;
}

void demo_konsolepart::manageProfiles()
{
    QMetaObject::invokeMethod(_terminalPart, "showManageProfilesDialog",
        Qt::QueuedConnection, Q_ARG(QWidget*, QApplication::activeWindow()));
}

void demo_konsolepart::quit()
{
    close();
}

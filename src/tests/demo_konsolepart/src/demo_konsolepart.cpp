/*
 Copyright 2017 by Kurt Hindenburg <kurt.hindenburg@gmail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation; either version 2 of
 the License or (at your option) version 3 or any later version
 accepted by the membership of KDE e.V. (or its successor approved
 by the membership of KDE e.V.), which shall act as a proxy
 defined in Section 14 of version 3 of the license.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
    KWindowEffects::enableBlurBehind(winId(), blurEnabled);
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

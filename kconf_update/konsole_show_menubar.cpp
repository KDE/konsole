/*
    SPDX-FileCopyrightText: 2022 Andrey Butirsky <butirsky@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later

    Keeps Menu Bar enabled if user has hidden a Session Toolbar thus no Hamburger Menu button is shown
*/

#include <QApplication>
#include <KAboutData>
#include <KXmlGuiWindow>
#include <KSharedConfig>
#include <KConfigGroup>
#include <QMenuBar>
#include <KToolBar>

int main (int argc, char *argv[])
{
    QApplication app(argc, argv);

    KAboutData::setApplicationData(KAboutData(QStringLiteral("konsole")));

    KXmlGuiWindow mainWindow;
    mainWindow.setupGUI(KXmlGuiWindow::Default, QStringLiteral("sessionui.rc"));

    static const char menuBarKey[] = "MenuBar";
    // SimpleConfig so that system-wide default won't interfere and hasKey() indicates user defined setting
    if (auto cg = KSharedConfig::openConfig(QStringLiteral("konsolerc"), KConfig::OpenFlag::SimpleConfig)->group("MainWindow"); !cg.hasKey(menuBarKey) &&
            mainWindow.menuBar()->isHidden() && mainWindow.toolBar(QStringLiteral("sessionToolbar"))->isHidden()) {
        cg.writeEntry(menuBarKey, "Enabled");
    }
}

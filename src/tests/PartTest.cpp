/*
    Copyright 2008 by Robert Knight <robertknight@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own
#include "PartTest.h"

// Qt
#include <QLabel>
#include <QVBoxLayout>

// KDE
#include <KPluginLoader>
#include <KPluginFactory>
#include <KService>
#include <KParts/Part>
#include <KPtyProcess>
#include <KPtyDevice>
#include <KDialog>
#include <KDebug>
#include <KMainWindow>
#include <qtest_kde.h>

// Konsole
#include "../Pty.h"

using namespace Konsole;

void PartTest::testFd()
{
    // find ping
    QStringList pingList;
    QFileInfo info;
    QString pingExe;
    pingList << "/bin/ping" << "/sbin/ping";
    for (int i = 0; i < pingList.size(); ++i) {
        info.setFile(pingList.at(i));
        if (info.exists() && info.isExecutable())
            pingExe = pingList.at(i);
    }

    if (pingExe.isEmpty()) {
        QSKIP("ping command not found.");
        return;
    }

    // create a Konsole part and attempt to connect to it
    KParts::Part* terminalPart = createPart();
    if (!terminalPart) { // not found
        QSKIP("konsolepart not found.");
        return;
    }

    // start a pty process
    KPtyProcess ptyProcess;
    ptyProcess.setProgram(pingExe, QStringList() << "localhost");
    ptyProcess.setPtyChannels(KPtyProcess::AllChannels);
    ptyProcess.start();
    QVERIFY(ptyProcess.waitForStarted());

    int fd = ptyProcess.pty()->masterFd();

    bool result = QMetaObject::invokeMethod(terminalPart, "openTeletype",
                                            Qt::DirectConnection, Q_ARG(int, fd));
    QVERIFY(result);

    // suspend the KPtyDevice so that the embedded terminal gets a chance to
    // read from the pty.  Otherwise the KPtyDevice will simply read everything
    // as soon as it becomes available and the terminal will not display any output
    ptyProcess.pty()->setSuspended(true);

    QWeakPointer<KDialog> dialog = new KDialog();
    dialog.data()->setButtons(0);
    QVBoxLayout* layout = new QVBoxLayout(dialog.data()->mainWidget());
    layout->addWidget(new QLabel("Output of 'ping localhost' should appear in a terminal below for 5 seconds"));
    layout->addWidget(terminalPart->widget());
    QTimer::singleShot(5000, dialog.data(), SLOT(close()));
    dialog.data()->exec();

    delete terminalPart;
    delete dialog.data();
    ptyProcess.kill();
    ptyProcess.waitForFinished(1000);
}

KParts::Part* PartTest::createPart()
{
    KService::Ptr service = KService::serviceByDesktopName("konsolepart");
    if (!service)       // not found
        return 0;
    KPluginFactory* factory = KPluginLoader(service->library()).factory();
    if (!factory)       // not found
        return 0;

    KParts::Part* terminalPart = factory->create<KParts::Part>(this);

    return terminalPart;
}

QTEST_KDEMAIN(PartTest , GUI)


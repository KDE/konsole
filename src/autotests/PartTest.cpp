/*
    SPDX-FileCopyrightText: 2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "PartTest.h"

// Qt
#include <QDialog>
#include <QFileInfo>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
// KDE
#include <KPluginFactory>
#include <KPluginLoader>
#include <KPtyDevice>
#include <KPtyProcess>
#include <qtest.h>

// Konsole
#include "../Pty.h"

using namespace Konsole;

void PartTest::initTestCase()
{
    /* Try to test against build konsolepart, so move directory containing
      executable to front of libraryPaths.  KPluginLoader should find the
      part first in the build dir over the system installed ones.
      I believe the CI installs first and then runs the test so the other
      paths can not be removed.
    */
    const auto libraryPaths = QCoreApplication::libraryPaths();
    auto buildPath = libraryPaths.last();
    QCoreApplication::removeLibraryPath(buildPath);
    // konsolepart.so is in ../autotests/
    if (buildPath.endsWith(QStringLiteral("/autotests"))) {
        buildPath.chop(10);
    }
    QCoreApplication::addLibraryPath(buildPath);
}

void PartTest::testFdShell()
{
    testFd(true);
}

void PartTest::testFdStandalone()
{
    testFd(false);
}

void PartTest::testFd(bool runShell)
{
    // find ping
    QStringList pingList;
    QFileInfo info;
    QString pingExe;
    pingList << QStringLiteral("/bin/ping") << QStringLiteral("/sbin/ping");
    for (int i = 0; i < pingList.size(); ++i) {
        info.setFile(pingList.at(i));
        if (info.exists() && info.isExecutable()) {
            pingExe = pingList.at(i);
        }
    }

    if (pingExe.isEmpty()) {
        QSKIP("ping command not found.");
        return;
    }

    // create a Konsole part and attempt to connect to it
    KParts::Part *terminalPart = createPart();
    if (terminalPart == nullptr) { // not found
        QFAIL("konsolepart not found.");
        return;
    }

    // start a pty process
    KPtyProcess ptyProcess;
    ptyProcess.setProgram(pingExe, QStringList() << QStringLiteral("localhost"));
    ptyProcess.setPtyChannels(KPtyProcess::AllChannels);
    ptyProcess.start();
    QVERIFY(ptyProcess.waitForStarted());

    int fd = ptyProcess.pty()->masterFd();

    // test that the 2nd argument of openTeletype is optional,
    // to run without shell
    if (runShell) {
        // connect to an existing pty
        bool result = QMetaObject::invokeMethod(terminalPart, "openTeletype", Qt::DirectConnection, Q_ARG(int, fd));
        QVERIFY(result);
    } else {
        // test the optional 2nd argument of openTeletype, to run without shell
        bool result = QMetaObject::invokeMethod(terminalPart, "openTeletype", Qt::DirectConnection, Q_ARG(int, fd), Q_ARG(bool, false));
        QVERIFY(result);
    }

    // suspend the KPtyDevice so that the embedded terminal gets a chance to
    // read from the pty.  Otherwise the KPtyDevice will simply read everything
    // as soon as it becomes available and the terminal will not display any output
    ptyProcess.pty()->setSuspended(true);

    QPointer<QDialog> dialog = new QDialog();
    auto layout = new QVBoxLayout(dialog.data());
    auto explanation = runShell ? QStringLiteral("Output of 'ping localhost' should appear in a terminal below for 5 seconds")
                                : QStringLiteral("Output of 'ping localhost' should appear standalone below for 5 seconds");
    layout->addWidget(new QLabel(explanation));
    layout->addWidget(terminalPart->widget());
    QTimer::singleShot(5000, dialog.data(), &QDialog::close);
    dialog.data()->exec();

    delete terminalPart;
    delete dialog.data();
    ptyProcess.kill();
    ptyProcess.waitForFinished(1000);
}

KParts::Part *PartTest::createPart()
{
    auto konsolePartPlugin = KPluginLoader::findPlugin(QStringLiteral("konsolepart"));
    if (konsolePartPlugin.isNull()) {
        return nullptr;
    }

    KPluginFactory *factory = KPluginLoader(konsolePartPlugin).factory();
    if (factory == nullptr) { // not found
        return nullptr;
    }

    auto *terminalPart = factory->create<KParts::Part>(this);

    return terminalPart;
}

QTEST_MAIN(PartTest)

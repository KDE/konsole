/*
    Copyright 2014 by Kurt Hindenburg <kurt.hindenburg@gmail.com>

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
#include "TerminalInterfaceTest.h"
#include "../Profile.h"
#include "../ProfileManager.h"
#include "config-konsole.h"

// Qt
#include <QDir>
#include <QSignalSpy>

// KDE
#include <KService>
#include <qtest.h>

using namespace Konsole;

/* TerminalInterface found in KParts/kde_terminal_interface.h
 *
 *  void startProgram(const QString &program,
 *                    const QStringList &args)
 *  void showShellInDir(const QString &dir)
 *  void sendInput(const QString &text)
 *  int terminalProcessId()
 *  int foregroundProcessId()
 *  QString foregroundProcessName()
 *  QString currentWorkingDirectory() const
*/

void TerminalInterfaceTest::initTestCase()
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
    if (buildPath.endsWith(QLatin1String("/autotests"))) {
        buildPath.chop(10);
    }
    QCoreApplication::addLibraryPath(buildPath);
}

// Test with no shell running
void TerminalInterfaceTest::testTerminalInterfaceNoShell()
{
    // create a Konsole part and attempt to connect to it
    _terminalPart = createPart();
    if (_terminalPart == nullptr) {
        QFAIL("konsolepart not found.");
    }

    TerminalInterface *terminal = qobject_cast<TerminalInterface *>(_terminalPart);
    QVERIFY(terminal);

#if !defined(Q_OS_FREEBSD)
    // Skip this for now on FreeBSD
    // -1 is current foreground process and name for process 0 is "kernel"

    // Verify results when no shell running
    int terminalProcessId = terminal->terminalProcessId();
    QCOMPARE(terminalProcessId, 0);
    int foregroundProcessId = terminal->foregroundProcessId();
    QCOMPARE(foregroundProcessId, -1);
    QString foregroundProcessName = terminal->foregroundProcessName();
    QCOMPARE(foregroundProcessName, QString());
    const QString currentWorkingDirectory = terminal->currentWorkingDirectory();
    QCOMPARE(currentWorkingDirectory, QString());

#endif
    delete _terminalPart;
}

// Test with default shell running
void TerminalInterfaceTest::testTerminalInterface()
{
    QString currentDirectory;

    // create a Konsole part and attempt to connect to it
    _terminalPart = createPart();
    if (_terminalPart == nullptr) {
        QFAIL("konsolepart not found.");
    }

    TerminalInterface *terminal = qobject_cast<TerminalInterface *>(_terminalPart);
    QVERIFY(terminal);

    // Start a shell in given directory
    terminal->showShellInDir(QDir::home().path());

    int foregroundProcessId = terminal->foregroundProcessId();
    QCOMPARE(foregroundProcessId, -1);
    QString foregroundProcessName = terminal->foregroundProcessName();
    QCOMPARE(foregroundProcessName, QString());

    // terminalProcessId() is the user's default shell
    // FIXME: find a way to verify this
    // int terminalProcessId  = terminal->terminalProcessId();

    // Let's try using QSignalSpy
    // http://techbase.kde.org/Development/Tutorials/Unittests
    // QSignalSpy is really a QList of QLists, so we take the first
    // list, which corresponds to the arguments for the first signal
    // we caught.

    QSignalSpy stateSpy(_terminalPart, SIGNAL(currentDirectoryChanged(QString)));
    QVERIFY(stateSpy.isValid());

    // Now we check to make sure we don't have any signals already
    QCOMPARE(stateSpy.count(), 0);

    // Let's trigger some signals

    // #1A - Test signal currentDirectoryChanged(QString)
    currentDirectory = QStringLiteral("/tmp");
    terminal->sendInput(QStringLiteral("cd ") + currentDirectory + QLatin1Char('\n'));
    stateSpy.wait(2000);
    QCOMPARE(stateSpy.count(), 1);

    // Correct result?
    QList<QVariant> firstSignalArgs = stateSpy.takeFirst();

    // Actual: /Users/kurthindenburg
    // Expected: /tmp
#if !defined(Q_OS_MACOS)
    QString firstSignalState = firstSignalArgs.at(0).toString();
    QCOMPARE(firstSignalState, currentDirectory);

    const QString currentWorkingDirectory = terminal->currentWorkingDirectory();
    QCOMPARE(currentWorkingDirectory, currentDirectory);

    // #1B - Test signal currentDirectoryChanged(QString)
    // Invalid directory - no signal should be emitted
    terminal->sendInput(QStringLiteral("cd /usrADADFASDF\n"));
    stateSpy.wait(2000);
    QCOMPARE(stateSpy.count(), 0);

    // Should be no change since the above cd didn't work
    const QString currentWorkingDirectory2 = terminal->currentWorkingDirectory();
    QCOMPARE(currentWorkingDirectory2, currentDirectory);

    // Test starting a new program
    QString command = QStringLiteral("top");
    terminal->sendInput(command + QLatin1Char('\n'));
    stateSpy.wait(2000);
    // FIXME: find a good way to validate process id of 'top'
    foregroundProcessId = terminal->foregroundProcessId();
    QVERIFY(foregroundProcessId != -1);
    foregroundProcessName = terminal->foregroundProcessName();
    QCOMPARE(foregroundProcessName, command);

    terminal->sendInput(QStringLiteral("q"));
    stateSpy.wait(2000);

    // Nothing running in foreground
    foregroundProcessId = terminal->foregroundProcessId();
    QCOMPARE(foregroundProcessId, -1);
    foregroundProcessName = terminal->foregroundProcessName();
    QCOMPARE(foregroundProcessName, QString());
#endif

    // Test destroyed()
    QSignalSpy destroyedSpy(_terminalPart, SIGNAL(destroyed()));
    QVERIFY(destroyedSpy.isValid());

    // Now we check to make sure we don't have any signals already
    QCOMPARE(destroyedSpy.count(), 0);

    delete _terminalPart;
    QCOMPARE(destroyedSpy.count(), 1);
}

void TerminalInterfaceTest::testTerminalInterfaceV2()
{
#ifdef USE_TERMINALINTERFACEV2
    Profile::Ptr testProfile(new Profile);
    testProfile->useFallback();
    ProfileManager::instance()->addProfile(testProfile);

    _terminalPart = createPart();
    if (_terminalPart == nullptr) {
        QFAIL("konsolepart not found.");
    }

    TerminalInterfaceV2 *terminal = qobject_cast<TerminalInterfaceV2*>(_terminalPart);

    QVERIFY(terminal);
    QVERIFY(terminal->setCurrentProfile(testProfile->name()));
    QCOMPARE(terminal->currentProfileName(), testProfile->name());

    QCOMPARE(terminal->profileProperty(QStringLiteral("Path")), testProfile->path());
    QCOMPARE(terminal->profileProperty(QStringLiteral("SilenceSeconds")), testProfile->silenceSeconds());
    QCOMPARE(terminal->profileProperty(QStringLiteral("Icon")), testProfile->icon());
    QCOMPARE(terminal->profileProperty(QStringLiteral("ShowTerminalSizeHint")), testProfile->showTerminalSizeHint());
    QCOMPARE(terminal->profileProperty(QStringLiteral("Environment")), testProfile->environment());
    QCOMPARE(terminal->profileProperty(QStringLiteral("BellMode")), testProfile->property<QVariant>(Profile::Property::BellMode));
#else
    QSKIP("TerminalInterfaceV2 not enabled", SkipSingle);
#endif
}

KParts::Part *TerminalInterfaceTest::createPart()
{
    auto konsolePartPlugin = KPluginLoader::findPlugin(QStringLiteral("konsolepart"));
    if (konsolePartPlugin.isNull()) {
        return nullptr;
    }

    KPluginFactory *factory = KPluginLoader(konsolePartPlugin).factory();
    if (factory == nullptr) {       // not found
        return nullptr;
    }

    auto *terminalPart = factory->create<KParts::Part>(this);

    return terminalPart;
}

QTEST_MAIN(TerminalInterfaceTest)

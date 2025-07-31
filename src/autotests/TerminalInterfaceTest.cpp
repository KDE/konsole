/*
    SPDX-FileCopyrightText: 2014 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "TerminalInterfaceTest.h"
#include "../profile/Profile.h"
#include "../profile/ProfileManager.h"
#include "config-konsole.h"

// Qt
#include <QDir>
#include <QSignalSpy>

// KDE
#include <KPluginFactory>

// Others
#if defined(Q_OS_LINUX)
#include <unistd.h>
#elif defined(Q_OS_OPENBSD)
#include <fcntl.h>
#include <kvm.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

#include <QTest>

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
    if (buildPath.endsWith(QStringLiteral("/autotests"))) {
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
#if defined(Q_OS_LINUX)
    int terminalProcessId = terminal->terminalProcessId();
    const int uid = getuid();
    QFile passwdFile(QStringLiteral("/etc/passwd"));
    QString defaultExePath;

    passwdFile.open(QIODevice::ReadOnly);

    do {
        const QString userData(QString::fromUtf8(passwdFile.readLine()));
        const QStringList dataFields(userData.split(QLatin1Char(':')));
        if (dataFields.at(2).toInt() == uid) {
            defaultExePath = dataFields.at(6);
        }
    } while (defaultExePath.isEmpty());

    passwdFile.close();

    QFile procExeTarget(QStringLiteral("/proc/%1/exe").arg(terminalProcessId));
    if (procExeTarget.exists()) {
        const auto procExeTargetNormalized = QFileInfo(procExeTarget.symLinkTarget()).canonicalFilePath();
        const auto defaultExePathNormalized = QFileInfo(defaultExePath.trimmed()).canonicalFilePath();
        QVERIFY(!procExeTargetNormalized.isEmpty());
        QVERIFY(!defaultExePathNormalized.isEmpty());
        QCOMPARE(procExeTargetNormalized, defaultExePathNormalized);
    }
#endif

    // Nothing running in foreground
    foregroundProcessId = terminal->foregroundProcessId();
    QCOMPARE(foregroundProcessId, -1);
    foregroundProcessName = terminal->foregroundProcessName();
    QCOMPARE(foregroundProcessName, QString());

    delete _terminalPart;
}

// Test with default shell running using QSignalSpy
void TerminalInterfaceTest::testTerminalInterfaceUsingSpy()
{
#if defined(Q_OS_WIN)
    return;
#endif
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

    // Let's try using QSignalSpy
    // https://community.kde.org/Guidelines_and_HOWTOs/UnitTests
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
    stateSpy.wait(5000);
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
    stateSpy.wait(2500);
    QCOMPARE(stateSpy.count(), 0);

    // Should be no change since the above cd didn't work
    const QString currentWorkingDirectory2 = terminal->currentWorkingDirectory();
    QCOMPARE(currentWorkingDirectory2, currentDirectory);

    // Test starting a new program
    QString command = QStringLiteral("top");
    terminal->sendInput(command + QLatin1Char('\n'));
    stateSpy.wait(2500);
    foregroundProcessId = terminal->foregroundProcessId();
    QVERIFY(foregroundProcessId != -1);

// Check that pid indeed belongs to a process running 'top'
#if defined(Q_OS_LINUX)
    QFile procInfo(QStringLiteral("/proc/%1/stat").arg(foregroundProcessId));
    QVERIFY(procInfo.open(QIODevice::ReadOnly));
    const QString data(QString::fromUtf8(procInfo.readAll()));
    const int nameStartIdx = data.indexOf(QLatin1Char('(')) + 1;
    const QString name(data.mid(nameStartIdx, data.lastIndexOf(QLatin1Char(')')) - nameStartIdx));
    QCOMPARE(name, command);
#elif defined(Q_OS_SOLARIS)
    QFile procExeTarget(QStringLiteral("/proc/%1/execname").arg(foregroundProcessId));
    QVERIFY(procExeTarget.open(QIODevice::ReadOnly));
    QCOMPARE(QStringLiteral(procExeTarget.readAll()), command);
#elif defined(Q_OS_OPENBSD)
    kvm_t *kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, NULL);
    int count;
    auto kProcStruct = ::kvm_getprocs(kd, KERN_PROC_PID, foregroundProcessId, sizeof(struct kinfo_proc), &count);
    QCOMPARE(count, 1);
    QCOMPARE(QString::fromUtf8(kProcStruct->p_comm), command);
    kvm_close(kd);
#endif

    // check that Terminal::foregroundProcessName outputs name of correct command
    foregroundProcessName = terminal->foregroundProcessName();
    QCOMPARE(foregroundProcessName, command);

    terminal->sendInput(QStringLiteral("q"));
    stateSpy.wait(2500);

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

// Oct 2023: KParts merge TerminalInterfaceV2 into TerminalInterface
void TerminalInterfaceTest::testTerminalInterfaceV2()
{
    // Use the built-in profile for testing
    Profile::Ptr testProfile = ProfileManager::instance()->builtinProfile();

    _terminalPart = createPart();
    if (_terminalPart == nullptr) {
        QFAIL("konsolepart not found.");
    }

    TerminalInterface *terminal = qobject_cast<TerminalInterface *>(_terminalPart);

    QVERIFY(terminal);
    QVERIFY(terminal->setCurrentProfile(testProfile->name()));
    QCOMPARE(terminal->currentProfileName(), testProfile->name());

    QCOMPARE(terminal->profileProperty(QStringLiteral("Path")), testProfile->path());
    QCOMPARE(terminal->profileProperty(QStringLiteral("SilenceSeconds")), testProfile->silenceSeconds());
    QCOMPARE(terminal->profileProperty(QStringLiteral("Icon")), testProfile->icon());
    QCOMPARE(terminal->profileProperty(QStringLiteral("ShowTerminalSizeHint")), testProfile->showTerminalSizeHint());
    QCOMPARE(terminal->profileProperty(QStringLiteral("Environment")), testProfile->environment());
    QCOMPARE(terminal->profileProperty(QStringLiteral("BellMode")), testProfile->property<QVariant>(Profile::Property::BellMode));
}

KParts::Part *TerminalInterfaceTest::createPart()
{
    const KPluginMetaData metaData(QStringLiteral("konsolepart"));
    Q_ASSERT(metaData.isValid());

    KPluginFactory::Result<KParts::Part> result = KPluginFactory::instantiatePlugin<KParts::Part>(metaData, this);
    Q_ASSERT(result);

    return result.plugin;
}

QTEST_MAIN(TerminalInterfaceTest)

#include "moc_TerminalInterfaceTest.cpp"

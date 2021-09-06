/*
    SPDX-FileCopyrightText: 2010 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "DBusTest.h"
#include "../profile/Profile.h"
#include "../profile/ProfileWriter.h"
#include "../session/Session.h"

// Qt
#include <QProcess>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QStandardPaths>

using namespace Konsole;

/* Exec a new Konsole and grab its dbus */
void DBusTest::initTestCase()
{
    qRegisterMetaType<QProcess::ExitStatus>();

    const QString interfaceName = QStringLiteral("org.kde.konsole");
    QDBusConnectionInterface *bus = nullptr;
    QStringList konsoleServices;

    if (!QDBusConnection::sessionBus().isConnected() || ((bus = QDBusConnection::sessionBus().interface()) == nullptr)) {
        QFAIL("Session bus not found");
    }

    QDBusReply<QStringList> serviceReply = bus->registeredServiceNames();
    QVERIFY2(serviceReply.isValid(), "SessionBus interfaces not available");

    // Find all current Konsoles' services running
    QStringList allServices = serviceReply;
    for (QStringList::const_iterator it = allServices.constBegin(), end = allServices.constEnd(); it != end; ++it) {
        const QString service = *it;
        if (service.startsWith(interfaceName)) {
            konsoleServices << service;
        }
    }

    // Create test profile
    auto profile = Profile::Ptr(new Profile());
    auto profileWriter = new ProfileWriter();

    do {
        _testProfileName = QStringLiteral("konsole-dbus-test-profile-%1").arg(QRandomGenerator::global()->generate());
        profile->setProperty(Profile::UntranslatedName, _testProfileName);
        profile->setProperty(Profile::Name, _testProfileName);
        _testProfilePath = profileWriter->getPath(profile);
    } while (QFile::exists(_testProfilePath));
    _testProfileEnv = QStringLiteral("TEST_PROFILE=%1").arg(_testProfileName);
    profile->setProperty(Profile::Environment, QStringList(_testProfileEnv));
    // %D = Current Directory (Long) - hacky way to check current directory
    profile->setProperty(Profile::LocalTabTitleFormat, QStringLiteral("%D"));
    profileWriter->writeProfile(_testProfilePath, profile);

    // Create a new Konsole with a separate process id
    _process = new QProcess;
    _process->setProcessChannelMode(QProcess::ForwardedChannels);
    _process->start(QStringLiteral("konsole"), QStringList(QStringLiteral("--separate")));

    if (!_process->waitForStarted()) {
        QFAIL(QStringLiteral("Unable to exec a new Konsole").toLatin1().data());
    }

    // Wait for above Konsole to finish starting
    QThread::sleep(5);

    serviceReply = bus->registeredServiceNames();
    QVERIFY2(serviceReply.isValid(), "SessionBus interfaces not available");

    // Find dbus service of above Konsole
    allServices = serviceReply;
    for (QStringList::const_iterator it = allServices.constBegin(), end = allServices.constEnd(); it != end; ++it) {
        const QString service = *it;
        if (service.startsWith(interfaceName)) {
            if (!konsoleServices.contains(service)) {
                _interfaceName = service;
            }
        }
    }

    QVERIFY2(!_interfaceName.isEmpty(), "This test will only work in a Konsole window with a new PID.  A new Konsole PID can't be found.");

    QDBusInterface iface(_interfaceName, QStringLiteral("/Windows/1"), QStringLiteral("org.kde.konsole.Konsole"));
    QVERIFY(iface.isValid());
}

/* Close the Konsole window that was opened to test the DBus interface
 */
void DBusTest::cleanupTestCase()
{
    // Remove test profile
    QFile::remove(_testProfilePath);

    // Need to take care of when user has CloseAllTabs=False otherwise
    // they will get a popup dialog when we try to close this.
    QSignalSpy quitSpy(_process, SIGNAL(finished(int, QProcess::ExitStatus)));

    // Do not use QWidget::close(), as it shows question popup when
    // CloseAllTabs is set to false (default).
    QDBusInterface iface(_interfaceName, QStringLiteral("/MainApplication"), QStringLiteral("org.qtproject.Qt.QCoreApplication"));
    QVERIFY2(iface.isValid(), "Unable to get a dbus interface to Konsole!");

    QDBusReply<void> instanceReply = iface.call(QStringLiteral("quit"));
    if (!instanceReply.isValid()) {
        QFAIL(QStringLiteral("Unable to close Konsole: %1").arg(instanceReply.error().message()).toLatin1().data());
    }
    quitSpy.wait();
    _process->kill();
    _process->deleteLater();
}

void DBusTest::testSessions()
{
    QDBusReply<void> voidReply;
    QDBusReply<bool> boolReply;
    QDBusReply<QByteArray> arrayReply;
    QDBusReply<QString> stringReply;
    QDBusReply<QStringList> listReply;

    QDBusInterface iface(_interfaceName, QStringLiteral("/Sessions/1"), QStringLiteral("org.kde.konsole.Session"));
    QVERIFY(iface.isValid());

    //****************** Test is/set MonitorActivity
    voidReply = iface.call(QStringLiteral("setMonitorActivity"), false);
    QVERIFY(voidReply.isValid());

    boolReply = iface.call(QStringLiteral("isMonitorActivity"));
    QVERIFY(boolReply.isValid());
    QCOMPARE(boolReply.value(), false);

    voidReply = iface.call(QStringLiteral("setMonitorActivity"), true);
    QVERIFY(voidReply.isValid());

    boolReply = iface.call(QStringLiteral("isMonitorActivity"));
    QVERIFY(boolReply.isValid());
    QCOMPARE(boolReply.value(), true);

    //****************** Test is/set MonitorSilence
    voidReply = iface.call(QStringLiteral("setMonitorSilence"), false);
    QVERIFY(voidReply.isValid());

    boolReply = iface.call(QStringLiteral("isMonitorSilence"));
    QVERIFY(boolReply.isValid());
    QCOMPARE(boolReply.value(), false);

    voidReply = iface.call(QStringLiteral("setMonitorSilence"), true);
    QVERIFY(voidReply.isValid());

    boolReply = iface.call(QStringLiteral("isMonitorSilence"));
    QVERIFY(boolReply.isValid());
    QCOMPARE(boolReply.value(), true);

    //****************** Test codec and setCodec
    arrayReply = iface.call(QStringLiteral("codec"));
    QVERIFY(arrayReply.isValid());

    // Obtain a list of system's Codecs
    QList<QByteArray> availableCodecs = QTextCodec::availableCodecs();
    for (int i = 0; i < availableCodecs.count(); ++i) {
        boolReply = iface.call(QStringLiteral("setCodec"), availableCodecs[i]);
        QVERIFY(boolReply.isValid());
        QCOMPARE(boolReply.value(), true);

        arrayReply = iface.call(QStringLiteral("codec"));
        QVERIFY(arrayReply.isValid());
        // Compare result with name due to aliases issue
        // Better way to do this?
        QCOMPARE((QTextCodec::codecForName(arrayReply.value()))->name(), (QTextCodec::codecForName(availableCodecs[i]))->name());
    }

    //****************** Test is/set flowControlEnabled
    voidReply = iface.call(QStringLiteral("setFlowControlEnabled"), true);
    QVERIFY(voidReply.isValid());

    boolReply = iface.call(QStringLiteral("flowControlEnabled"));
    QVERIFY(boolReply.isValid());
    QCOMPARE(boolReply.value(), true);

    voidReply = iface.call(QStringLiteral("setFlowControlEnabled"), false);
    QVERIFY(voidReply.isValid());

    boolReply = iface.call(QStringLiteral("flowControlEnabled"));
    QVERIFY(boolReply.isValid());
    QCOMPARE(boolReply.value(), false);

    //****************** Test is/set environment
    listReply = iface.call(QStringLiteral("environment"));
    QVERIFY(listReply.isValid());

    QStringList prevEnv = listReply.value();
    // for (int i = 0; i < prevEnv.size(); ++i)
    //    qDebug()<< prevEnv.at(i).toLocal8Bit().constData() << endl;

    voidReply = iface.call(QStringLiteral("setEnvironment"), QStringList());
    QVERIFY(voidReply.isValid());

    listReply = iface.call(QStringLiteral("environment"));
    QVERIFY(listReply.isValid());
    QCOMPARE(listReply.value(), QStringList());

    voidReply = iface.call(QStringLiteral("setEnvironment"), prevEnv);
    QVERIFY(voidReply.isValid());

    listReply = iface.call(QStringLiteral("environment"));
    QVERIFY(listReply.isValid());
    QCOMPARE(listReply.value(), prevEnv);

    //****************** Test is/set title
    // TODO: Consider checking what is in Profile
    stringReply = iface.call(QStringLiteral("title"), Session::NameRole);
    QVERIFY(stringReply.isValid());

    // set title to,  what title should be
    QMap<QString, QString> titleMap;
    titleMap[QStringLiteral("Shell")] = QStringLiteral("Shell");

    // BUG: It appears that Session::LocalTabTitle is set to Shell and
    // doesn't change.  While RemoteTabTitle is actually the LocalTabTitle
    // TODO: Figure out what's going on...
    QMapIterator<QString, QString> i(titleMap);
    while (i.hasNext()) {
        i.next();
        voidReply = iface.call(QStringLiteral("setTitle"), Session::NameRole, i.key());
        QVERIFY(voidReply.isValid());

        stringReply = iface.call(QStringLiteral("title"), Session::NameRole);
        QVERIFY(stringReply.isValid());

        QCOMPARE(stringReply.value(), i.value());
    }
}

void DBusTest::testWindows()
{
    // Tested functions:
    // [+] int sessionCount();
    // [+] QStringList sessionList();
    // [+] int currentSession();
    // [+] void setCurrentSession(int sessionId);
    // [+] int newSession();
    // [+] int newSession(const QString &profile);
    // [+] int newSession(const QString &profile, const QString &directory);
    // [ ] QString defaultProfile();
    // [ ] QStringList profileList();
    // [ ] void nextSession();
    // [ ] void prevSession();
    // [ ] void moveSessionLeft();
    // [ ] void moveSessionRight();
    // [ ] void setTabWidthToText(bool);

    QDBusReply<int> intReply;
    QDBusReply<QStringList> listReply;
    QDBusReply<void> voidReply;
    QDBusReply<QString> stringReply;

    int sessionCount = -1;
    int initialSessionId = -1;

    QDBusInterface iface(_interfaceName, QStringLiteral("/Windows/1"), QStringLiteral("org.kde.konsole.Window"));
    QVERIFY(iface.isValid());

    intReply = iface.call(QStringLiteral("sessionCount"));
    QVERIFY(intReply.isValid());
    sessionCount = intReply.value();
    QVERIFY(sessionCount > 0);

    intReply = iface.call(QStringLiteral("currentSession"));
    QVERIFY(intReply.isValid());
    initialSessionId = intReply.value();

    intReply = iface.call(QStringLiteral("newSession"));
    QVERIFY(intReply.isValid());
    ++sessionCount;
    int newSessionId = intReply.value();
    QVERIFY(newSessionId != initialSessionId);

    listReply = iface.call(QStringLiteral("sessionList"));
    QVERIFY(listReply.isValid());

    QStringList sessions = listReply.value();
    QVERIFY(sessions.contains(QString::number(initialSessionId)));
    QVERIFY(sessions.contains(QString::number(newSessionId)));
    QVERIFY(sessions.size() == sessionCount);

    intReply = iface.call(QStringLiteral("newSession"), _testProfileName);
    QVERIFY(intReply.isValid());
    ++sessionCount;
    newSessionId = intReply.value();
    QVERIFY(newSessionId != initialSessionId);
    {
        QDBusInterface sessionIface(_interfaceName, QStringLiteral("/Sessions/%1").arg(newSessionId), QStringLiteral("org.kde.konsole.Session"));
        QVERIFY(iface.isValid());

        listReply = sessionIface.call(QStringLiteral("environment"));
        QVERIFY(listReply.isValid());
        QStringList env = listReply.value();
        QVERIFY(env.contains(_testProfileEnv));
    }

    const auto tempDirectories = QStandardPaths::standardLocations(QStandardPaths::TempLocation);
    const QString sessionDirectory = tempDirectories.empty() ? QStringLiteral("/") : tempDirectories.constFirst();
    intReply = iface.call(QStringLiteral("newSession"), _testProfileName, sessionDirectory);
    QVERIFY(intReply.isValid());
    ++sessionCount;
    newSessionId = intReply.value();
    QVERIFY(newSessionId != initialSessionId);
    {
        QDBusInterface sessionIface(_interfaceName, QStringLiteral("/Sessions/%1").arg(newSessionId), QStringLiteral("org.kde.konsole.Session"));
        QVERIFY(iface.isValid());

        listReply = sessionIface.call(QStringLiteral("environment"));
        QVERIFY(listReply.isValid());
        QStringList env = listReply.value();
        QVERIFY(env.contains(_testProfileEnv));

        // Apparently there's no function for checking CWD.
        // The profile uses "%D" as title format, so check the title
        stringReply = sessionIface.call(QStringLiteral("title"), Session::DisplayedTitleRole);
        QVERIFY(stringReply.isValid());
        QVERIFY(QDir(stringReply.value()) == QDir(sessionDirectory));
    }

    voidReply = iface.call(QStringLiteral("setCurrentSession"), initialSessionId);
    QVERIFY(voidReply.isValid());

    intReply = iface.call(QStringLiteral("currentSession"));
    QVERIFY(intReply.isValid());
    QVERIFY(intReply.value() == initialSessionId);

    intReply = iface.call(QStringLiteral("sessionCount"));
    QVERIFY(intReply.isValid());
    QVERIFY(intReply.value() == sessionCount);
}

QTEST_MAIN(DBusTest)

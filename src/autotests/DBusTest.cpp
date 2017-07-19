/*
    Copyright 2010 by Kurt Hindenburg <kurt.hindenburg@gmail.com>

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
#include "DBusTest.h"
#include "../Session.h"
#include <KProcess>

using namespace Konsole;

/* Exec a new Konsole and grab its dbus */
void DBusTest::initTestCase()
{
    const QString interfaceName = QStringLiteral("org.kde.konsole");
    QDBusConnectionInterface *bus = nullptr;
    QStringList konsoleServices;

    if (!QDBusConnection::sessionBus().isConnected()
        || ((bus = QDBusConnection::sessionBus().interface()) == nullptr)) {
        QFAIL("Session bus not found");
    }

    QDBusReply<QStringList> serviceReply = bus->registeredServiceNames();
    QVERIFY2(serviceReply.isValid(), "SessionBus interfaces not available");

    // Find all current Konsoles' services running
    QStringList allServices = serviceReply;
    for (QStringList::const_iterator it = allServices.constBegin(),
         end = allServices.constEnd(); it != end; ++it) {
        const QString service = *it;
        if (service.startsWith(interfaceName)) {
            konsoleServices << service;
        }
    }

    // Create a new Konsole with a separate process id
    int pid = KProcess::startDetached(QStringLiteral("konsole"),
                                      QStringList(QStringLiteral("--separate")));
    if (pid == 0) {
        QFAIL(QStringLiteral("Unable to exec a new Konsole").toLatin1().data());
    }

    // Wait for above Konsole to finish starting
#if defined(HAVE_USLEEP)
    usleep(5 * 1000);
#else
    sleep(5);
#endif

    serviceReply = bus->registeredServiceNames();
    QVERIFY2(serviceReply.isValid(), "SessionBus interfaces not available");

    // Find dbus service of above Konsole
    allServices = serviceReply;
    for (QStringList::const_iterator it = allServices.constBegin(),
         end = allServices.constEnd(); it != end; ++it) {
        const QString service = *it;
        if (service.startsWith(interfaceName)) {
            if (!konsoleServices.contains(service)) {
                _interfaceName = service;
            }
        }
    }

    QVERIFY2(!_interfaceName.isEmpty(),
             "This test will only work in a Konsole window with a new PID.  A new Konsole PID can't be found.");

    QDBusInterface iface(_interfaceName,
                         QStringLiteral("/Windows/1"),
                         QStringLiteral("org.kde.konsole.Konsole"));
    QVERIFY(iface.isValid());
}

/* Close the Konsole window that was opened to test the DBus interface
 */
void DBusTest::cleanupTestCase()
{
    // Need to take care of when user has CloseAllTabs=False otherwise
    // they will get a popup dialog when we try to close this.

    QDBusInterface iface(_interfaceName,
                         QStringLiteral("/konsole/MainWindow_1"),
                         QStringLiteral("org.qtproject.Qt.QWidget"));
    QVERIFY2(iface.isValid(), "Unable to get a dbus interface to Konsole!");

    QDBusReply<void> instanceReply = iface.call(QStringLiteral("close"));
    if (!instanceReply.isValid()) {
        QFAIL(QStringLiteral("Unable to close Konsole: %1").arg(instanceReply.error().message()).toLatin1().data());
    }
}

void DBusTest::testSessions()
{
    QDBusReply<void> voidReply;
    QDBusReply<bool> boolReply;
    QDBusReply<QByteArray> arrayReply;
    QDBusReply<QString> stringReply;
    QDBusReply<QStringList> listReply;

    QDBusInterface iface(_interfaceName,
                         QStringLiteral("/Sessions/1"),
                         QStringLiteral("org.kde.konsole.Session"));
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
        QCOMPARE((QTextCodec::codecForName(arrayReply.value()))->name(),
                 (QTextCodec::codecForName(availableCodecs[i]))->name());
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
    //for (int i = 0; i < prevEnv.size(); ++i)
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
    stringReply = iface.call(QStringLiteral("title"), Session::LocalTabTitle);
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
        voidReply = iface.call(QStringLiteral("setTitle"), Session::LocalTabTitle, i.key());
        QVERIFY(voidReply.isValid());

        stringReply = iface.call(QStringLiteral("title"), Session::LocalTabTitle);
        QVERIFY(stringReply.isValid());

        QCOMPARE(stringReply.value(), i.value());
    }
}

QTEST_MAIN(DBusTest)

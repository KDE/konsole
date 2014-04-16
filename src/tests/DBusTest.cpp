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

using namespace Konsole;

/* Exec a new Konsole and grab its dbus */
void DBusTest::initTestCase()
{
    const QString interfaceName = "org.kde.konsole";
    QDBusConnectionInterface* bus = 0;
    QStringList konsoleServices;

    if (!QDBusConnection::sessionBus().isConnected() ||
            !(bus = QDBusConnection::sessionBus().interface()))
        kFatal() << "Session bus not found";

    QDBusReply<QStringList> serviceReply = bus->registeredServiceNames();
    if (!serviceReply.isValid())
        kFatal() << "SessionBus interfaces not available";

    // Find all current Konsoles' services running
    QStringList allServices = serviceReply;
    for (QStringList::const_iterator it = allServices.constBegin(),
            end = allServices.constEnd(); it != end; ++it) {
        const QString service = *it;
        if (service.startsWith(interfaceName))
            konsoleServices << service;
    }

    // Create a new Konsole with a separate process id
    int result = KProcess::execute("konsole");
    if (result)
        kFatal() << "Unable to exec a new Konsole : " << result;

    // Wait for above Konsole to finish starting
#if defined(HAVE_USLEEP)
    usleep(5 * 1000);
#else
    sleep(5);
#endif

    serviceReply = bus->registeredServiceNames();
    if (!serviceReply.isValid())
        kFatal() << "SessionBus interfaces not available";

    // Find dbus service of above Konsole
    allServices = serviceReply;
    for (QStringList::const_iterator it = allServices.constBegin(),
            end = allServices.constEnd(); it != end; ++it) {
        const QString service = *it;
        if (service.startsWith(interfaceName))
            if (!konsoleServices.contains(service))
                _interfaceName = service;
    }

    if (_interfaceName.isEmpty()) {
        kFatal() << "This test will only work in a Konsole window with a new PID.  A new Konsole PID can't be found.";
    }
    //kDebug()<< "Using service: " + _interfaceName.toLatin1();

    QDBusInterface iface(_interfaceName,
                         QLatin1String("/Konsole"),
                         QLatin1String("org.kde.konsole.Konsole"));
    QVERIFY(iface.isValid());
}

/* Close the Konsole window that was opened to test the DBus interface
 */
void DBusTest::cleanupTestCase()
{
    // Need to take care of when user has CloseAllTabs=False otherwise
    // they will get a popup dialog when we try to close this.

    QDBusInterface iface(_interfaceName,
                         QLatin1String("/konsole/MainWindow_1"),
#if QT_VERSION < 0x040800
                         QLatin1String("com.trolltech.Qt.QWidget"));
#else
                         QLatin1String("org.qtproject.Qt.QWidget"));
#endif
    if (!iface.isValid())
        kFatal() << "Unable to get a dbus interface to Konsole!";

    QDBusReply<void> instanceReply = iface.call("close");
    if (!instanceReply.isValid())
        kFatal() << "Unable to close Konsole :" << instanceReply.error();
}

void DBusTest::testSessions()
{
    QDBusReply<void> voidReply;
    QDBusReply<bool> boolReply;
    QDBusReply<QByteArray> arrayReply;
    QDBusReply<QString> stringReply;
    QDBusReply<QStringList> listReply;

    QDBusInterface iface(_interfaceName,
                         QLatin1String("/Sessions/1"),
                         QLatin1String("org.kde.konsole.Session"));
    QVERIFY(iface.isValid());

    //****************** Test is/set MonitorActivity
    voidReply = iface.call("setMonitorActivity", false);
    QVERIFY(voidReply.isValid());

    boolReply = iface.call("isMonitorActivity");
    QVERIFY(boolReply.isValid());
    QCOMPARE(boolReply.value(), false);

    voidReply = iface.call("setMonitorActivity", true);
    QVERIFY(voidReply.isValid());

    boolReply = iface.call("isMonitorActivity");
    QVERIFY(boolReply.isValid());
    QCOMPARE(boolReply.value(), true);

    //****************** Test is/set MonitorSilence
    voidReply = iface.call("setMonitorSilence", false);
    QVERIFY(voidReply.isValid());

    boolReply = iface.call("isMonitorSilence");
    QVERIFY(boolReply.isValid());
    QCOMPARE(boolReply.value(), false);

    voidReply = iface.call("setMonitorSilence", true);
    QVERIFY(voidReply.isValid());

    boolReply = iface.call("isMonitorSilence");
    QVERIFY(boolReply.isValid());
    QCOMPARE(boolReply.value(), true);

    //****************** Test codec and setCodec
    arrayReply = iface.call("codec");
    QVERIFY(arrayReply.isValid());

    // Obtain a list of system's Codecs
    QList<QByteArray> availableCodecs = QTextCodec::availableCodecs();
    for (int i = 0; i < availableCodecs.count(); ++i) {
        boolReply = iface.call("setCodec", availableCodecs[i]);
        QVERIFY(boolReply.isValid());
        QCOMPARE(boolReply.value(), true);

        arrayReply = iface.call("codec");
        QVERIFY(arrayReply.isValid());
        // Compare result with name due to aliases issue
        // Better way to do this?
        QCOMPARE((QTextCodec::codecForName(arrayReply.value()))->name(),
                 (QTextCodec::codecForName(availableCodecs[i]))->name());
    }

    //****************** Test is/set flowControlEnabled
    voidReply = iface.call("setFlowControlEnabled", true);
    QVERIFY(voidReply.isValid());

    boolReply = iface.call("flowControlEnabled");
    QVERIFY(boolReply.isValid());
    QCOMPARE(boolReply.value(), true);

    voidReply = iface.call("setFlowControlEnabled", false);
    QVERIFY(voidReply.isValid());

    boolReply = iface.call("flowControlEnabled");
    QVERIFY(boolReply.isValid());
    QCOMPARE(boolReply.value(), false);

    //****************** Test is/set environment
    listReply = iface.call("environment");
    QVERIFY(listReply.isValid());

    QStringList prevEnv = listReply.value();
    //for (int i = 0; i < prevEnv.size(); ++i)
    //    kDebug()<< prevEnv.at(i).toLocal8Bit().constData() << endl;

    voidReply = iface.call("setEnvironment", QStringList());
    QVERIFY(voidReply.isValid());

    listReply = iface.call("environment");
    QVERIFY(listReply.isValid());
    QCOMPARE(listReply.value(), QStringList());

    voidReply = iface.call("setEnvironment", prevEnv);
    QVERIFY(voidReply.isValid());

    listReply = iface.call("environment");
    QVERIFY(listReply.isValid());
    QCOMPARE(listReply.value(), prevEnv);

    //****************** Test is/set title
    // TODO: Consider checking what is in Profile
    stringReply = iface.call("title", Session::LocalTabTitle);
    QVERIFY(stringReply.isValid());

    //kDebug()<< stringReply.value();
    QString prevLocalTitle = stringReply.value();

    // set title to,  what title should be
    QMap<QString, QString> titleMap;
    titleMap["Shell"] = "Shell";

    // BUG: It appears that Session::LocalTabTitle is set to Shell and
    // doesn't change.  While RemoteTabTitle is actually the LocalTabTitle
    // TODO: Figure out what's going on...
    QMapIterator<QString, QString> i(titleMap);
    while (i.hasNext()) {
        i.next();
        voidReply = iface.call("setTitle", Session::LocalTabTitle, i.key());
        QVERIFY(voidReply.isValid());

        stringReply = iface.call("title", Session::LocalTabTitle);
        QVERIFY(stringReply.isValid());

        QCOMPARE(stringReply.value(), i.value());

    }
}

QTEST_MAIN(DBusTest)

#include "DBusTest.moc"


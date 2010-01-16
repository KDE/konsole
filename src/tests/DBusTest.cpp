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

// KDE
#include <qtest_kde.h>

using namespace Konsole;

/* We need to find the Konsole DBus service.  Depending on the user's setup,
 * this could be org.kde.konsole or org.kde.konsole-PID#.  Since QTest
 * doesn't accept user command line options, we will just pick the last
 * Konsole service.
 */
DBusTest::DBusTest()
{
    init();
    newInstance();
}

void DBusTest::init()
{
    const QString interfaceName = "org.kde.konsole";
    _originalSessionCount = 0;
    QDBusConnectionInterface* bus = 0;

    if (!QDBusConnection::sessionBus().isConnected() ||
            !(bus = QDBusConnection::sessionBus().interface()))
        kFatal() << "Session bus not found";

    QDBusReply<QStringList> serviceReply = bus->registeredServiceNames();
    if (!serviceReply.isValid())
        kFatal() << "SessionBus interfaces not available";

    const QStringList allServices = serviceReply;
    for (QStringList::const_iterator it = allServices.begin(),
            end = allServices.end(); it != end; ++it) {
        const QString service = *it;
        if (service.startsWith(interfaceName))
            _interfaceName = service;
    }

    if (_interfaceName.isEmpty())
        kFatal() << "No services matching " << interfaceName << " were found.";
    //kDebug()<< "Using service: " + _interfaceName.toLatin1();

    _iface = new QDBusInterface(_interfaceName,
                                QLatin1String("/Konsole"),
                                QLatin1String("org.kde.konsole.Konsole"),
                                QDBusConnection::sessionBus(), this);

    QDBusReply<int> sessionCount = _iface->call("sessionCount");
    QVERIFY(sessionCount.isValid());
    _originalSessionCount = sessionCount;
    //kDebug()<<"sessionCount "<<sessionCount.value();
}

void DBusTest::newInstance()
{
    QDBusInterface iface(_interfaceName,
                         QLatin1String("/MainApplication"),
                         QLatin1String("org.kde.KUniqueApplication"));
    if (!iface.isValid())
        kFatal() << "Unable to get a dbus interface to Konsole!";

    QDBusReply<void> instanceReply = iface.call("newInstance");
    if (!instanceReply.isValid())
        kFatal() << "Unable to start a newInstance of Konsole: " << instanceReply.error();

}

void DBusTest::initTestCase()
{
    QVERIFY(_iface);
    QVERIFY(_iface->isValid());
}

/* Close the Konsole window that was opened to test the DBus interface
 */
void DBusTest::cleanupTestCase()
{
    QVERIFY(_iface);
    QVERIFY(_iface->isValid());

    // How to handle when profile has CloseAllTabs=false?
    // Assume that testing window is MainWindow_2?
    QDBusInterface iface(_interfaceName,
                         QLatin1String("/konsole/MainWindow_2"),
                         QLatin1String("com.trolltech.Qt.QWidget"));
    if (!iface.isValid())
        kFatal() << "Unable to get a dbus interface to Konsole!";

    QDBusReply<void> instanceReply = iface.call("close");
    if (!instanceReply.isValid())
        kFatal() << "Unable to close MainWindow_2 :" << instanceReply.error();

    QDBusReply<int> sessionCount = _iface->call("sessionCount");
    QVERIFY(sessionCount.isValid());
    QCOMPARE(sessionCount.value(), _originalSessionCount);

}

void DBusTest::testSessions()
{
    QVERIFY(_iface);
    QVERIFY(_iface->isValid());

}

QTEST_MAIN(DBusTest)

#include "DBusTest.moc"



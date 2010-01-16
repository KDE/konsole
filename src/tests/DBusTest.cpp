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
//#include <qtest_kde.h>

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
    for (QStringList::const_iterator it = allServices.begin(),
            end = allServices.end(); it != end; ++it) {
        const QString service = *it;
        if (service.startsWith(interfaceName))
            konsoleServices << service;
    }

    // Create a new Konsole
    int result = KToolInvocation::kdeinitExec("konsole");
    if (result)
        kFatal() << "Unable to exec a new Konsole : " << result;

    // Wait for above Konsole to finish starting
#ifdef HAVE_USLEEP
    usleep(5*1000);
#else
    sleep(5);
#endif

    serviceReply = bus->registeredServiceNames();
    if (!serviceReply.isValid())
        kFatal() << "SessionBus interfaces not available";

    // Find dbus service of above Konsole
    allServices = serviceReply;
    for (QStringList::const_iterator it = allServices.begin(),
            end = allServices.end(); it != end; ++it) {
        const QString service = *it;
        if (service.startsWith(interfaceName))
            if (!konsoleServices.contains(service))
                _interfaceName = service;
    }

    if (_interfaceName.isEmpty())
        kFatal() << "No services matching " << interfaceName << " were found.";
    //kDebug()<< "Using service: " + _interfaceName.toLatin1();

    _iface = new QDBusInterface(_interfaceName,
                                QLatin1String("/Konsole"),
                                QLatin1String("org.kde.konsole.Konsole"),
                                QDBusConnection::sessionBus(), this);
    QVERIFY(_iface);
    QVERIFY(_iface->isValid());
}

/* Close the Konsole window that was opened to test the DBus interface
 */
void DBusTest::cleanupTestCase()
{
    QVERIFY(_iface);
    QVERIFY(_iface->isValid());

    QDBusInterface iface(_interfaceName,
                         QLatin1String("/konsole/MainWindow_1"),
                         QLatin1String("com.trolltech.Qt.QWidget"));
    if (!iface.isValid())
        kFatal() << "Unable to get a dbus interface to Konsole!";

    QDBusReply<void> instanceReply = iface.call("close");
    if (!instanceReply.isValid())
        kFatal() << "Unable to close MainWindow_2 :" << instanceReply.error();
}

void DBusTest::testSessions()
{
    QVERIFY(_iface);
    QVERIFY(_iface->isValid());
}

QTEST_MAIN(DBusTest)

#include "DBusTest.moc"



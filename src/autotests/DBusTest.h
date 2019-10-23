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

#ifndef DBUSTEST_H
#define DBUSTEST_H

#include <QTest>
#include <QDBusInterface>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QTextCodec>

#include <unistd.h>

class QProcess;

namespace Konsole
{

class DBusTest : public QObject
{
    Q_OBJECT
public:

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testSessions();
    void testWindows();

// protected slots are not treated as test cases
protected Q_SLOTS:

private:
    QString _interfaceName;
    QProcess *_process = nullptr;

    QString _testProfileName;
    QString _testProfilePath;
    QString _testProfileEnv;
};

}

#endif // DBUSTEST_H


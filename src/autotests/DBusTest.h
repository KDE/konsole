/*
    SPDX-FileCopyrightText: 2010 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef DBUSTEST_H
#define DBUSTEST_H

#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QTest>
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

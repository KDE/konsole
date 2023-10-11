/*
    SPDX-FileCopyrightText: 2023 Theodore Wang <theodorewang12@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "ProcessInfoTest.h"

// Qt
#include <QDir>
#include <QString>
#include <QTest>

// Konsole
#include "../ProcessInfo.h"
#include "../session/Session.h"

// Others
#include <memory>

#include <QTest>

using namespace Konsole;

std::unique_ptr<ProcessInfo> ProcessInfoTest::createProcInfo(const KProcess &proc)
{
    return std::unique_ptr<ProcessInfo>(ProcessInfo::newInstance(proc.processId()));
}

void ProcessInfoTest::testProcessValidity()
{
    if (Session::checkProgram(QStringLiteral("bash")).isEmpty())
        return;

    KProcess proc;
    proc.setProgram(QStringLiteral("bash"));
    proc.start();

    QVERIFY(createProcInfo(proc)->isValid());

    proc.close();
    proc.waitForFinished(1000);
}

void ProcessInfoTest::testProcessCwd()
{
#ifndef Q_OS_FREEBSD
    if (Session::checkProgram(QStringLiteral("bash")).isEmpty())
        return;

    KProcess proc;
    proc.setProgram({QStringLiteral("bash"), QStringLiteral("-x")});
    proc.start();

    auto procInfo = createProcInfo(proc);
    const QString startDir(QDir::currentPath());
    const QString parentDir(startDir.mid(0, startDir.lastIndexOf(QLatin1Char('/'))));

    bool ok;
    QString currDir;

    currDir = procInfo->currentDir(&ok);
    QVERIFY(ok);
    QCOMPARE(currDir, startDir);

    proc.write(QStringLiteral("cd ..\n").toLocal8Bit());
    proc.waitForReadyRead(1000);
    procInfo->update();

    currDir = procInfo->currentDir(&ok);
    QVERIFY(ok);
    QCOMPARE(currDir, parentDir);

    proc.write(QStringLiteral("exit\n").toLocal8Bit());
    proc.waitForFinished(1000);
#endif
}

void ProcessInfoTest::testProcessNameSpecialChars()
{
#ifndef Q_OS_FREEBSD
    if (Session::checkProgram(QStringLiteral("bash")).isEmpty())
        return;

    const QVector<QString> specNames({QStringLiteral("(( a("), QStringLiteral("("), QStringLiteral("ab) ("), QStringLiteral(")")});

    KProcess mainProc;
    mainProc.setProgram({QStringLiteral("bash"), QStringLiteral("-x")});
    mainProc.start();

    auto mainProcInfo = createProcInfo(mainProc);
    bool ok;

    for (auto specName : specNames) {
        mainProc.write(QStringLiteral("cp $(which bash) '%1'\n").arg(specName).toLocal8Bit());
        mainProc.waitForReadyRead(1000);
        mainProc.write(QStringLiteral("exec %1'%2'\n").arg(QDir::currentPath() + QDir::separator(), specName).toLocal8Bit());
        mainProc.waitForReadyRead(1000);

        mainProcInfo->update();

        QDir::current().remove(specName);

        const QString currName(mainProcInfo->name(&ok));
        QVERIFY(ok);
        QCOMPARE(currName, specName);
    }

    mainProc.write(QStringLiteral("exit\n").toLocal8Bit());
    mainProc.waitForFinished(1000);
#endif
}

QTEST_MAIN(ProcessInfoTest)

#include "moc_ProcessInfoTest.cpp"

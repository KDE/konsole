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

// Qt
#include <QSignalSpy>

// KDE
#include <KService>
#include <KDebug>
#include <qtest_kde.h>
//#include "../Part.h"

using namespace Konsole;

/* In KDE 4.x there are 2 versions: TerminalInterface and TerminalInterfaceV2
   In KDE 5.x only one: TerminalInterface
   The code below uses both as well the KonsolePart API
*/
void TerminalInterfaceTest::testTerminalInterface()
{
    QString currentDirectory;
    QString retVal;
    bool result;

    // create a Konsole part and attempt to connect to it
    _terminalPart = createPart();
    if (!_terminalPart)
        QSKIP("konsolepart not found.", SkipSingle);

    TerminalInterface* terminal = qobject_cast<TerminalInterface*>(_terminalPart);
    QVERIFY(terminal);
    terminal->showShellInDir(QDir::home().path());

    int foregroundProcessId  = terminal->foregroundProcessId();
    QCOMPARE(foregroundProcessId, -1);
    QString foregroundProcessName  = terminal->foregroundProcessName();
    QCOMPARE(foregroundProcessName, QString(""));

    // terminalProcessId() is the user's default shell
    // FIXME: find a way to verify this
    // int terminalProcessId  = terminal->terminalProcessId();

    // Sleep is used to allow enough time for these to work
    // In Qt5 we can use QSignalSpy::wait()

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
    currentDirectory = QString("/tmp");
    terminal->sendInput("cd " + currentDirectory + '\n');
    sleep(2000);
    QCOMPARE(stateSpy.count(), 1);

    // Correct result?
    QList<QVariant> firstSignalArgs = stateSpy.takeFirst();

    QString firstSignalState = firstSignalArgs.at(0).toString();
    QCOMPARE(firstSignalState, currentDirectory);

    // Test KonsolePart API currentWorkingDirectory()
    result = QMetaObject::invokeMethod(_terminalPart,
                                       "currentWorkingDirectory",
                                       Qt::DirectConnection,
                                       Q_RETURN_ARG(QString, retVal));
    QVERIFY(result);
    QCOMPARE(retVal, currentDirectory);

    // #1B - Test signal currentDirectoryChanged(QString)
    // Invalid directory - no signal should be emitted
    terminal->sendInput("cd /usrADADFASDF\n");
    sleep(2000);
    QCOMPARE(stateSpy.count(), 0);

    // Should be no change since the above cd didn't work
    result = QMetaObject::invokeMethod(_terminalPart,
                                       "currentWorkingDirectory",
                                       Qt::DirectConnection,
                                       Q_RETURN_ARG(QString, retVal));
    QVERIFY(result);
    QCOMPARE(retVal, currentDirectory);


    // Test starting a new program
    QString command = "top";
    terminal->sendInput(command + '\n');
    sleep(2000);
    // FIXME: find a good way to validate process id of 'top'
    foregroundProcessId  = terminal->foregroundProcessId();
    QVERIFY(foregroundProcessId != -1);
    foregroundProcessName  = terminal->foregroundProcessName();
    QCOMPARE(foregroundProcessName, command);

    terminal->sendInput("q");
    sleep(2000);

    // Nothing running in foreground
    foregroundProcessId  = terminal->foregroundProcessId();
    QCOMPARE(foregroundProcessId, -1);
    foregroundProcessName  = terminal->foregroundProcessName();
    QCOMPARE(foregroundProcessName, QString(""));

    // Test destroyed()
    QSignalSpy destroyedSpy(_terminalPart, SIGNAL(destroyed()));
    QVERIFY(destroyedSpy.isValid());
 
    // Now we check to make sure we don't have any signals already
    QCOMPARE(destroyedSpy.count(), 0);

    delete _terminalPart;
    QCOMPARE(destroyedSpy.count(), 1);

}

void TerminalInterfaceTest::sleep(int msecs)
{
    QEventLoop loop;
    QTimer::singleShot(msecs, &loop, SLOT(quit()));
    loop.exec(QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers);
}

KParts::Part* TerminalInterfaceTest::createPart()
{
    KService::Ptr service = KService::serviceByDesktopName("konsolepart");
    if (!service)       // not found
        return 0;
    KPluginFactory* factory = KPluginLoader(service->library()).factory();
    if (!factory)       // not found
        return 0;

    KParts::Part* terminalPart = factory->create<KParts::Part>(this);

    return terminalPart;
}

QTEST_KDEMAIN(TerminalInterfaceTest , GUI)

#include "TerminalInterfaceTest.moc"


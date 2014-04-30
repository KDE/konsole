/*
    Copyright 2008 by Robert Knight <robertknight@gmail.com>

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
#include "PartManualTest.h"

// Qt
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QtGui/QKeyEvent>
#include <qtestkeyboard.h>

// System
#include <termios.h>
#include <sys/types.h>

// KDE
#include <KMenu>
#include <KMenuBar>
#include <KPluginLoader>
#include <KPluginFactory>
#include <KService>
#include <KParts/Part>
#include <KPtyProcess>
#include <KPtyDevice>
#include <KDialog>
#include <KDebug>
#include <KMainWindow>
#include <qtest_kde.h>

// Konsole
#include "../Pty.h"
#include "../Session.h"
#include "../KeyboardTranslator.h"

using namespace Konsole;

void PartManualTest::testShortcutOverride()
{
    // FIXME: This test asks the user to press shortcut key sequences manually because
    // the result is different than when sending the key press via QTest::keyClick()
    //
    // When the key presses are sent manually, Konsole::TerminalDisplay::event() is called
    // and the overrideShortcut() signal is emitted by the part.
    // When the key presses are sent automatically, the shortcut is triggered but
    // Konsole::TerminalDisplay::event() is not called and the overrideShortcut() signal is
    // not emitted by the part.

    // Create a main window with a menu and a test
    // action with a shortcut set to Ctrl+S, which is also used by the terminal
    KMainWindow* mainWindow = new KMainWindow();
    QMenu* fileMenu = mainWindow->menuBar()->addMenu("File");
    QAction* testAction = fileMenu->addAction("Test");
    testAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_S));
    connect(testAction, &QAction::triggered, this, &Konsole::PartManualTest::shortcutTriggered);

    // Create terminal part and embed in into the main window
    KParts::Part* terminalPart = createPart();
    QVERIFY(terminalPart);
    mainWindow->setCentralWidget(terminalPart->widget());
    TerminalInterface* terminal = qobject_cast<TerminalInterface*>(terminalPart);
    QVERIFY(terminal);
    terminal->sendInput("Press Ctrl+S twice.\n");
    mainWindow->show();

    // Test shortcut with override disabled, so the shortcut will be triggered
    _shortcutTriggered = false;
    _override = false;
    _overrideCalled = false;
    QVERIFY(connect(terminalPart, SIGNAL(overrideShortcut(QKeyEvent*,bool&)),
                    this, SLOT(overrideShortcut(QKeyEvent*,bool&))));

    //QTest::keyClick(terminalPart->widget(),Qt::Key_S,Qt::ControlModifier);
    _shortcutEventLoop = new QEventLoop();
    _shortcutEventLoop->exec();

    QVERIFY(_overrideCalled);
    QVERIFY(_shortcutTriggered);
    QVERIFY(!_override);

    // Test shortcut with override enabled, so the shortcut will not be triggered
    _override = true;
    _overrideCalled = false;
    _shortcutTriggered = false;

    //QTest::keyClick(terminalPart->widget(),Qt::Key_S,Qt::ControlModifier);
    _shortcutEventLoop->exec();

    QVERIFY(_overrideCalled);
    QVERIFY(!_shortcutTriggered);
    QVERIFY(_override);

    delete _shortcutEventLoop;
    delete terminalPart;
    delete mainWindow;
}
void PartManualTest::overrideShortcut(QKeyEvent* event, bool& override)
{
    QVERIFY(override == true);
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_S) {
        _overrideCalled = true;
        override = _override;
        _shortcutEventLoop->exit();
    }
}
void PartManualTest::shortcutTriggered()
{
    _shortcutTriggered = true;
}

KParts::Part* PartManualTest::createPart()
{
    KService::Ptr service = KService::serviceByDesktopName("konsolepart");
    Q_ASSERT(service);
    KPluginFactory* factory = KPluginLoader(service->library()).factory();
    Q_ASSERT(factory);

    KParts::Part* terminalPart = factory->create<KParts::Part>(this);

    return terminalPart;
}

QTEST_KDEMAIN(PartManualTest , GUI)


/*
    SPDX-FileCopyrightText: 2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "PartManualTest.h"

// Qt
#include <QAction>
#include <QKeyEvent>
#include <QMenu>
#include <QMenuBar>
#include <qtestkeyboard.h>
// System
#include <sys/types.h>
#include <termios.h>

// KDE
#include <KMainWindow>
#include <KPluginFactory>
#include <qtest.h>
#include <kservice_version.h>

#if KSERVICE_VERSION < QT_VERSION_CHECK(5, 86, 0)
#include <KPluginLoader>
#include <KService>
#endif

// Konsole
#include "../Pty.h"
#include "../session/Session.h"
#include "keyboardtranslator/KeyboardTranslator.h"

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
    auto mainWindow = new KMainWindow();
    QMenu *fileMenu = mainWindow->menuBar()->addMenu(QStringLiteral("File"));
    QAction *testAction = fileMenu->addAction(QStringLiteral("Test"));
    testAction->setShortcut(QKeySequence(Konsole::ACCEL | Qt::Key_S));
    connect(testAction, &QAction::triggered, this, &Konsole::PartManualTest::shortcutTriggered);

    // Create terminal part and embed in into the main window
    KParts::Part *terminalPart = createPart();
    QVERIFY(terminalPart);
    mainWindow->setCentralWidget(terminalPart->widget());
    TerminalInterface *terminal = qobject_cast<TerminalInterface *>(terminalPart);
    QVERIFY(terminal);
    terminal->sendInput(QStringLiteral("Press Ctrl+S twice.\n"));
    mainWindow->show();

    // Test shortcut with override disabled, so the shortcut will be triggered
    _shortcutTriggered = false;
    _override = false;
    _overrideCalled = false;
    QVERIFY(connect(terminalPart, SIGNAL(overrideShortcut(QKeyEvent *, bool &)), this, SLOT(overrideShortcut(QKeyEvent *, bool &))));

    // QTest::keyClick(terminalPart->widget(),Qt::Key_S,Qt::ControlModifier);
    _shortcutEventLoop = new QEventLoop();
    _shortcutEventLoop->exec();

    QVERIFY(_overrideCalled);
    QVERIFY(_shortcutTriggered);
    QVERIFY(!_override);

    // Test shortcut with override enabled, so the shortcut will not be triggered
    _override = true;
    _overrideCalled = false;
    _shortcutTriggered = false;

    // QTest::keyClick(terminalPart->widget(),Qt::Key_S,Qt::ControlModifier);
    _shortcutEventLoop->exec();

    QVERIFY(_overrideCalled);
    QVERIFY(!_shortcutTriggered);
    QVERIFY(_override);

    delete _shortcutEventLoop;
    delete terminalPart;
    delete mainWindow;
}
void PartManualTest::overrideShortcut(QKeyEvent *event, bool &override)
{
    QVERIFY(override);
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

KParts::Part *PartManualTest::createPart()
{
#if KSERVICE_VERSION < QT_VERSION_CHECK(5, 86, 0)
    KService::Ptr service = KService::serviceByDesktopName(QStringLiteral("konsolepart"));
    Q_ASSERT(service);
    KPluginFactory *factory = KPluginLoader(service->library()).factory();
    Q_ASSERT(factory);

    auto *terminalPart = factory->create<KParts::Part>(this);

    return terminalPart;
#else
    const KPluginFactory::Result<KParts::Part> result = KPluginFactory::instantiatePlugin<KParts::Part>(KPluginMetaData(QStringLiteral("konsolepart")), this);
    Q_ASSERT(result);

    return result.plugin;
#endif
}

QTEST_MAIN(PartManualTest)

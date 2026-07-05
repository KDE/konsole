/*
    SPDX-FileCopyrightText: 2026 Zhuxuan Huang <raycppwizard@outlook.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "TerminalDisplayAccessibleTest.h"

#include <QtTest>
#include <QtTypes>
#include <QAccessible>
#include <QAccessibleTextInterface>
#include <QByteArray>
#include <QCoreApplication>
#include <QLatin1Char>
#include <QList>
#include <QString>
#include <QStringList>

#include <memory>

#include "../terminalDisplay/TerminalDisplay.h"
#include "../ScreenWindow.h"
#include "../Screen.h"
#include "../Emulation.h"
#include "../colorscheme/ColorScheme.h"
#include "../profile/Profile.h"
#include "../profile/ProfileManager.h"
#include "../session/Session.h"
#include "../session/SessionManager.h"
#include "../session/SessionController.h"

using namespace Konsole;

namespace {

// Strips trailing spaces added by the terminal window layout.
void stripTrailingSpaces(QString &line)
{
    while (line.endsWith(QLatin1Char(' '))) {
        line.chop(1);
    }
}

/**
 * A lightweight RAII fixture for testing TerminalDisplay.
 *
 * Each test method instantiates this on the stack. The destructor guarantees
 * that TerminalDisplay is cleanly detached and destroyed, avoiding state
 * sharing between subsequent accessibility interface tests.
 */
class TestTerminal
{
public:
    TestTerminal()
        : display_(nullptr)
    {
        // Apply a default color scheme wallpaper to prevent null pointer crashes
        // in TerminalDisplay::setScreenWindow when accessing _wallpaper->isAnimated().
        const ColorScheme scheme;
        display_.setWallpaper(scheme.wallpaper());

        // Initialize Session on the heap.
        session_ = std::make_unique<Session>(nullptr);

        // Disconnect the default imageSizeInitialized -> Session::run connection immediately.
        // This MUST be done right after Session instantiation and before any layout, addView,
        // or processEvents() runs. This prevents the queued signal from spawning a real
        // /bin/bash process during subsequent resize events.
        QObject::disconnect(session_->emulation(), &Emulation::imageSizeInitialized, session_.get(), &Session::run);

        // Ensure the session is mapped to a valid profile in SessionManager to prevent
        // null pointer dereferences when SessionController queries profile attributes.
        Profile::Ptr defaultProfile = ProfileManager::instance()->defaultProfile();
        if (defaultProfile == nullptr) {
            defaultProfile = Profile::Ptr(new Profile());
        }
        SessionManager::instance()->setSessionProfile(session_.get(), defaultProfile);

        // This internally creates a SessionController using the session's emulation
        // and binds it directly to our TerminalDisplay.
        controller_ = std::make_unique<SessionController>(session_.get(), &display_, nullptr);
        display_.setSessionController(controller_.get());

        // Explicitly register the display view to the session, which guarantees
        // the immediate and synchronous creation and binding of a valid ScreenWindow.
        session_->addView(&display_);

        // Prevent the widget from physically popping up on the screen during local testing.
        // Under Qt, this attribute keeps the widget's logical visibility (isVisible() == true)
        // intact while completely suppressing its physical on-screen window mapping.
        display_.setAttribute(Qt::WA_DontShowOnScreen);

        // Make the display logically visible.
        display_.setVisible(true);

        // Process pending Qt events (including QResizeEvents) to allow TerminalDisplay
        // to layout itself and propagate its size to the Session and Emulation.
        flushEvents();

        // Synchronize the physical screen size of the emulation to match the
        // final, laid-out viewport size of the ScreenWindow.
        if (const ScreenWindow *screenWindow = display_.screenWindow();
            screenWindow != nullptr) {
            const int lines = screenWindow->windowLines();
            const int columns = screenWindow->windowColumns();
            session_->emulation()->setImageSize(lines, columns);
        }

        // Set the codec to initialize the internal decoder.
        session_->emulation()->setCodec("UTF-8");
    }

    ~TestTerminal()
    {
        // Class Invariant Contract
        Q_ASSERT(session_ != nullptr);
        Q_ASSERT(controller_ != nullptr);

        // Detach screen window to prevent dangling pointer access during teardown.
        display_.setScreenWindow(nullptr);
    }

    TestTerminal(const TestTerminal&) = delete;
    TestTerminal& operator=(const TestTerminal&) = delete;

    TestTerminal(TestTerminal&&) = delete;
    TestTerminal& operator=(TestTerminal&&) = delete;

    static void flushEvents()
    {
        QCoreApplication::processEvents();
    }

    void writeAndFlush(const QString &text)
    {
        const QByteArray bytes = text.toUtf8();
        session_->emulation()->receiveData(bytes.constData(), static_cast<int>(bytes.size()));
        display_.updateImage();

        // Ensure image update and character metrics are fully processed
        flushEvents();
    }

    ScreenWindow *screenWindow() const
    {
        return display_.screenWindow().data();
    }

    QAccessibleTextInterface *textInterface()
    {
        QAccessibleInterface *accessibleInterface = QAccessible::queryAccessibleInterface(&display_);
        if (accessibleInterface == nullptr) {
            return nullptr;
        }
        return accessibleInterface->textInterface();
    }
private:
    TerminalDisplay display_;
    std::unique_ptr<Session> session_;
    std::unique_ptr<SessionController> controller_;
};

} // namespace

void TerminalDisplayAccessibleTest::initTestCase()
{
    QAccessible::setActive(true);
}

void TerminalDisplayAccessibleTest::cleanupTestCase()
{
    QAccessible::setActive(false);
}

void TerminalDisplayAccessibleTest::testFactoryAndBasicProperties()
{
    TerminalDisplay display(nullptr);

    const QAccessibleInterface *interface = QAccessible::queryAccessibleInterface(&display);
    QVERIFY(interface != nullptr);
    QCOMPARE(interface->role(), QAccessible::Terminal);

    // Regression test for commit dd6f3ca9 "Terminaldisplayaccessible: report multiline state"
    const QAccessible::State state = interface->state();
    QVERIFY(state.multiLine);
}

void TerminalDisplayAccessibleTest::testTextExtractionOutOfBounds()
{
    TestTerminal terminal;
    const QString expectedText = QStringLiteral("Line1\nLine2\nLine3");

    terminal.writeAndFlush(expectedText);

    const QAccessibleTextInterface *textInterface = terminal.textInterface();
    QVERIFY(textInterface != nullptr);

    const int maxIndex = textInterface->characterCount();
    QVERIFY(maxIndex > 0);

    // Regression test for commit 36ca1171 "Terminaldisplayaccessible: do not explode on invalid text offsets"
    //
    // Stale offsets exceeding the maximum buffer boundaries must safely clamp to the
    // valid boundary rather than triggering index assertions or out-of-bounds crashes.
    const int staleEndOffset = maxIndex + 100;
    const QString normalText = textInterface->text(0, maxIndex);
    const QString clampedText = textInterface->text(0, staleEndOffset);
    QCOMPARE(clampedText, normalText);

    // Offset intervals completely beyond boundary must return an empty string (allowing trailing newlines).
    const QString completelyOutOfBoundsText = textInterface->text(maxIndex + 10, staleEndOffset);
    QVERIFY(completelyOutOfBoundsText.trimmed().isEmpty());
}

void TerminalDisplayAccessibleTest::testBlockSelectionBypass()
{
    TestTerminal terminal;
    const QString mockInput = QStringLiteral("abcde\r\n12345\r\nABCDE");
    const QList expectedLines = {
        QStringLiteral("abcde"),
        QStringLiteral("12345"),
        QStringLiteral("ABCDE"),
    };

    terminal.writeAndFlush(mockInput);

    ScreenWindow *screenWindow = terminal.screenWindow();
    QVERIFY(screenWindow != nullptr);
    Screen *screen = screenWindow->screen();
    QVERIFY(screen != nullptr);

    // Enable block/column selection mode on the active data coordinates.
    screen->setSelectionStart(1, 0, true);
    screen->setSelectionEnd(2, 2, true);

    const QAccessibleTextInterface *textInterface = terminal.textInterface();
    QVERIFY(textInterface != nullptr);

    const int charCount = textInterface->characterCount();
    QVERIFY(charCount > 0);

    const QString rawA11yText = textInterface->text(0, charCount);

    // Regression test: Accessibility queries must bypass visual block selections.
    // Rather than returning a vertical column slice, the full lines must be intact.
    QStringList lines = rawA11yText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (QString &line : lines) {
        stripTrailingSpaces(line);
    }

    QCOMPARE(lines.size(), expectedLines.size());
    for (qsizetype i = 0; i < expectedLines.size(); ++i) {
        QCOMPARE(lines.at(i), expectedLines[i]);
    }
}

QTEST_MAIN(TerminalDisplayAccessibleTest)

#include "moc_TerminalDisplayAccessibleTest.cpp"

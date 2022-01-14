/*
    SPDX-FileCopyrightText: 2020 Lukasz Kotula <lukasz.kotula@gmx.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "ScreenTest.h"

// Qt
#include <QString>

// KDE
#include <qtest.h>

using namespace Konsole;

void ScreenTest::doLargeScreenCopyVerification(const QString &putToScreen, const QString &expectedSelection)
{
    Screen screen(largeScreenLines, largeScreenColumns);

    for (const auto &lineCharacter : putToScreen) {
        screen.displayCharacter(lineCharacter.toLatin1());
    }

    screen.setSelectionStart(0, 0, false);
    screen.setSelectionEnd(largeScreenColumns, 0);
    QCOMPARE(screen.selectedText(Screen::PlainText), expectedSelection);
}

void ScreenTest::testLargeScreenCopyShortLine()
{
    const QString putToScreen = QStringLiteral("0123456789abcde");
    const QString expectedSelection = QStringLiteral("0123456789abcde\n");
    doLargeScreenCopyVerification(putToScreen, expectedSelection);
}

void ScreenTest::testBlockSelection()
{
    Screen screen(largeScreenLines, largeScreenColumns);

    const QString reallyBigTextForReflow = QStringLiteral("abcd efgh ijkl mnop qrst uvxz ABCD EFGH IJKL MNOP QRST UVXZ");

    for (const QChar &c : reallyBigTextForReflow) {
        screen.displayCharacter(c.toLatin1());
    }

    // this breaks the lines in `abcd efgh `
    // reflowing everything to the lines below.
    screen.setReflowLines(true);

    // reflow does not reflows cursor line, so let's move it a bit down.
    screen.cursorDown(1);
    screen.resizeImage(largeScreenLines, 10);

    // True here means block selection.
    screen.setSelectionStart(0, 0, true);
    screen.setSelectionEnd(3, 1);

    // after the resize, the string should be:
    // abcd efgh
    // ijkl mnop
    // ...
    // I'm selecting the first two lines of the first column of strings,
    // so, abcd ijkl.
    const QString selectedText = screen.selectedText(Screen::PlainText);
    QCOMPARE(screen.selectedText(Screen::PlainText), QStringLiteral("abcd ijkl"));
}

void ScreenTest::testLargeScreenCopyEmptyLine()
{
    const QString putToScreen;
    const QString expectedSelection = QStringLiteral("\n");

    doLargeScreenCopyVerification(putToScreen, expectedSelection);
}

void ScreenTest::testLargeScreenCopyLongLine()
{
    QString putToScreen;

    // Make the line longer than screen size (1300 characters)
    for (int i = 0; i < 130; ++i) {
        putToScreen.append(QStringLiteral("0123456789"));
    }
    const QString expectedSelection = putToScreen.left(1200);

    doLargeScreenCopyVerification(putToScreen, expectedSelection);
}

void ScreenTest::doComparePosition(Screen *screen, int y, int x)
{
    QCOMPARE(screen->getCursorY(), y);
    QCOMPARE(screen->getCursorX(), x);
}

// Test: setCursorYX, setCursorX, setCursorY, cursorDown, cursorUp,
// cursorRight, cursorLeft, cursorNextLine and cursorPreviousLine
void ScreenTest::testCursorPosition()
{
    Screen *screen = new Screen(largeScreenLines, largeScreenColumns);

    // setCursorYX will test setCursorX and setCursorY too
    screen->setCursorYX(6, 6);
    doComparePosition(screen, 5, 5);

    screen->setCursorYX(2147483647, 2147483647);
    doComparePosition(screen, largeScreenLines - 1, largeScreenColumns - 1);

    screen->setCursorYX(-1, -1);
    doComparePosition(screen, 0, 0);

    screen->setCursorYX(0, 0);
    doComparePosition(screen, 0, 0);

    screen->setCursorYX(1, 1);
    doComparePosition(screen, 0, 0);

    screen->cursorDown(2147483647);
    doComparePosition(screen, largeScreenLines - 1, 0);

    screen->cursorUp(2147483647);
    doComparePosition(screen, 0, 0);

    screen->cursorDown(4);
    doComparePosition(screen, 4, 0);

    screen->cursorDown(-1);
    doComparePosition(screen, 5, 0);

    screen->cursorDown(0);
    doComparePosition(screen, 6, 0);

    screen->cursorUp(0);
    doComparePosition(screen, 5, 0);

    screen->cursorUp(-1);
    doComparePosition(screen, 4, 0);

    screen->cursorUp(4);
    doComparePosition(screen, 0, 0);

    screen->cursorRight(-1);
    doComparePosition(screen, 0, 1);

    screen->cursorRight(3);
    doComparePosition(screen, 0, 4);

    screen->cursorRight(0);
    doComparePosition(screen, 0, 5);

    screen->cursorLeft(0);
    doComparePosition(screen, 0, 4);

    screen->cursorLeft(2);
    doComparePosition(screen, 0, 2);

    screen->cursorLeft(-1);
    doComparePosition(screen, 0, 1);

    screen->cursorRight(2147483647);
    doComparePosition(screen, 0, largeScreenColumns - 1);

    screen->cursorLeft(2147483647);
    doComparePosition(screen, 0, 0);

    screen->cursorNextLine(4);
    doComparePosition(screen, 4, 0);

    screen->cursorNextLine(-1);
    doComparePosition(screen, 5, 0);

    screen->cursorNextLine(0);
    doComparePosition(screen, 6, 0);

    screen->cursorPreviousLine(0);
    doComparePosition(screen, 5, 0);

    screen->cursorPreviousLine(2);
    doComparePosition(screen, 3, 0);

    screen->cursorPreviousLine(-1);
    doComparePosition(screen, 2, 0);

    screen->cursorPreviousLine(2147483647);
    doComparePosition(screen, 0, 0);

    screen->cursorNextLine(2147483647);
    doComparePosition(screen, largeScreenLines - 1, 0);

    delete screen;
}

QTEST_GUILESS_MAIN(ScreenTest)

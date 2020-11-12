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

  for(const auto &lineCharacter : putToScreen) {
    screen.displayCharacter(lineCharacter.toLatin1());
  }

  screen.setSelectionStart(0,0, false);
  screen.setSelectionEnd(largeScreenColumns,0);
  QCOMPARE(screen.selectedText(Screen::PlainText), expectedSelection);
}

void ScreenTest::testLargeScreenCopyShortLine()
{
  const QString putToScreen = QStringLiteral("0123456789abcde");
  const QString expectedSelection = QStringLiteral("0123456789abcde\n");
  doLargeScreenCopyVerification(putToScreen, expectedSelection);
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
  for(int i = 0; i <130; ++i) {
    putToScreen.append(QStringLiteral("0123456789"));
  }
  const QString expectedSelection = putToScreen.left(1200);

  doLargeScreenCopyVerification(putToScreen, expectedSelection);
}

QTEST_GUILESS_MAIN(ScreenTest)

/*
    Copyright 2020 by Lukasz Kotula <lukasz.kotula@gmx.com>

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

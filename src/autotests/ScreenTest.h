/*
    SPDX-FileCopyrightText: 2020 Lukasz Kotula <lukasz.kotula@gmx.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SCREENTEST_H
#define SCREENTEST_H

#include <QObject>

#include "../Screen.h"

namespace Konsole
{
class ScreenTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testLargeScreenCopyShortLine();
    void testLargeScreenCopyEmptyLine();
    void testLargeScreenCopyLongLine();
    void testBlockSelection();
    void testCursorPosition();

private:
    void doLargeScreenCopyVerification(const QString &putToScreen, const QString &expectedSelection);
    void doComparePosition(Screen *screen, int y, int x);

    const int largeScreenLines = 10;
    const int largeScreenColumns = 1200;
};

}

#endif // SCREENTEST_H

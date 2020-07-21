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

private:
    void doLargeScreenCopyVerification(const QString &putToScreen, const QString &expectedSelection);

    const int largeScreenLines = 10;
    const int largeScreenColumns = 1200;
};

}

#endif // SCREENTEST_H


/*
    SPDX-FileCopyrightText: 2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PROFILETEST_H
#define PROFILETEST_H

#include <QObject>

namespace Konsole
{
class ProfileTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void testProfile();
    void testClone();
    void testProfileGroup();
    void testProfileFileNames();
    void testProfileNameSorting();
    void testFallbackProfile();
};

}

#endif // PROFILETEST_H

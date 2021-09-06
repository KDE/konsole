/*
    SPDX-FileCopyrightText: 2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CHARACTERCOLORTEST_H
#define CHARACTERCOLORTEST_H

#include <QObject>

#include "../characters/CharacterColor.h"

namespace Konsole
{
class CharacterColorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();

    void testColorEntry();

    void testDummyConstructor();
    void testColorSpaceDefault_data();
    void testColorSpaceDefault();
    void testColorSpaceSystem_data();
    void testColorSpaceSystem();
    void testColorSpaceRGB_data();
    void testColorSpaceRGB();
    void testColor256_data();
    void testColor256();

private:
    static const QColor DefaultColorTable[];
};

}

#endif // CHARACTERCOLORTEST_H

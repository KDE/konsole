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
    static const ColorEntry DefaultColorTable[];
};

}

#endif // CHARACTERCOLORTEST_H


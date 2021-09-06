/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef RANDOMIZATIONRANGE_H
#define RANDOMIZATIONRANGE_H

namespace Konsole
{
// specifies how much a particular color can be randomized by
class RandomizationRange
{
public:
    RandomizationRange();
    bool isNull() const;

    double hue;
    double saturation;
    double lightness;
};

}

#endif

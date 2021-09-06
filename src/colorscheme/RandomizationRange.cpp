/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "RandomizationRange.h"

// Qt
#include <QtGlobal>

namespace Konsole
{
RandomizationRange::RandomizationRange()
    : hue(0.0)
    , saturation(0.0)
    , lightness(0.0)
{
}

bool RandomizationRange::isNull() const
{
    return qFuzzyIsNull(hue) && qFuzzyIsNull(saturation) && qFuzzyIsNull(lightness);
}

}

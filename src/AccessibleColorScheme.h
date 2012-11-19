/*
    This source file is part of Konsole, a terminal emulator.

    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>

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

#ifndef ACCESSIBLECOLORSCHEME_H
#define ACCESSIBLECOLORSCHEME_H

// Konsole
#include "ColorScheme.h"

namespace Konsole
{

/**
 * A color scheme which uses colors from the standard KDE color palette.
 *
 * This is designed primarily for the benefit of users who are using specially
 * designed colors.
 *
 * TODO Implement and make it the default on systems with specialized KDE
 * color schemes.
 */
class AccessibleColorScheme : public ColorScheme
{
public:
    AccessibleColorScheme();
};
}

#endif //ACCESSIBLECOLORSCHEME_H

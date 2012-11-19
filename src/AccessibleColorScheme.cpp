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

// Own
#include "AccessibleColorScheme.h"

// Qt
#include <QtGui/QBrush>

// KDE
#include <KColorScheme>
#include <KLocalizedString>

using namespace Konsole;

// Work In Progress - A color scheme for use on KDE setups for users
// with visual disabilities which means that they may have trouble
// reading text with the supplied color schemes.
//
// This color scheme uses only the 'safe' colors defined by the
// KColorScheme class.
//
// A complication this introduces is that each color provided by
// KColorScheme is defined as a 'background' or 'foreground' color.
// Only foreground colors are allowed to be used to render text and
// only background colors are allowed to be used for backgrounds.
//
// The ColorEntry and TerminalDisplay classes do not currently
// support this restriction.
//
// Requirements:
//  - A color scheme which uses only colors from the KColorScheme class
//  - Ability to restrict which colors the TerminalDisplay widget
//    uses as foreground and background color
//  - Make use of KGlobalSettings::allowDefaultBackgroundImages() as
//    a hint to determine whether this accessible color scheme should
//    be used by default.
//
//
// -- Robert Knight <robertknight@gmail.com> 21/07/2007
//
AccessibleColorScheme::AccessibleColorScheme()
    : ColorScheme()
{
    // basic attributes
    setName("accessible");
    setDescription(i18n("Accessible Color Scheme"));

    // setup colors
    const int ColorRoleCount = 8;

    const KColorScheme colorScheme(QPalette::Active);

    QBrush colors[ColorRoleCount] = {
        colorScheme.foreground(colorScheme.NormalText),
        colorScheme.background(colorScheme.NormalBackground),

        colorScheme.foreground(colorScheme.InactiveText),
        colorScheme.foreground(colorScheme.ActiveText),
        colorScheme.foreground(colorScheme.LinkText),
        colorScheme.foreground(colorScheme.VisitedText),
        colorScheme.foreground(colorScheme.NegativeText),
        colorScheme.foreground(colorScheme.NeutralText)
    };

    for (int i = 0 ; i < TABLE_COLORS ; i++) {
        ColorEntry entry;
        entry.color = colors[ i % ColorRoleCount ].color();

        setColorTableEntry(i , entry);
    }
}


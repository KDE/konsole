/*
    This file is part of Konsole, an X terminal.
    Copyright 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

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

#include "CharacterFormat.h"

using namespace Konsole;

bool CharacterFormat::equalsFormat(const CharacterFormat &other) const
{
    return (other.rendition & ~RE_EXTENDED_CHAR) == (rendition & ~RE_EXTENDED_CHAR)
        && other.fgColor == fgColor && other.bgColor == bgColor;
}

bool CharacterFormat::equalsFormat(const Character &c) const
{
    return (c.rendition & ~RE_EXTENDED_CHAR) == (rendition & ~RE_EXTENDED_CHAR)
        && c.foregroundColor == fgColor && c.backgroundColor == bgColor;
}

void CharacterFormat::setFormat(const Character &c)
{
    rendition = c.rendition;
    fgColor = c.foregroundColor;
    bgColor = c.backgroundColor;
    isRealCharacter = c.isRealCharacter;
}

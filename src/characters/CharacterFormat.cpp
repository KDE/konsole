/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "CharacterFormat.h"

using namespace Konsole;

bool CharacterFormat::equalsFormat(const CharacterFormat &other) const
{
    return (other.rendition & ~RE_EXTENDED_CHAR) == (rendition & ~RE_EXTENDED_CHAR) && other.fgColor == fgColor && other.bgColor == bgColor;
}

bool CharacterFormat::equalsFormat(const Character &c) const
{
    return (c.rendition & ~RE_EXTENDED_CHAR) == (rendition & ~RE_EXTENDED_CHAR) && c.foregroundColor == fgColor && c.backgroundColor == bgColor;
}

void CharacterFormat::setFormat(const Character &c)
{
    rendition = c.rendition;
    fgColor = c.foregroundColor;
    bgColor = c.backgroundColor;
    isRealCharacter = c.isRealCharacter;
}

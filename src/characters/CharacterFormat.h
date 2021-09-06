/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CHARACTERFORMAT_H
#define CHARACTERFORMAT_H

#include "Character.h"

namespace Konsole
{
class CharacterFormat
{
public:
    bool equalsFormat(const CharacterFormat &other) const;
    bool equalsFormat(const Character &c) const;
    void setFormat(const Character &c);

    CharacterColor fgColor, bgColor;
    quint16 startPos;
    RenditionFlags rendition;
    bool isRealCharacter;
};

}

#endif

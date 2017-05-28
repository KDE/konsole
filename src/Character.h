/*
    This file is part of Konsole, KDE's terminal.

    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>
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

#ifndef CHARACTER_H
#define CHARACTER_H

// Konsole
#include "CharacterColor.h"

namespace Konsole {
typedef unsigned char LineProperty;

typedef quint16 RenditionFlags;

const int LINE_DEFAULT      = 0;
const int LINE_WRAPPED      = (1 << 0);
const int LINE_DOUBLEWIDTH  = (1 << 1);
const int LINE_DOUBLEHEIGHT = (1 << 2);

const RenditionFlags DEFAULT_RENDITION = 0;
const RenditionFlags RE_BOLD           = (1 << 0);
const RenditionFlags RE_BLINK          = (1 << 1);
const RenditionFlags RE_UNDERLINE      = (1 << 2);
const RenditionFlags RE_REVERSE        = (1 << 3); // Screen only
const RenditionFlags RE_ITALIC         = (1 << 4);
const RenditionFlags RE_CURSOR         = (1 << 5);
const RenditionFlags RE_EXTENDED_CHAR  = (1 << 6);
const RenditionFlags RE_FAINT          = (1 << 7);
const RenditionFlags RE_STRIKEOUT      = (1 << 8);
const RenditionFlags RE_CONCEAL        = (1 << 9);
const RenditionFlags RE_OVERLINE       = (1 << 10);

/**
 * Unicode character in the range of U+2500 ~ U+257F are known as line
 * characters, or box-drawing characters. Currently, konsole draws those
 * characters itself, instead of using the glyph provided by the font.
 * Unfortunately, the triple and quadruple dash lines (┄┅┆┇┈┉┊┋) are too
 * detailed too be drawn cleanly at normal font scales without anti
 * -aliasing, so those are drawn as regular characters.
 */
inline bool isSupportedLineChar(quint16 codePoint)
{
    return (codePoint & 0xFF80) == 0x2500 // Unicode block: Mathematical Symbols - Box Drawing
           && !(0x2504 <= codePoint && codePoint <= 0x250B); // Triple and quadruple dash range
}

/**
 * A single character in the terminal which consists of a unicode character
 * value, foreground and background colors and a set of rendition attributes
 * which specify how it should be drawn.
 */
class Character
{
public:
    /**
     * Constructs a new character.
     *
     * @param _c The unicode character value of this character.
     * @param _f The foreground color used to draw the character.
     * @param _b The color used to draw the character's background.
     * @param _r A set of rendition flags which specify how this character
     *           is to be drawn.
     * @param _real Indicate whether this character really exists, or exists
     *              simply as place holder.
     */
    explicit inline Character(quint16 _c = ' ',
                              CharacterColor  _f = CharacterColor(COLOR_SPACE_DEFAULT, DEFAULT_FORE_COLOR),
                              CharacterColor  _b = CharacterColor(COLOR_SPACE_DEFAULT, DEFAULT_BACK_COLOR),
                              RenditionFlags  _r = DEFAULT_RENDITION,
                              bool _real = true)
        : character(_c)
        , rendition(_r)
        , foregroundColor(_f)
        , backgroundColor(_b)
        , isRealCharacter(_real) { }

    /** The unicode character value for this character.
     *
     * if RE_EXTENDED_CHAR is set, character is a hash code which can be used to
     * look up the unicode character sequence in the ExtendedCharTable used to
     * create the sequence.
     */
    quint16 character;

    /** A combination of RENDITION flags which specify options for drawing the character. */
    RenditionFlags rendition;

    /** The foreground color used to draw this character. */
    CharacterColor foregroundColor;

    /** The color used to draw this character's background. */
    CharacterColor backgroundColor;

    /** Indicate whether this character really exists, or exists simply as place holder.
     *
     *  TODO: this boolean filed can be further improved to become a enum filed, which
     *  indicates different roles:
     *
     *    RealCharacter: a character which really exists
     *    PlaceHolderCharacter: a character which exists as place holder
     *    TabStopCharacter: a special place holder for HT("\t")
     */
    bool isRealCharacter;

    /**
     * returns true if the format (color, rendition flag) of the compared characters is equal
     */
    bool equalsFormat(const Character &other) const;

    /**
     * Compares two characters and returns true if they have the same unicode character value,
     * rendition and colors.
     */
    friend bool operator ==(const Character &a, const Character &b);

    /**
     * Compares two characters and returns true if they have different unicode character values,
     * renditions or colors.
     */
    friend bool operator !=(const Character &a, const Character &b);

    inline bool isLineChar() const
    {
        if (rendition & RE_EXTENDED_CHAR) {
            return false;
        } else {
            return isSupportedLineChar(character);
        }
    }

    inline bool isSpace() const
    {
        if (rendition & RE_EXTENDED_CHAR) {
            return false;
        } else {
            return QChar(character).isSpace();
        }
    }
};

inline bool operator ==(const Character &a, const Character &b)
{
    return a.character == b.character && a.equalsFormat(b);
}

inline bool operator !=(const Character &a, const Character &b)
{
    return !operator==(a, b);
}

inline bool Character::equalsFormat(const Character &other) const
{
    return backgroundColor == other.backgroundColor
           && foregroundColor == other.foregroundColor
           && rendition == other.rendition;
}
}
Q_DECLARE_TYPEINFO(Konsole::Character, Q_MOVABLE_TYPE);

#endif // CHARACTER_H

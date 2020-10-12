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

#ifndef CHARACTERCOLOR_H
#define CHARACTERCOLOR_H

// Qt
#include <QColor>

namespace Konsole {
/**
 * An entry in a terminal display's color palette.
 *
 * A color palette is an array of 16 ColorEntry instances which map
 * system color indexes (from 0 to 15) into actual colors.
 */
typedef QColor ColorEntry;

// Attributed Character Representations ///////////////////////////////

// Colors

#define BASE_COLORS   (2+8)
#define INTENSITIES   3
#define TABLE_COLORS  (INTENSITIES*BASE_COLORS)

enum ColorTableIndex {
    ColorFgIndex,
    ColorBgIndex,
    Color0Index,
    Color1Index,
    Color2Index,
    Color3Index,
    Color4Index,
    Color5Index,
    Color6Index,
    Color7Index,

    ColorFgIntenseIndex,
    ColorBgIntenseIndex,
    Color0IntenseIndex,
    Color1IntenseIndex,
    Color2IntenseIndex,
    Color3IntenseIndex,
    Color4IntenseIndex,
    Color5IntenseIndex,
    Color6IntenseIndex,
    Color7IntenseIndex,

    ColorFgFaintIndex,
    ColorBgFaintIndex,
    Color0FaintIndex,
    Color1FaintIndex,
    Color2FaintIndex,
    Color3FaintIndex,
    Color4FaintIndex,
    Color5FaintIndex,
    Color6FaintIndex,
    Color7FaintIndex,
};

#define DEFAULT_FORE_COLOR 0
#define DEFAULT_BACK_COLOR 1

/* CharacterColor is a union of the various color spaces.

   Assignment is as follows:

   Type  - Space        - Values

   0     - Undefined   - u:  0,      v:0        w:0
   1     - Default     - u:  0..1    v:intense  w:0
   2     - System      - u:  0..7    v:intense  w:0
   3     - Index(256)  - u: 16..255  v:0        w:0
   4     - RGB         - u:  0..255  v:0..256   w:0..256

   ``intense'' is either 0 (normal), 1 (intensive), or 2 (faint)

   Default color space has two separate colors, namely
   default foreground and default background color.
*/

#define COLOR_SPACE_UNDEFINED   0
#define COLOR_SPACE_DEFAULT     1
#define COLOR_SPACE_SYSTEM      2
#define COLOR_SPACE_256         3
#define COLOR_SPACE_RGB         4

/**
 * Describes the color of a single character in the terminal.
 */
class CharacterColor
{
    friend class Character;

public:
    /** Constructs a new CharacterColor whose color and color space are undefined. */
    CharacterColor() :
        _colorSpace(COLOR_SPACE_UNDEFINED),
        _u(0),
        _v(0),
        _w(0)
    {
    }

    /**
     * Constructs a new CharacterColor using the specified @p colorSpace and with
     * color value @p co
     *
     * The meaning of @p co depends on the @p colorSpace used.
     *
     * TODO : Document how @p co relates to @p colorSpace
     *
     * TODO : Add documentation about available color spaces.
     */
    CharacterColor(quint8 colorSpace, int co) :
        _colorSpace(colorSpace),
        _u(0),
        _v(0),
        _w(0)
    {
        switch (colorSpace) {
        case COLOR_SPACE_DEFAULT:
            _u = co & 1;
            break;
        case COLOR_SPACE_SYSTEM:
            _u = co & 7;
            _v = (co >> 3) & 3;
            break;
        case COLOR_SPACE_256:
            _u = co & 255;
            break;
        case COLOR_SPACE_RGB:
            _u = co >> 16;
            _v = co >> 8;
            _w = co;
            break;
        default:
            _colorSpace = COLOR_SPACE_UNDEFINED;
        }
    }

    quint8 colorSpace() const { return _colorSpace; }
    void termColor(int *u, int *v, int *w) { *u = _u; *v = _v; *w = _w; }

    /**
     * Returns true if this character color entry is valid.
     */
    bool isValid() const
    {
        return _colorSpace != COLOR_SPACE_UNDEFINED;
    }

    /**
     * Set this color as an intensive system color.
     *
     * This is only applicable if the color is using the COLOR_SPACE_DEFAULT or COLOR_SPACE_SYSTEM
     * color spaces.
     */
    void setIntensive();

    /**
     * Set this color as a faint system color.
     *
     * This is only applicable if the color is using the COLOR_SPACE_DEFAULT or COLOR_SPACE_SYSTEM
     * color spaces.
     */
    void setFaint();

    /**
     * Returns the color within the specified color @p base
     *
     * The @p base is only used if this color is one of the 16 system colors, otherwise
     * it is ignored.
     */
    QColor color(const ColorEntry *base) const;

    /**
     * Compares two colors and returns true if they represent the same color value and
     * use the same color space.
     */
    friend bool operator ==(const CharacterColor &a, const CharacterColor &b);
    /**
     * Compares two colors and returns true if they represent different color values
     * or use different color spaces.
     */
    friend bool operator !=(const CharacterColor &a, const CharacterColor &b);

private:
    quint8 _colorSpace;

    // bytes storing the character color
    quint8 _u;
    quint8 _v;
    quint8 _w;
};

inline bool operator ==(const CharacterColor &a, const CharacterColor &b)
{
    return a._colorSpace == b._colorSpace
           && a._u == b._u
           && a._v == b._v
           && a._w == b._w;
}

inline bool operator !=(const CharacterColor &a, const CharacterColor &b)
{
    return !operator==(a, b);
}

inline const QColor color256(quint8 u, const ColorEntry *base)
{
    //   0.. 16: system colors
    if (u < 8) {
        return base[u + 2];
    }
    u -= 8;
    if (u < 8) {
        return base[u + 2 + BASE_COLORS];
    }
    u -= 8;

    //  16..231: 6x6x6 rgb color cube
    if (u < 216) {
        return QColor(((u / 36) % 6) ? (40 * ((u / 36) % 6) + 55) : 0,
                      ((u / 6) % 6) ? (40 * ((u / 6) % 6) + 55) : 0,
                      ((u / 1) % 6) ? (40 * ((u / 1) % 6) + 55) : 0);
    }
    u -= 216;

    // 232..255: gray, leaving out black and white
    int gray = u * 10 + 8;

    return QColor(gray, gray, gray);
}

inline QColor CharacterColor::color(const ColorEntry *base) const
{
    switch (_colorSpace) {
    case COLOR_SPACE_DEFAULT:
        return base[_u + 0 + (_v * BASE_COLORS)];
    case COLOR_SPACE_SYSTEM:
        return base[_u + 2 + (_v * BASE_COLORS)];
    case COLOR_SPACE_256:
        return color256(_u, base);
    case COLOR_SPACE_RGB:
        return QColor(_u, _v, _w);
    case COLOR_SPACE_UNDEFINED:
        return QColor();
    }

    Q_ASSERT(false); // invalid color space

    return QColor();
}

inline void CharacterColor::setIntensive()
{
    if (_colorSpace == COLOR_SPACE_SYSTEM || _colorSpace == COLOR_SPACE_DEFAULT) {
        _v = 1;
    }
}

inline void CharacterColor::setFaint()
{
    if (_colorSpace == COLOR_SPACE_SYSTEM || _colorSpace == COLOR_SPACE_DEFAULT) {
        _v = 2;
    }
}
}

#endif // CHARACTERCOLOR_H

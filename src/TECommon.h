/*
    This file is part of Konsole, an X terminal.
    Copyright (C) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

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

/*! \file TECommon.h
    \brief Definitions shared between Screen and TerminalDisplay.
*/

#ifndef TECOMMON_H
#define TECOMMON_H

// Qt
#include <QColor>
#include <QHash>

namespace Konsole
{

#ifndef UINT8
typedef unsigned char UINT8;
#endif

#ifndef UINT16
typedef unsigned short UINT16;
#endif

// Color Table Elements ///////////////////////////////////////////////

/*!
*/
struct ColorEntry
{
  ColorEntry(QColor c, bool tr, bool b) : color(c), transparent(tr), bold(b) {}
  ColorEntry() : transparent(false), bold(false) {} // default constructors
  void operator=(const ColorEntry& rhs) { 
       color = rhs.color; 
       transparent = rhs.transparent; 
       bold = rhs.bold; 
  }
  QColor color;
  bool   transparent; // if used on bg
  bool   bold;        // if used on fg
};


// Attributed Character Representations ///////////////////////////////

// Colors

#define BASE_COLORS   (2+8)
#define INTENSITIES   2
#define TABLE_COLORS  (INTENSITIES*BASE_COLORS)

#define DEFAULT_FORE_COLOR 0
#define DEFAULT_BACK_COLOR 1

//a standard set of colors using black text on a white background.
//defined in TerminalDisplay.cpp

static const ColorEntry base_color_table[TABLE_COLORS] =
// The following are almost IBM standard color codes, with some slight
// gamma correction for the dim colors to compensate for bright X screens.
// It contains the 8 ansiterm/xterm colors in 2 intensities.
{
  // Fixme: could add faint colors here, also.
  // normal
  ColorEntry(QColor(0x00,0x00,0x00), 0, 0 ), ColorEntry( QColor(0xB2,0xB2,0xB2), 1, 0 ), // Dfore, Dback
  ColorEntry(QColor(0x00,0x00,0x00), 0, 0 ), ColorEntry( QColor(0xB2,0x18,0x18), 0, 0 ), // Black, Red
  ColorEntry(QColor(0x18,0xB2,0x18), 0, 0 ), ColorEntry( QColor(0xB2,0x68,0x18), 0, 0 ), // Green, Yellow
  ColorEntry(QColor(0x18,0x18,0xB2), 0, 0 ), ColorEntry( QColor(0xB2,0x18,0xB2), 0, 0 ), // Blue, Magenta
  ColorEntry(QColor(0x18,0xB2,0xB2), 0, 0 ), ColorEntry( QColor(0xB2,0xB2,0xB2), 0, 0 ), // Cyan, White
  // intensiv
  ColorEntry(QColor(0x00,0x00,0x00), 0, 1 ), ColorEntry( QColor(0xFF,0xFF,0xFF), 1, 0 ),
  ColorEntry(QColor(0x68,0x68,0x68), 0, 0 ), ColorEntry( QColor(0xFF,0x54,0x54), 0, 0 ),
  ColorEntry(QColor(0x54,0xFF,0x54), 0, 0 ), ColorEntry( QColor(0xFF,0xFF,0x54), 0, 0 ),
  ColorEntry(QColor(0x54,0x54,0xFF), 0, 0 ), ColorEntry( QColor(0xFF,0x54,0xFF), 0, 0 ),
  ColorEntry(QColor(0x54,0xFF,0xFF), 0, 0 ), ColorEntry( QColor(0xFF,0xFF,0xFF), 0, 0 )
};

#define DEFAULT_RENDITION  0
#define RE_BOLD            (1 << 0)
#define RE_BLINK           (1 << 1)
#define RE_UNDERLINE       (1 << 2)
#define RE_REVERSE         (1 << 3) // Screen only
#define RE_INTENSIVE       (1 << 3) // Widget only
#define RE_CURSOR          (1 << 4)
#define RE_EXTENDED_CHAR   (1 << 5)

/**
 * A table which stores sequences of unicode characters, referenced
 * by hash keys.  The hash key itself is the same size as a unicode
 * character ( ushort ) so that it can occupy the same space in
 * a structure.
 */
class ExtendedCharTable
{
public:
    /** Constructs a new character table. */
    ExtendedCharTable();
    ~ExtendedCharTable();

    /**
     * Adds a sequences of unicode characters to the table and returns
     * a hash code which can be used later to look up the sequence
     * using lookupExtendedChar()
     *
     * If the same sequence already exists in the table, the hash
     * of the existing sequence will be returned.
     *
     * @param unicodePoints An array of unicode character points
     * @param length Length of @p unicodePoints
     */
    ushort createExtendedChar(ushort* unicodePoints , ushort length);
    /**
     * Looks up and returns a pointer to a sequence of unicode characters
     * which was added to the table using createExtendedChar().
     *
     * @param hash The hash key returned by createExtendedChar()
     * @param length This variable is set to the length of the 
     * character sequence.
     *
     * @return A unicode character sequence of size @p length.
     */
    ushort* lookupExtendedChar(ushort hash , ushort& length) const;

    /** The global ExtendedCharTable instance. */
    static ExtendedCharTable instance;
private:
    // calculates the hash key of a sequence of unicode points of size 'length'
    ushort extendedCharHash(ushort* unicodePoints , ushort length) const;
    // tests whether the entry in the table specified by 'hash' matches the 
    // character sequence 'unicodePoints' of size 'length'
    bool extendedCharMatch(ushort hash , ushort* unicodePoints , ushort length) const;
    // internal, maps hash keys to character sequence buffers.  The first ushort
    // in each value is the length of the buffer, followed by the ushorts in the buffer
    // themselves.
    QHash<ushort,ushort*> extendedCharTable;
};

typedef unsigned char LineProperty;

static const int LINE_DEFAULT		= 0;
static const int LINE_WRAPPED 	 	= (1 << 0);
static const int LINE_DOUBLEWIDTH  	= (1 << 1);
static const int LINE_DOUBLEHEIGHT	= (1 << 2);

/* CharacterColor is a union of the various color spaces.

   Assignment is as follows:

   Type  - Space        - Values

   0     - Undefined   - u:  0,      v:0        w:0
   1     - Default     - u:  0..1    v:intense  w:0
   2     - System      - u:  0..7    v:intense  w:0
   3     - Index(256)  - u: 16..255  v:0        w:0
   4     - RGB         - u:  0..255  v:0..256   w:0..256

   Default colour space has two separate colours, namely
   default foreground and default background colour.
*/

#define COLOR_SPACE_UNDEFINED   0
#define COLOR_SPACE_DEFAULT     1
#define COLOR_SPACE_SYSTEM      2
#define COLOR_SPACE_256         3
#define COLOR_SPACE_RGB         4

class CharacterColor
{
public:
  CharacterColor() : t(0), u(0), v(0), w(0) {}
  CharacterColor(UINT8 ty, int co) : t(ty), u(0), v(0), w(0)
  {
    if (COLOR_SPACE_DEFAULT == t) {
      u = co & 1;
    } else if (COLOR_SPACE_SYSTEM == t) {
      u = co & 7;
      v = (co >> 3) & 1;
    } else if (COLOR_SPACE_256 == t) {
      u = co & 255;
    } else if (COLOR_SPACE_RGB == t) {
      u = co >> 16;
      v = co >> 8;
      w = co;
    }
#if 0
    // Doesn't work with gcc 3.3.4
    switch (t)
    {
      case COLOR_SPACE_UNDEFINED:                                break;
      case COLOR_SPACE_DEFAULT: u = co&  1;                    break;
      case COLOR_SPACE_SYSTEM: u = co&  7; v = (co>>3)&1;     break;
      case COLOR_SPACE_256: u = co&255;                    break;
      case COLOR_SPACE_RGB: u = co>>16; v = co>>8; w = co; break;
      default    : t = 0;                         break;
    }
#endif
  }
  UINT8 t; // color space indicator
  UINT8 u; // various bytes representing the data in the respective ...
  UINT8 v; // ... color space. C++ does not do unions, so we cannot ...
  UINT8 w; // ... express ourselfs here, properly.
  void toggleIntensive(); // Hack or helper?
  QColor color(const ColorEntry* base) const;
  friend bool operator == (const CharacterColor& a, const CharacterColor& b);
  friend bool operator != (const CharacterColor& a, const CharacterColor& b);
};

inline bool operator == (const CharacterColor& a, const CharacterColor& b)
{ 
  return *reinterpret_cast<const quint32*>(&a.t) == *reinterpret_cast<const quint32*>(&b.t);
}

inline bool operator != (const CharacterColor& a, const CharacterColor& b)
{
  return *reinterpret_cast<const quint32*>(&a.t) != *reinterpret_cast<const quint32*>(&b.t);
}

inline const QColor color256(UINT8 u, const ColorEntry* base)
{
  //   0.. 16: system colors
  if (u <   8) return base[u+2            ].color; u -= 8;
  if (u <   8) return base[u+2+BASE_COLORS].color; u -= 8;

  //  16..231: 6x6x6 rgb color cube
  if (u < 216) return QColor(255*((u/36)%6)/5,
                             255*((u/ 6)%6)/5,
                             255*((u/ 1)%6)/5); u -= 216;
  
  // 232..255: gray, leaving out black and white
  int gray = u*10+8; return QColor(gray,gray,gray);
}

inline QColor CharacterColor::color(const ColorEntry* base) const
{
  switch (t)
  {
    case COLOR_SPACE_DEFAULT: return base[u+0+(v?BASE_COLORS:0)].color;
    case COLOR_SPACE_SYSTEM: return base[u+2+(v?BASE_COLORS:0)].color;
    case COLOR_SPACE_256: return color256(u,base);
    case COLOR_SPACE_RGB: return QColor(u,v,w);
    default    : return QColor(255,0,0); // diagnostic catch all
  }
}

inline void CharacterColor::toggleIntensive()
{
  if (t == COLOR_SPACE_SYSTEM || t == COLOR_SPACE_DEFAULT)
  {
    v = !v;
  }
}

/*! \class Character
 *  \brief a character with rendition attributes.
*/

class Character
{
public:
  inline Character(UINT16 _c = ' ',
            CharacterColor  _f = CharacterColor(COLOR_SPACE_DEFAULT,DEFAULT_FORE_COLOR),
            CharacterColor  _b = CharacterColor(COLOR_SPACE_DEFAULT,DEFAULT_BACK_COLOR),
            UINT8  _r = DEFAULT_RENDITION)
       : character(_c), rendition(_r), foregroundColor(_f), backgroundColor(_b) {}
public:

  union
  {
    UINT16 character; // a single unicode character
    UINT16 charSequence; // index into a table of unicode character sequences
  };

  UINT8  rendition; // rendition
  CharacterColor  foregroundColor; // foreground color
  CharacterColor  backgroundColor; // background color
public:
  //FIXME: following a hack to cope with various color spaces
  // it brings the rendition pipeline further out of balance,
  // which it was anyway as the result of various additions.
  bool   isTransparent(const ColorEntry* base) const;
  bool   isBold(const ColorEntry* base) const;
public:
  friend bool operator == (const Character& a, const Character& b);
  friend bool operator != (const Character& a, const Character& b);
};

inline bool operator == (const Character& a, const Character& b)
{ 
  return a.character == b.character && 
         a.rendition == b.rendition && 
         a.foregroundColor == b.foregroundColor && 
         a.backgroundColor == b.backgroundColor;
}

inline bool operator != (const Character& a, const Character& b)
{
  return    a.character != b.character || 
            a.rendition != b.rendition || 
            a.foregroundColor != b.foregroundColor || 
            a.backgroundColor != b.backgroundColor;
}

inline bool Character::isTransparent(const ColorEntry* base) const
{
  return ((backgroundColor.t == COLOR_SPACE_DEFAULT) && 
          base[backgroundColor.u+0+(backgroundColor.v?BASE_COLORS:0)].transparent)
      || ((backgroundColor.t == COLOR_SPACE_SYSTEM) && 
          base[backgroundColor.u+2+(backgroundColor.v?BASE_COLORS:0)].transparent);
}

inline bool Character::isBold(const ColorEntry* base) const
{
  return (backgroundColor.t == COLOR_SPACE_DEFAULT) && 
            base[backgroundColor.u+0+(backgroundColor.v?BASE_COLORS:0)].bold
      || (backgroundColor.t == COLOR_SPACE_SYSTEM) && 
            base[backgroundColor.u+2+(backgroundColor.v?BASE_COLORS:0)].bold;
}

extern unsigned short vt100_graphics[32];

}

#endif // TECOMMON_H

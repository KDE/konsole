/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CHARACTER_H
#define CHARACTER_H

// Konsole
#include "CharacterColor.h"
#include "CharacterWidth.h"
#include "ExtendedCharTable.h"
#include "Hangul.h"
#include "LineBlockCharacters.h"

// Qt
#include <QVector>

namespace Konsole
{
typedef quint32 LineProperty;

typedef quint16 RenditionFlags;

typedef quint16 ExtraFlags;

/* clang-format off */
const int LINE_DEFAULT              = 0;
const int LINE_WRAPPED              = (1 << 0);
const int LINE_DOUBLEWIDTH          = (1 << 1);
const int LINE_DOUBLEHEIGHT_TOP     = (1 << 2);
const int LINE_DOUBLEHEIGHT_BOTTOM  = (1 << 3);
const int LINE_PROMPT_START         = (1 << 4);
const int LINE_INPUT_START          = (1 << 5);
const int LINE_OUTPUT_START         = (1 << 6);
const int LINE_LEN_POS              = 8;
const int LINE_LEN_MASK             = (0xfff << LINE_LEN_POS);

const RenditionFlags DEFAULT_RENDITION  = 0;
const RenditionFlags RE_BOLD            = (1 << 0);
const RenditionFlags RE_BLINK           = (1 << 1);
const RenditionFlags RE_UNDERLINE       = (1 << 2);
const RenditionFlags RE_REVERSE         = (1 << 3); // Screen only
const RenditionFlags RE_ITALIC          = (1 << 4);
const RenditionFlags RE_CURSOR          = (1 << 5);
const RenditionFlags RE_EXTENDED_CHAR   = (1 << 6);
const RenditionFlags RE_FAINT           = (1 << 7);
const RenditionFlags RE_STRIKEOUT       = (1 << 8);
const RenditionFlags RE_CONCEAL         = (1 << 9);
const RenditionFlags RE_OVERLINE        = (1 << 10);
const RenditionFlags RE_SELECTED        = (1 << 11);
const RenditionFlags RE_TRANSPARENT        = (1 << 12);

const ExtraFlags EF_UNREAL       = 0;
const ExtraFlags EF_REAL         = (1 << 0);
const ExtraFlags EF_REPL         = (3 << 1);
const ExtraFlags EF_REPL_NONE    = (0 << 1);
const ExtraFlags EF_REPL_PROMPT  = (1 << 1);
const ExtraFlags EF_REPL_INPUT   = (2 << 1);
const ExtraFlags EF_REPL_OUTPUT  = (3 << 1);

#define setRepl(f, m) (((f) & ~EF_REPL) | ((m) * EF_REPL_PROMPT))
#define LineLength(f) static_cast<int>(((f) & LINE_LEN_MASK) >> LINE_LEN_POS)
#define SetLineLength(f, l) (((f) & ~LINE_LEN_MASK) | ((l) << LINE_LEN_POS))
/* clang-format on */

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
     * @param _flags Extra flags describing the character. not directly related to its rendition
     */
    explicit constexpr Character(uint _c = ' ',
                                 CharacterColor _f = CharacterColor(COLOR_SPACE_DEFAULT, DEFAULT_FORE_COLOR),
                                 CharacterColor _b = CharacterColor(COLOR_SPACE_DEFAULT, DEFAULT_BACK_COLOR),
                                 RenditionFlags _r = DEFAULT_RENDITION,
                                 ExtraFlags _flags = EF_REAL)
        : character(_c)
        , rendition(_r)
        , foregroundColor(_f)
        , backgroundColor(_b)
        , flags(_flags)
    {
    }

    /** The unicode character value for this character.
     *
     * if RE_EXTENDED_CHAR is set, character is a hash code which can be used to
     * look up the unicode character sequence in the ExtendedCharTable used to
     * create the sequence.
     */
    uint character;

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
    ExtraFlags flags;

    /**
     * returns true if the format (color, rendition flag) of the compared characters is equal
     */
    constexpr bool equalsFormat(const Character &other) const;

    /**
     * Compares two characters and returns true if they have the same unicode character value,
     * rendition and colors.
     */
    friend constexpr bool operator==(const Character &a, const Character &b);

    /**
     * Compares two characters and returns true if they have different unicode character values,
     * renditions or colors.
     */
    friend constexpr bool operator!=(const Character &a, const Character &b);

    constexpr bool isSpace() const
    {
        if (rendition & RE_EXTENDED_CHAR) {
            return false;
        } else {
            return QChar(character).isSpace();
        }
    }

    int width() const
    {
        return width(character);
    }

    int repl() const
    {
        return flags & EF_REPL;
    }

    static int width(uint ucs4)
    {
        // ASCII
        if (ucs4 >= 0x20 && ucs4 < 0x7f)
            return 1;

        if (ucs4 >= 0xA0 && ucs4 <= 0xFF)
            return 1;

        // NULL
        if (ucs4 == 0)
            return 0;

        // Control chars
        if ((ucs4 > 0x0 && ucs4 < 0x20) || (ucs4 >= 0x7F && ucs4 < 0xA0))
            return -1;

        return characterWidth(ucs4);
    }

    static int stringWidth(const uint *ucs4Str, int len)
    {
        int w = 0;
        Hangul::SyllablePos hangulSyllablePos = Hangul::NotInSyllable;

        for (int i = 0; i < len; ++i) {
            const uint c = ucs4Str[i];

            if (!Hangul::isHangul(c)) {
                w += width(c);
                hangulSyllablePos = Hangul::NotInSyllable;
            } else {
                w += Hangul::width(c, width(c), hangulSyllablePos);
            }
        }
        return w;
    }

    inline static int stringWidth(const QString &str)
    {
        QVector<uint> ucs4Str = str.toUcs4();
        return stringWidth(ucs4Str.constData(), ucs4Str.length());
    }

    inline bool canBeGrouped(bool bidirectionalEnabled, bool isDoubleWidth) const
    {
        if (character <= 0x7e) {
            return true;
        }

        if (QChar::script(character) == QChar::Script_Braille) {
            return false;
        }

        return (rendition & RE_EXTENDED_CHAR) || (bidirectionalEnabled && !isDoubleWidth);
    }

    inline uint baseCodePoint() const
    {
        if (rendition & RE_EXTENDED_CHAR) {
            ushort extendedCharLength = 0;
            const uint *chars = ExtendedCharTable::instance.lookupExtendedChar(character, extendedCharLength);
            // FIXME: Coverity-Dereferencing chars, which is known to be nullptr
            return chars[0];
        }
        return character;
    }

    inline bool isSameScript(Character lhs) const
    {
        const QChar::Script script = QChar::script(lhs.baseCodePoint());
        const QChar::Script currentScript = QChar::script(baseCodePoint());
        if (currentScript == QChar::Script_Common || script == QChar::Script_Common || currentScript == QChar::Script_Inherited
            || script == QChar::Script_Inherited) {
            return true;
        }
        return currentScript == script;
    };

    inline bool hasSameColors(Character lhs) const
    {
        return lhs.foregroundColor == foregroundColor && lhs.backgroundColor == backgroundColor;
    }

    inline bool hasSameRendition(Character lhs) const
    {
        return (lhs.rendition & ~RE_EXTENDED_CHAR) == (rendition & ~RE_EXTENDED_CHAR) && lhs.flags == flags;
    };

    inline bool hasSameLineDrawStatus(Character lhs) const
    {
        const bool lineDraw = LineBlockCharacters::canDraw(character);
        return LineBlockCharacters::canDraw(lhs.character) == lineDraw;
    };

    inline bool hasSameAttributes(Character lhs) const
    {
        return hasSameColors(lhs) && hasSameRendition(lhs) && hasSameLineDrawStatus(lhs) && isSameScript(lhs);
    }

    inline bool isRightHalfOfDoubleWide() const
    {
        return character == 0;
    }

    inline void setRightHalfOfDoubleWide()
    {
        character = 0;
    }
};

constexpr bool operator==(const Character &a, const Character &b)
{
    return a.character == b.character && a.equalsFormat(b);
}

constexpr bool operator!=(const Character &a, const Character &b)
{
    return !operator==(a, b);
}

constexpr bool Character::equalsFormat(const Character &other) const
{
    return backgroundColor == other.backgroundColor && foregroundColor == other.foregroundColor && rendition == other.rendition;
}
}
Q_DECLARE_TYPEINFO(Konsole::Character, Q_MOVABLE_TYPE);

#endif // CHARACTER_H

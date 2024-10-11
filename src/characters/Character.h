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

/* clang-format off */
const int LINE_WRAPPED              = (1 << 0);
const int LINE_DOUBLEWIDTH          = (1 << 1);
const int LINE_DOUBLEHEIGHT_TOP     = (1 << 2);
const int LINE_DOUBLEHEIGHT_BOTTOM  = (1 << 3);
const int LINE_PROMPT_START         = (1 << 4);
const int LINE_INPUT_START          = (1 << 5);
const int LINE_OUTPUT_START         = (1 << 6);
/* clang-format on */

namespace Konsole
{
#pragma pack(1)
class LineProperty
{
public:
    explicit constexpr LineProperty(quint16 f = 0, uint l = 0, uint c = 0)
        : flags({f})
        , length(l)
        , counter(c)
    {
    }
    union {
        quint16 all;
        struct {
            uint wrapped : 1;
            uint doublewidth : 1;
            uint doubleheight_top : 1;
            uint doubleheight_bottom : 1;
            uint prompt_start : 1;
            uint output_start : 1;
            uint input_start : 1;
            uint output : 1;
            uint error : 1;
        } f;
    } flags;
    qint16 length;
    quint16 counter;
    bool operator!=(const LineProperty &rhs) const
    {
        return (flags.all != rhs.flags.all);
    }
    void resetStarts()
    {
        flags.all &= ~(LINE_PROMPT_START | LINE_INPUT_START | LINE_OUTPUT_START);
    }
    quint16 getStarts() const
    {
        return flags.all & (LINE_PROMPT_START | LINE_INPUT_START | LINE_OUTPUT_START);
    }
    void setStarts(quint16 starts)
    {
        flags.all = (flags.all & ~(LINE_PROMPT_START | LINE_INPUT_START | LINE_OUTPUT_START)) | starts;
    }
};

typedef union {
    quint16 all;
    struct {
        uint bold : 1;
        uint blink : 1;
        uint transparent : 1;
        uint reverse : 1;
        uint italic : 1;
        uint cursor : 1;
        uint extended : 1;
        uint faint : 1;
        uint strikeout : 1;
        uint conceal : 1;
        uint overline : 1;
        uint selected : 1;
        uint underline : 4;
    } f;
} RenditionFlagsC;
typedef quint16 RenditionFlags;
#pragma pack()

typedef quint16 ExtraFlags;

/* clang-format off */
const RenditionFlags DEFAULT_RENDITION  = 0;
const RenditionFlags RE_BOLD            = (1 << 0);
const RenditionFlags RE_BLINK           = (1 << 1);
const RenditionFlags RE_TRANSPARENT     = (1 << 2);
const RenditionFlags RE_REVERSE         = (1 << 3); // Screen only
const RenditionFlags RE_ITALIC          = (1 << 4);
const RenditionFlags RE_CURSOR          = (1 << 5);
const RenditionFlags RE_EXTENDED_CHAR   = (1 << 6);
const RenditionFlags RE_FAINT           = (1 << 7);
const RenditionFlags RE_STRIKEOUT       = (1 << 8);
const RenditionFlags RE_CONCEAL         = (1 << 9);
const RenditionFlags RE_OVERLINE        = (1 << 10);
const RenditionFlags RE_SELECTED        = (1 << 11);
const RenditionFlags RE_UNDERLINE_MASK  = (15 << 12);
const RenditionFlags RE_UNDERLINE_NONE  = 0;
const RenditionFlags RE_UNDERLINE       = 1;
const RenditionFlags RE_UNDERLINE_DOUBLE= 2;
const RenditionFlags RE_UNDERLINE_CURL  = 3;
const RenditionFlags RE_UNDERLINE_DOT   = 4;
const RenditionFlags RE_UNDERLINE_DASH  = 5;
const RenditionFlags RE_UNDERLINE_BIT   = (1 << 12);
// Masks of flags that matter for drawing what is below/above the text
const RenditionFlags RE_MASK_UNDER = RE_TRANSPARENT | RE_REVERSE | RE_CURSOR | RE_SELECTED;
const RenditionFlags RE_MASK_ABOVE = RE_TRANSPARENT | RE_REVERSE | RE_CURSOR | RE_SELECTED | RE_STRIKEOUT | RE_CONCEAL | RE_OVERLINE | RE_UNDERLINE_MASK;

// flags that affect how the text is drawn
// without RE_REVERSE, since the foreground color is calculated
const RenditionFlags RE_TEXTDRAWING = RE_BOLD | RE_BLINK | RE_TRANSPARENT | RE_ITALIC | RE_CURSOR | RE_FAINT | RE_SELECTED;


const ExtraFlags EF_UNREAL       = 0;
const ExtraFlags EF_REAL         = (1 << 0);
const ExtraFlags EF_REPL         = (3 << 1);
const ExtraFlags EF_REPL_NONE    = (0 << 1);
const ExtraFlags EF_REPL_PROMPT  = (1 << 1);
const ExtraFlags EF_REPL_INPUT   = (2 << 1);
const ExtraFlags EF_REPL_OUTPUT  = (3 << 1);
const ExtraFlags EF_UNDERLINE_COLOR = (15 << 3);
const ExtraFlags EF_UNDERLINE_COLOR_1 = (1 << 3);
const ExtraFlags EF_EMOJI_REPRESENTATION = (1 << 7);
const ExtraFlags EF_ASCII_WORD = (1 << 8);
const ExtraFlags EF_BRAHMIC_WORD = (1 << 9);

#define SetULColor(f, m) (((f) & ~EF_UNDERLINE_COLOR) | ((m) * EF_UNDERLINE_COLOR_1))
#define setRepl(f, m) (((f) & ~EF_REPL) | ((m) * EF_REPL_PROMPT))
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
                                 RenditionFlags _r = DEFAULT_RENDITION | RE_TRANSPARENT,
                                 ExtraFlags _flags = EF_REAL)
        : character(_c)
        , rendition({_r})
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
    char32_t character;

    /** A combination of RENDITION flags which specify options for drawing the character. */
    RenditionFlagsC rendition;

    /** The foreground color used to draw this character. */
    CharacterColor foregroundColor;

    /** The color used to draw this character's background. */
    CharacterColor backgroundColor;

    /** Flags which are not specific to rendition
     * Indicate whether this character really exists, or exists simply as place holder.
     * REPL mode
     * Character type (script, etc.)
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
        if (rendition.f.extended) {
            return false;
        } else {
            return QChar::isSpace(character);
        }
    }

    int width(bool ignoreWcWidth = false) const
    {
        return width(character, ignoreWcWidth);
    }

    int repl() const
    {
        return flags & EF_REPL;
    }

    static constexpr int emojiPresentation1Start = 8986;
    static constexpr int emojiPresentation1End = 11093;
    static constexpr int emojiPresentation2Start = 126980;
    static constexpr int emojiPresentation2End = 129782;
    /* clang-format off */
    static constexpr uint64_t emojiPresentation1Bits[] = {
        0x3, 0x0, 0x0, 0x2478000, 0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0xc00001800000000, 0x3ffc00000000000, 0x200002000000000, 0x4100c1800030080, 0x308090b010000,
        0x2e14000000004000, 0x3800000000000000, 0x2000400000, 0x0, 0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
        0x840000000000006
    };
    static constexpr uint64_t emojiPresentation2Bits[] = {
        0x1, 0x0, 0x0, 0x800, 0x0, 0x0, 0x7fe400, 0x2ffffffc00000000,
        0x77c80000400000, 0x3000, 0x0, 0xf000000000000000,
        0xfffbfe001fffffff, 0xfdffffffffffffff, 0xfffffffff000ffff, 0xfff11ffff000f87f,
        0xd7ffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xf9ffffffffffffff,
        0x3ffffffffffffff, 0x40000ffffff780, 0x100060000, 0xff80000000000000,
        0xffffffffffffffff, 0xf000000000000fff, 0xffffffffffffffff, 0x1ff01800e0e7103, 0x0, 0x0, 0x0, 0x10fff0000000,
        0x0, 0x0, 0x0, 0x0, 0xff7fffffffffff00, 0xfffffffffffffffb, 0xffffffffffffffff, 0xfffffffffffffff,
        0x0, 0xf1f1f00000000000, 0xf07ff1fffffff007, 0x7f00ff03ff003
    };
    /* clang-format on */

    static bool emojiPresentation(uint ucs4)
    {
        if (ucs4 >= emojiPresentation1Start && ucs4 <= emojiPresentation1End) {
            return (emojiPresentation1Bits[(ucs4 - emojiPresentation1Start) / 64] & (uint64_t(1) << ((ucs4 - emojiPresentation1Start) % 64))) != 0;
        } else if (ucs4 >= emojiPresentation2Start && ucs4 <= emojiPresentation2End) {
            return (emojiPresentation2Bits[(ucs4 - emojiPresentation2Start) / 64] & (uint64_t(1) << ((ucs4 - emojiPresentation2Start) % 64))) != 0;
        }
        return false;
    }

    static constexpr int emoji1Start = 8252;
    static constexpr int emoji1End = 12953;
    static constexpr int emoji2Start = 126980;
    static constexpr int emoji2End = 129782;
    /* clang-format off */
    static constexpr uint64_t emoji1Bits[] = {
        0x2001, 0x0, 0x0, 0x2000004000000000, 0x0, 0x60003f000000, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x1000c0000000, 0x0, 0x0, 0x70ffe00000080000, 0x0,
        0x0, 0x0, 0x40, 0x0, 0x0, 0x400c00000000000, 0x8000000000000010, 0x700c44d2132401f7,
        0x8000169800fff050, 0x30c831afc0000c, 0x7bf0600001ac1306, 0x1801022054bf242, 0x1800b850900, 0x1000200e000000, 0x8, 0x0,
        0x0, 0x0, 0x0, 0x300000000000000, 0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x180000e00, 0x2100000, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x10000000000000,
        0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
        0x0, 0x28000000
    };
    static constexpr uint64_t emoji2Bits[] = {
        0x1, 0x0, 0x0, 0x800, 0x0, 0xc00300000000000, 0x7fe400, 0x6ffffffc00000000,
        0x7fc80000400000, 0x3000, 0x0, 0xf000000000000000,
        0xffffffff3fffffff, 0xffffffffffffffff, 0xfffffffffcecffff, 0xfffb9fffffffffff,
        0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xfbffffffffffffff,
        0x3ffffffffffffff, 0x7f980ffffff7e0, 0xc1006013000613c8, 0xffc08810a700e001,
        0xffffffffffffffff, 0xf000000000000fff, 0xffffffffffffffff, 0x1ff91a3fe0e7f83, 0x0, 0x0, 0x0, 0x10fff0000000,
        0x0, 0x0, 0x0, 0x0, 0xff7fffffffffff00, 0xfffffffffffffffb, 0xffffffffffffffff, 0xfffffffffffffff,
        0x0, 0xf1f1f00000000000, 0xf07ff1fffffff007, 0x7f00ff03ff003
    };
    /* clang-format on */

    static bool emoji(uint ucs4)
    {
        if (ucs4 >= emoji1Start && ucs4 <= emoji1End) {
            return (emoji1Bits[(ucs4 - emoji1Start) / 64] & (uint64_t(1) << ((ucs4 - emoji1Start) % 64))) != 0;
        } else if (ucs4 >= emoji2Start && ucs4 <= emoji2End) {
            return (emoji2Bits[(ucs4 - emoji2Start) / 64] & (uint64_t(1) << ((ucs4 - emoji2Start) % 64))) != 0;
        }
        return false;
    }

    static int width(uint ucs4, bool ignoreWcWidth = false)
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

        if (ignoreWcWidth && 0x04DC0 <= ucs4 && ucs4 <= 0x04DFF) {
            // Yijing Hexagram Symbols have wcwidth 2, but unicode width 1
            return 1;
        }

        return characterWidth(ucs4);
    }

    static int stringWidth(const char32_t *ucs4Str, int len, bool ignoreWcWidth = false)
    {
        int w = 0;
        Hangul::SyllablePos hangulSyllablePos = Hangul::NotInSyllable;

        for (int i = 0; i < len; ++i) {
            const uint c = ucs4Str[i];

            if (!Hangul::isHangul(c)) {
                w += width(c, ignoreWcWidth);
                hangulSyllablePos = Hangul::NotInSyllable;
            } else {
                w += Hangul::width(c, width(c, ignoreWcWidth), hangulSyllablePos);
            }
        }
        return w;
    }

    inline static int stringWidth(const QString &str, bool ignoreWcWidth = false)
    {
        const auto ucs4Str = str.toStdU32String();
        return stringWidth(ucs4Str.data(), ucs4Str.size(), ignoreWcWidth);
    }

    inline uint baseCodePoint() const
    {
        if (rendition.f.extended) {
            ushort extendedCharLength = 0;
            const char32_t *chars = ExtendedCharTable::instance.lookupExtendedChar(character, extendedCharLength);
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
    }

    inline bool hasSameColors(Character lhs) const
    {
        return lhs.foregroundColor == foregroundColor && lhs.backgroundColor == backgroundColor;
    }

    inline bool hasSameRendition(Character lhs) const
    {
        return (lhs.rendition.all & ~RE_EXTENDED_CHAR) == (rendition.all & ~RE_EXTENDED_CHAR) && lhs.flags == flags;
    }

    inline bool hasSameLineDrawStatus(Character lhs) const
    {
        const bool lineDraw = LineBlockCharacters::canDraw(character);
        return LineBlockCharacters::canDraw(lhs.character) == lineDraw;
    }

    inline bool hasSameBrailleStatus(Character lhs) const
    {
        const bool braille = LineBlockCharacters::isBraille(character);
        return LineBlockCharacters::isBraille(lhs.character) == braille;
    }

    inline bool hasSameAttributes(Character lhs) const
    {
        return hasSameColors(lhs) && hasSameRendition(lhs) && hasSameLineDrawStatus(lhs) && isSameScript(lhs) && hasSameBrailleStatus(lhs);
    }

    inline bool notSameAttributesText(Character lhs) const
    {
        // Only compare attributes used for drawing text
        return (lhs.rendition.all & RE_TEXTDRAWING) != (rendition.all & RE_TEXTDRAWING) || lhs.foregroundColor != foregroundColor;
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
    return backgroundColor == other.backgroundColor && foregroundColor == other.foregroundColor && rendition.all == other.rendition.all;
}
}
Q_DECLARE_TYPEINFO(Konsole::Character, Q_MOVABLE_TYPE);

#endif // CHARACTER_H

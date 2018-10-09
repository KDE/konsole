/*
    This file is part of Konsole, a terminal emulator for KDE.

    Copyright 2018 by Mariusz Glebocki <mglb@arccos-1.net>

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

#ifndef CATEGORY_PROPERTY_VALUE
#define CATEGORY_PROPERTY_VALUE(val, sym, intVal)
#endif
#ifndef CATEGORY_PROPERTY_GROUP
#define CATEGORY_PROPERTY_GROUP(val, sym, intVal)
#endif

CATEGORY_PROPERTY_VALUE(Lu, UppercaseLetter,        1<<0)   // an uppercase letter
CATEGORY_PROPERTY_VALUE(Ll, LowercaseLetter,        1<<1)   // a lowercase letter
CATEGORY_PROPERTY_VALUE(Lt, TitlecaseLetter,        1<<2)   // a digraphic character, with first part uppercase
CATEGORY_PROPERTY_GROUP(LC, CasedLetter,            1<<0|1<<1|1<<2)
CATEGORY_PROPERTY_VALUE(Lm, ModifierLetter,         1<<3)   // a modifier letter
CATEGORY_PROPERTY_VALUE(Lo, OtherLetter,            1<<4)   // other letters, including syllables and ideographs
CATEGORY_PROPERTY_GROUP(L,  Letter,                 1<<0|1<<1|1<<2|1<<3|1<<4)
CATEGORY_PROPERTY_VALUE(Mn, NonspacingMark,         1<<5)   // a nonspacing combining mark (zero advance width)
CATEGORY_PROPERTY_VALUE(Mc, SpacingMark,            1<<6)   // a spacing combining mark (positive advance width)
CATEGORY_PROPERTY_VALUE(Me, EnclosingMark,          1<<7)   // an enclosing combining mark
CATEGORY_PROPERTY_GROUP(M,  Mark,                   1<<5|1<<6|1<<7)
CATEGORY_PROPERTY_VALUE(Nd, DecimalNumber,          1<<8)   // a decimal digit
CATEGORY_PROPERTY_VALUE(Nl, LetterNumber,           1<<9)   // a letterlike numeric character
CATEGORY_PROPERTY_VALUE(No, OtherNumber,            1<<10)  // a numeric character of other type
CATEGORY_PROPERTY_GROUP(N,  Number,                 1<<8|1<<9|1<<10)
CATEGORY_PROPERTY_VALUE(Pc, ConnectorPunctuation,   1<<11)  // a connecting punctuation mark, like a tie
CATEGORY_PROPERTY_VALUE(Pd, DashPunctuation,        1<<12)  // a dash or hyphen punctuation mark
CATEGORY_PROPERTY_VALUE(Ps, OpenPunctuation,        1<<13)  // an opening punctuation mark (of a pair)
CATEGORY_PROPERTY_VALUE(Pe, ClosePunctuation,       1<<14)  // a closing punctuation mark (of a pair)
CATEGORY_PROPERTY_VALUE(Pi, InitialPunctuation,     1<<15)  // an initial quotation mark
CATEGORY_PROPERTY_VALUE(Pf, FinalPunctuation,       1<<16)  // a final quotation mark
CATEGORY_PROPERTY_VALUE(Po, OtherPunctuation,       1<<17)  // a punctuation mark of other type
CATEGORY_PROPERTY_GROUP(P,  Punctuation,            1<<11|1<<12|1<<13|1<<14|1<<15|1<<16|1<<17)
CATEGORY_PROPERTY_VALUE(Sm, MathSymbol,             1<<18)  // a symbol of mathematical use
CATEGORY_PROPERTY_VALUE(Sc, CurrencySymbol,         1<<19)  // a currency sign
CATEGORY_PROPERTY_VALUE(Sk, ModifierSymbol,         1<<20)  // a non-letterlike modifier symbol
CATEGORY_PROPERTY_VALUE(So, OtherSymbol,            1<<21)  // a symbol of other type
CATEGORY_PROPERTY_GROUP(S,  Symbol,                 1<<18|1<<19|1<<20|1<<21)
CATEGORY_PROPERTY_VALUE(Zs, SpaceSeparator,         1<<22)  // a space character (of various non-zero widths)
CATEGORY_PROPERTY_VALUE(Zl, LineSeparator,          1<<23)  // U+2028 LINE SEPARATOR only
CATEGORY_PROPERTY_VALUE(Zp, ParagraphSeparator,     1<<24)  // U+2029 PARAGRAPH SEPARATOR only
CATEGORY_PROPERTY_GROUP(Z,  Separator,              1<<22|1<<23|1<<24)
CATEGORY_PROPERTY_VALUE(Cc, Control,                1<<25)  // a C0 or C1 control code
CATEGORY_PROPERTY_VALUE(Cf, Format,                 1<<26)  // a format control character
CATEGORY_PROPERTY_VALUE(Cs, Surrogate,              1<<27)  // a surrogate code point
CATEGORY_PROPERTY_VALUE(Co, PrivateUse,             1<<28)  // a private-use character
CATEGORY_PROPERTY_VALUE(Cn, Unassigned,             1<<29)  // a reserved unassigned code point or a noncharacter
CATEGORY_PROPERTY_GROUP(C,  Other,                  1<<25|1<<26|1<<27|1<<28|1<<29)

#undef CATEGORY_PROPERTY_VALUE
#undef CATEGORY_PROPERTY_GROUP

/**************************************/

#ifndef EAST_ASIAN_WIDTH_PROPERTY_VALUE
#define EAST_ASIAN_WIDTH_PROPERTY_VALUE(val, sym, intVal)
#endif

EAST_ASIAN_WIDTH_PROPERTY_VALUE(A,  Ambiguous,  1<<0)
EAST_ASIAN_WIDTH_PROPERTY_VALUE(F,  Fullwidth,  1<<1)
EAST_ASIAN_WIDTH_PROPERTY_VALUE(H,  Halfwidth,  1<<2)
EAST_ASIAN_WIDTH_PROPERTY_VALUE(N,  Neutral,    1<<3)
EAST_ASIAN_WIDTH_PROPERTY_VALUE(Na, Narrow,     1<<4)
EAST_ASIAN_WIDTH_PROPERTY_VALUE(W,  Wide,       1<<5)

#undef EAST_ASIAN_WIDTH_PROPERTY_VALUE

/**************************************/

#ifndef EMOJI_PROPERTY_VALUE
#define EMOJI_PROPERTY_VALUE(val, sym, intVal)
#endif

EMOJI_PROPERTY_VALUE(,                      None,               0)
EMOJI_PROPERTY_VALUE(Emoji,                 Emoji,              1<<0)
EMOJI_PROPERTY_VALUE(Emoji_Presentation,    EmojiPresentation,  1<<1)
EMOJI_PROPERTY_VALUE(Emoji_Modifier,        EmojiModifier,      1<<2)
EMOJI_PROPERTY_VALUE(Emoji_Modifier_Base,   EmojiModifier_Base, 1<<3)
EMOJI_PROPERTY_VALUE(Emoji_Component,       EmojiComponent,     1<<4)

#undef EMOJI_PROPERTY_VALUE

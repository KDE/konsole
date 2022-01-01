/*
    SPDX-FileCopyrightText: 2022 Luis Javier Merino Morán <ninjalj@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef HANGUL_H
#define HANGUL_H

#include <QtGlobal>

namespace Konsole
{

class Character;

class Hangul
{
public:
    // See HangulSyllableType.txt from the Unicode data distribution
    enum SyllableType {
        Not_Applicable,
        Leading_Jamo,
        Vowel_Jamo,
        Trailing_Jamo,

        LV_Syllable,
        LVT_Syllable,
    };

    enum SyllablePos {
        NotInSyllable,
        AtLeadingJamo,
        AtVowelJamo,
        AtTrailingJamo,
    };

    static int width(uint c, int widthFromTable, enum SyllablePos &syllablePos);
    static bool combinesWith(Character prev, uint c);

    static bool isHangul(const uint c)
    {
        return (c >= 0x1100 && c <= 0x11ff) || (c >= 0xa960 && c <= 0xa97f) || (c >= 0xd7b0 && c <= 0xd7ff) || (c >= 0xac00 && c <= 0xd7a3);
    }

private:
    static bool isLeadingJamo(const uint c)
    {
        return (c >= 0x1100 && c <= 0x115f) || (c >= 0xa960 && c <= 0xa97f);
    }

    static bool isVowelJamo(const uint c)
    {
        return (c >= 0x1160 && c <= 0x11a7) || (c >= 0xd7b0 && c <= 0xd7c6);
    }

    static bool isTrailingJamo(const uint c)
    {
        return (c >= 0x11a8 && c <= 0x11ff) || (c >= 0xd7cb && c <= 0xd7ff);
    }

    static bool isLvSyllable(const uint c)
    {
        return (c >= 0xac00 && c <= 0xd7a3) && (c % 0x1c == 0);
    }

    static bool isLvtSyllable(const uint c)
    {
        return (c >= 0xac00 && c <= 0xd7a3) && (c % 0x1c != 0);
    }

    static SyllableType jamoType(const uint c)
    {
        // clang-format off
        if (isLeadingJamo(c))  return Leading_Jamo;
        if (isVowelJamo(c))    return Vowel_Jamo;
        if (isTrailingJamo(c)) return Trailing_Jamo;
        if (isLvSyllable(c))   return LV_Syllable;
        if (isLvtSyllable(c))  return LVT_Syllable;
        return Not_Applicable;
        // clang-format on
    }

    static void updateHangulSyllablePos(Hangul::SyllablePos &syllablePos, uint c);
    static bool validSyllableContinuation(Hangul::SyllablePos syllablePos, uint c);
};

}

#endif // HANGUL_H

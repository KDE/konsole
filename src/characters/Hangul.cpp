/*
    SPDX-FileCopyrightText: 2022 Luis Javier Merino Morán <ninjalj@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "Hangul.h"

// Qt
#include <QtGlobal>

// Konsole
#include "Character.h"

namespace Konsole
{

int Hangul::width(uint c, int widthFromTable, enum Hangul::SyllablePos &syllablePos)
{
    int w = 0;
    switch (syllablePos) {
    case NotInSyllable:
        if (isLeadingJamo(c)) {
            // new Hangul syllable
            syllablePos = AtLeadingJamo;
            w += widthFromTable;
        } else if (isLvSyllable(c)) {
            // new Hangul syllable
            syllablePos = AtVowelJamo;
            w += widthFromTable;
        } else if (isLvtSyllable(c)) {
            // new Hangul syllable
            syllablePos = AtTrailingJamo;
            w += widthFromTable;
        } else if (isVowelJamo(c)) {
            // invalid here, isolated Jamo
            w += 2;
        } else if (isTrailingJamo(c)) {
            // invalid here, isolated Jamo
            w += 2;
        }
        break;
    case AtLeadingJamo:
        if (isLeadingJamo(c)) {
            // conjoin
        } else if (isLvSyllable(c)) {
            // new Hangul syllable
            syllablePos = AtVowelJamo;
            w += widthFromTable;
        } else if (isLvtSyllable(c)) {
            // new Hangul syllable
            syllablePos = AtTrailingJamo;
            w += widthFromTable;
        } else if (isVowelJamo(c)) {
            syllablePos = AtVowelJamo;
            // conjoin
        } else if (isTrailingJamo(c)) {
            // invalid here, isolated Jamo
            syllablePos = NotInSyllable;
            w += 2;
        }
        break;
    case AtVowelJamo:
        if (isLeadingJamo(c)) {
            // new Hangul syllable
            syllablePos = AtLeadingJamo;
            w += widthFromTable;
        } else if (isLvSyllable(c)) {
            // new Hangul syllable
            syllablePos = AtVowelJamo;
            w += widthFromTable;
        } else if (isLvtSyllable(c)) {
            // new Hangul syllable
            syllablePos = AtTrailingJamo;
            w += widthFromTable;
        } else if (isVowelJamo(c)) {
            // conjoin
        } else if (isTrailingJamo(c)) {
            syllablePos = AtTrailingJamo;
            // conjoin
        }
        break;
    case AtTrailingJamo:
        if (isLeadingJamo(c)) {
            // new Hangul syllable
            syllablePos = AtLeadingJamo;
            w += widthFromTable;
        } else if (isLvSyllable(c)) {
            // new Hangul syllable
            syllablePos = AtVowelJamo;
            w += widthFromTable;
        } else if (isLvtSyllable(c)) {
            // new Hangul syllable
            syllablePos = AtTrailingJamo;
            w += widthFromTable;
        } else if (isVowelJamo(c)) {
            // invalid here, isolated Jamo
            syllablePos = NotInSyllable;
            w += 2;
        } else if (isTrailingJamo(c)) {
            // conjoin
        }
    }

    return w;
}

void Hangul::updateHangulSyllablePos(Hangul::SyllablePos &syllablePos, uint c)
{
    if (!isHangul(c)) {
        syllablePos = NotInSyllable;
    }
    (void)Hangul::width(c, 0, syllablePos);
}

bool Hangul::validSyllableContinuation(Hangul::SyllablePos syllablePos, uint c)
{
    SyllableType type = jamoType(c);

    switch (syllablePos) {
    case AtLeadingJamo:
        return type != Trailing_Jamo && type != Not_Applicable;
    case AtVowelJamo:
        return type == Trailing_Jamo || type == Vowel_Jamo;
    case AtTrailingJamo:
        return type == Trailing_Jamo;
    default:
        return false;
    }
}

bool Hangul::combinesWith(Character prevChar, uint c)
{
    Hangul::SyllablePos syllablePos = Hangul::NotInSyllable;

    if ((prevChar.rendition & RE_EXTENDED_CHAR) == 0) {
        const uint prev = prevChar.character;
        updateHangulSyllablePos(syllablePos, prev);
    } else {
        ushort extendedCharLength;
        const uint *oldChars = ExtendedCharTable::instance.lookupExtendedChar(prevChar.character, extendedCharLength);
        if (oldChars == nullptr) {
            return false;
        } else {
            for (int i = 0; i < extendedCharLength; i++) {
                updateHangulSyllablePos(syllablePos, oldChars[i]);
            }
        }
    }

    return validSyllableContinuation(syllablePos, c);
}

}

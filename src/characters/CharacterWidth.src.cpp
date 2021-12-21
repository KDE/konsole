/*
    SPDX-FileCopyrightText: 2018 Mariusz Glebocki <mglb@arccos-1.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
/* clang-format off */
«*NOTE:-----------------------------------------------------------------------*»
// Typing in "«" and "»" characters in some keyboard layouts (X11):
//
//   English/UK: AltGr+Z AltGr+X
//   EurKEY:     AltGr+[ AltGr+]
//   German:     AltGr+X AltGr+Y
//   Polish:     AltGr+9 AltGr+0
//   English/US: N/A; You can try EurKEY which extends En/US layout with extra
//               characters available with AltGr[+Shift].
//
// Alternatively, you can use e.g. "<<<" and ">>>" and convert it to the valid
// characters using sed or your editor's replace function.
//
// This text  will not appear in an output file.
«*-----------------------------------------------------------------------:NOTE*»
//
// «gen-file-warning»
//
// CharacterWidth.cpp file is automatically generated - do not edit it.
//
// To build uni2characterwidth binary, add
// -DKONSOLE_BUILD_UNI2CHARACTERWIDTH=TRUE to cmake-options
//
// To change anything here, edit CharacterWidth.src.cpp and regenerate the file
// using following command:
//
// «cmdline»
//

#include "CharacterWidth.h"
#include "konsolecharacters_export.h"

struct Range {
    uint first, last;
};
/* clang-format on */

struct RangeLut {
    int8_t width;
    const Range *const lut;
    int size;
};

enum {
    InvalidWidth = INT8_MIN,
};

/* clang-format off */
static constexpr const int8_t DIRECT_LUT[] = {«!fmt "% d":«direct-lut:
    «!repeat 32:«:«»,»»
»»};

«ranges-luts:«:
static constexpr const Range «name»[] = {«!fmt "%#.6x":«ranges:
    «!repeat 8:«:{«first»,«last»},»»
»»};
»»

static constexpr const RangeLut RANGE_LUT_LIST[] = {«ranges-lut-list:
    «:{«!fmt "% d":«width»», «!fmt "%-16s":«name»», «size»},»
»};
static constexpr const int RANGE_LUT_LIST_SIZE = «ranges-lut-list-size»;
/* clang-format on */

int KONSOLECHARACTERS_EXPORT characterWidth(uint ucs4)
{
    if (Q_LIKELY(ucs4 < sizeof(DIRECT_LUT))) {
        return DIRECT_LUT[ucs4];
    }

    for (auto rl = RANGE_LUT_LIST; rl->lut != nullptr; ++rl) {
        int l = 0;
        int r = rl->size - 1;
        while (l <= r) {
            const int m = (l + r) / 2;
            if (rl->lut[m].last < ucs4) {
                l = m + 1;
            } else if (rl->lut[m].first > ucs4) {
                r = m - 1;
            } else {
                return rl->width;
            }
        }
    }

    return RANGE_LUT_LIST[RANGE_LUT_LIST_SIZE - 1].width;
}

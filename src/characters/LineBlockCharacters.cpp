/*
    This file is part of Konsole, a terminal emulator for KDE.

    Copyright 2018-2019 by Mariusz Glebocki <mglb@arccos-1.net>
    Copyright 2018 by Martin T. H. Sandsmark <martin.sandsmark@kde.org>
    Copyright 2015-2018 by Kurt Hindenburg <kurt.hindenburg@gmail.com>
    Copyright 2005 by Maksim Orlovich <maksim@kde.org>

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

// Own
#include "LineBlockCharacters.h"

// Qt
#include <QPainterPath>

namespace Konsole {
namespace LineBlockCharacters {

enum LineType {
    LtNone   = 0,
    LtDouble = 1,
    LtLight  = 2,
    LtHeavy  = 3
};

// PackedLineTypes is an 8-bit number representing types of 4 line character's lines. Each line is
// represented by 2 bits. Lines order, starting from MSB: top, right, bottom, left.
static inline constexpr quint8 makePackedLineTypes(LineType top, LineType right, LineType bottom, LineType left)
{
    return (int(top) & 3) << 6 | (int(right) & 3) << 4 | (int(bottom) & 3) << 2 | (int(left) & 3);
}

static constexpr const quint8 PackedLineTypesLut[] = {
    //                  top       right     bottom    left
    makePackedLineTypes(LtNone  , LtLight , LtNone  , LtLight ), /* U+2500 ─ */
    makePackedLineTypes(LtNone  , LtHeavy , LtNone  , LtHeavy ), /* U+2501 ━ */
    makePackedLineTypes(LtLight , LtNone  , LtLight , LtNone  ), /* U+2502 │ */
    makePackedLineTypes(LtHeavy , LtNone  , LtHeavy , LtNone  ), /* U+2503 ┃ */
    0, 0, 0, 0, 0, 0, 0, 0, /* U+2504-0x250b */
    makePackedLineTypes(LtNone  , LtLight , LtLight , LtNone  ), /* U+250C ┌ */
    makePackedLineTypes(LtNone  , LtHeavy , LtLight , LtNone  ), /* U+250D ┍ */
    makePackedLineTypes(LtNone  , LtLight , LtHeavy , LtNone  ), /* U+250E ┎ */
    makePackedLineTypes(LtNone  , LtHeavy , LtHeavy , LtNone  ), /* U+250F ┏ */
    makePackedLineTypes(LtNone  , LtNone  , LtLight , LtLight ), /* U+2510 ┐ */
    makePackedLineTypes(LtNone  , LtNone  , LtLight , LtHeavy ), /* U+2511 ┑ */
    makePackedLineTypes(LtNone  , LtNone  , LtHeavy , LtLight ), /* U+2512 ┒ */
    makePackedLineTypes(LtNone  , LtNone  , LtHeavy , LtHeavy ), /* U+2513 ┓ */
    makePackedLineTypes(LtLight , LtLight , LtNone  , LtNone  ), /* U+2514 └ */
    makePackedLineTypes(LtLight , LtHeavy , LtNone  , LtNone  ), /* U+2515 ┕ */
    makePackedLineTypes(LtHeavy , LtLight , LtNone  , LtNone  ), /* U+2516 ┖ */
    makePackedLineTypes(LtHeavy , LtHeavy , LtNone  , LtNone  ), /* U+2517 ┗ */
    makePackedLineTypes(LtLight , LtNone  , LtNone  , LtLight ), /* U+2518 ┘ */
    makePackedLineTypes(LtLight , LtNone  , LtNone  , LtHeavy ), /* U+2519 ┙ */
    makePackedLineTypes(LtHeavy , LtNone  , LtNone  , LtLight ), /* U+251A ┚ */
    makePackedLineTypes(LtHeavy , LtNone  , LtNone  , LtHeavy ), /* U+251B ┛ */
    makePackedLineTypes(LtLight , LtLight , LtLight , LtNone  ), /* U+251C ├ */
    makePackedLineTypes(LtLight , LtHeavy , LtLight , LtNone  ), /* U+251D ┝ */
    makePackedLineTypes(LtHeavy , LtLight , LtLight , LtNone  ), /* U+251E ┞ */
    makePackedLineTypes(LtLight , LtLight , LtHeavy , LtNone  ), /* U+251F ┟ */
    makePackedLineTypes(LtHeavy , LtLight , LtHeavy , LtNone  ), /* U+2520 ┠ */
    makePackedLineTypes(LtHeavy , LtHeavy , LtLight , LtNone  ), /* U+2521 ┡ */
    makePackedLineTypes(LtLight , LtHeavy , LtHeavy , LtNone  ), /* U+2522 ┢ */
    makePackedLineTypes(LtHeavy , LtHeavy , LtHeavy , LtNone  ), /* U+2523 ┣ */
    makePackedLineTypes(LtLight , LtNone  , LtLight , LtLight ), /* U+2524 ┤ */
    makePackedLineTypes(LtLight , LtNone  , LtLight , LtHeavy ), /* U+2525 ┥ */
    makePackedLineTypes(LtHeavy , LtNone  , LtLight , LtLight ), /* U+2526 ┦ */
    makePackedLineTypes(LtLight , LtNone  , LtHeavy , LtLight ), /* U+2527 ┧ */
    makePackedLineTypes(LtHeavy , LtNone  , LtHeavy , LtLight ), /* U+2528 ┨ */
    makePackedLineTypes(LtHeavy , LtNone  , LtLight , LtHeavy ), /* U+2529 ┩ */
    makePackedLineTypes(LtLight , LtNone  , LtHeavy , LtHeavy ), /* U+252A ┪ */
    makePackedLineTypes(LtHeavy , LtNone  , LtHeavy , LtHeavy ), /* U+252B ┫ */
    makePackedLineTypes(LtNone  , LtLight , LtLight , LtLight ), /* U+252C ┬ */
    makePackedLineTypes(LtNone  , LtLight , LtLight , LtHeavy ), /* U+252D ┭ */
    makePackedLineTypes(LtNone  , LtHeavy , LtLight , LtLight ), /* U+252E ┮ */
    makePackedLineTypes(LtNone  , LtHeavy , LtLight , LtHeavy ), /* U+252F ┯ */
    makePackedLineTypes(LtNone  , LtLight , LtHeavy , LtLight ), /* U+2530 ┰ */
    makePackedLineTypes(LtNone  , LtLight , LtHeavy , LtHeavy ), /* U+2531 ┱ */
    makePackedLineTypes(LtNone  , LtHeavy , LtHeavy , LtLight ), /* U+2532 ┲ */
    makePackedLineTypes(LtNone  , LtHeavy , LtHeavy , LtHeavy ), /* U+2533 ┳ */
    makePackedLineTypes(LtLight , LtLight , LtNone  , LtLight ), /* U+2534 ┴ */
    makePackedLineTypes(LtLight , LtLight , LtNone  , LtHeavy ), /* U+2535 ┵ */
    makePackedLineTypes(LtLight , LtHeavy , LtNone  , LtLight ), /* U+2536 ┶ */
    makePackedLineTypes(LtLight , LtHeavy , LtNone  , LtHeavy ), /* U+2537 ┷ */
    makePackedLineTypes(LtHeavy , LtLight , LtNone  , LtLight ), /* U+2538 ┸ */
    makePackedLineTypes(LtHeavy , LtLight , LtNone  , LtHeavy ), /* U+2539 ┹ */
    makePackedLineTypes(LtHeavy , LtHeavy , LtNone  , LtLight ), /* U+253A ┺ */
    makePackedLineTypes(LtHeavy , LtHeavy , LtNone  , LtHeavy ), /* U+253B ┻ */
    makePackedLineTypes(LtLight , LtLight , LtLight , LtLight ), /* U+253C ┼ */
    makePackedLineTypes(LtLight , LtLight , LtLight , LtHeavy ), /* U+253D ┽ */
    makePackedLineTypes(LtLight , LtHeavy , LtLight , LtLight ), /* U+253E ┾ */
    makePackedLineTypes(LtLight , LtHeavy , LtLight , LtHeavy ), /* U+253F ┿ */
    makePackedLineTypes(LtHeavy , LtLight , LtLight , LtLight ), /* U+2540 ╀ */
    makePackedLineTypes(LtLight , LtLight , LtHeavy , LtLight ), /* U+2541 ╁ */
    makePackedLineTypes(LtHeavy , LtLight , LtHeavy , LtLight ), /* U+2542 ╂ */
    makePackedLineTypes(LtHeavy , LtLight , LtLight , LtHeavy ), /* U+2543 ╃ */
    makePackedLineTypes(LtHeavy , LtHeavy , LtLight , LtLight ), /* U+2544 ╄ */
    makePackedLineTypes(LtLight , LtLight , LtHeavy , LtHeavy ), /* U+2545 ╅ */
    makePackedLineTypes(LtLight , LtHeavy , LtHeavy , LtLight ), /* U+2546 ╆ */
    makePackedLineTypes(LtHeavy , LtHeavy , LtLight , LtHeavy ), /* U+2547 ╇ */
    makePackedLineTypes(LtLight , LtHeavy , LtHeavy , LtHeavy ), /* U+2548 ╈ */
    makePackedLineTypes(LtHeavy , LtLight , LtHeavy , LtHeavy ), /* U+2549 ╉ */
    makePackedLineTypes(LtHeavy , LtHeavy , LtHeavy , LtLight ), /* U+254A ╊ */
    makePackedLineTypes(LtHeavy , LtHeavy , LtHeavy , LtHeavy ), /* U+254B ╋ */
    0, 0, 0, 0, /* U+254C - U+254F */
    makePackedLineTypes(LtNone  , LtDouble, LtNone  , LtDouble), /* U+2550 ═ */
    makePackedLineTypes(LtDouble, LtNone  , LtDouble, LtNone  ), /* U+2551 ║ */
    makePackedLineTypes(LtNone  , LtDouble, LtLight , LtNone  ), /* U+2552 ╒ */
    makePackedLineTypes(LtNone  , LtLight , LtDouble, LtNone  ), /* U+2553 ╓ */
    makePackedLineTypes(LtNone  , LtDouble, LtDouble, LtNone  ), /* U+2554 ╔ */
    makePackedLineTypes(LtNone  , LtNone  , LtLight , LtDouble), /* U+2555 ╕ */
    makePackedLineTypes(LtNone  , LtNone  , LtDouble, LtLight ), /* U+2556 ╖ */
    makePackedLineTypes(LtNone  , LtNone  , LtDouble, LtDouble), /* U+2557 ╗ */
    makePackedLineTypes(LtLight , LtDouble, LtNone  , LtNone  ), /* U+2558 ╘ */
    makePackedLineTypes(LtDouble, LtLight , LtNone  , LtNone  ), /* U+2559 ╙ */
    makePackedLineTypes(LtDouble, LtDouble, LtNone  , LtNone  ), /* U+255A ╚ */
    makePackedLineTypes(LtLight , LtNone  , LtNone  , LtDouble), /* U+255B ╛ */
    makePackedLineTypes(LtDouble, LtNone  , LtNone  , LtLight ), /* U+255C ╜ */
    makePackedLineTypes(LtDouble, LtNone  , LtNone  , LtDouble), /* U+255D ╝ */
    makePackedLineTypes(LtLight , LtDouble, LtLight , LtNone  ), /* U+255E ╞ */
    makePackedLineTypes(LtDouble, LtLight , LtDouble, LtNone  ), /* U+255F ╟ */
    makePackedLineTypes(LtDouble, LtDouble, LtDouble, LtNone  ), /* U+2560 ╠ */
    makePackedLineTypes(LtLight , LtNone  , LtLight , LtDouble), /* U+2561 ╡ */
    makePackedLineTypes(LtDouble, LtNone  , LtDouble, LtLight ), /* U+2562 ╢ */
    makePackedLineTypes(LtDouble, LtNone  , LtDouble, LtDouble), /* U+2563 ╣ */
    makePackedLineTypes(LtNone  , LtDouble, LtLight , LtDouble), /* U+2564 ╤ */
    makePackedLineTypes(LtNone  , LtLight , LtDouble, LtLight ), /* U+2565 ╥ */
    makePackedLineTypes(LtNone  , LtDouble, LtDouble, LtDouble), /* U+2566 ╦ */
    makePackedLineTypes(LtLight , LtDouble, LtNone  , LtDouble), /* U+2567 ╧ */
    makePackedLineTypes(LtDouble, LtLight , LtNone  , LtLight ), /* U+2568 ╨ */
    makePackedLineTypes(LtDouble, LtDouble, LtNone  , LtDouble), /* U+2569 ╩ */
    makePackedLineTypes(LtLight , LtDouble, LtLight , LtDouble), /* U+256A ╪ */
    makePackedLineTypes(LtDouble, LtLight , LtDouble, LtLight ), /* U+256B ╫ */
    makePackedLineTypes(LtDouble, LtDouble, LtDouble, LtDouble), /* U+256C ╬ */
    0, 0, 0, 0, 0, 0, 0, /* U+256D - U+2573 */
    makePackedLineTypes(LtNone  , LtNone  , LtNone  , LtLight ), /* U+2574 ╴ */
    makePackedLineTypes(LtLight , LtNone  , LtNone  , LtNone  ), /* U+2575 ╵ */
    makePackedLineTypes(LtNone  , LtLight , LtNone  , LtNone  ), /* U+2576 ╶ */
    makePackedLineTypes(LtNone  , LtNone  , LtLight , LtNone  ), /* U+2577 ╷ */
    makePackedLineTypes(LtNone  , LtNone  , LtNone  , LtHeavy ), /* U+2578 ╸ */
    makePackedLineTypes(LtHeavy , LtNone  , LtNone  , LtNone  ), /* U+2579 ╹ */
    makePackedLineTypes(LtNone  , LtHeavy , LtNone  , LtNone  ), /* U+257A ╺ */
    makePackedLineTypes(LtNone  , LtNone  , LtHeavy , LtNone  ), /* U+257B ╻ */
    makePackedLineTypes(LtNone  , LtHeavy , LtNone  , LtLight ), /* U+257C ╼ */
    makePackedLineTypes(LtLight , LtNone  , LtHeavy , LtNone  ), /* U+257D ╽ */
    makePackedLineTypes(LtNone  , LtLight , LtNone  , LtHeavy ), /* U+257E ╾ */
    makePackedLineTypes(LtHeavy , LtNone  , LtLight , LtNone  ), /* U+257F ╿ */
};



// Bitwise rotate left
template <typename T>
inline static T rotateBitsLeft(T value, quint8 amount)
{
    static_assert (std::is_unsigned<T>(), "T must be unsigned type");
    Q_ASSERT(amount < sizeof(value) * 8);
    return value << amount | value >> (sizeof(value) * 8 - amount);
}

inline static const QPen pen(const QPainter &paint, uint lineWidth)
{
    return QPen(paint.pen().brush(), lineWidth, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
}



static inline uint lineWidth(uint fontWidth, bool heavy, bool bold)
{
    static const qreal LightWidthToFontWidthRatio = 1.0 / 6.5;
    static const qreal HeavyHalfExtraToLightRatio = 1.0 / 3.0;
    static const qreal BoldCoefficient            = 1.5;

    //        ▄▄▄▄▄▄▄ } heavyHalfExtraWidth  ⎫
    // ██████████████ } lightWidth           ⎬ heavyWidth
    //        ▀▀▀▀▀▀▀                        ⎭
    //  light  heavy

    const qreal baseWidth           = fontWidth * LightWidthToFontWidthRatio;
    const qreal boldCoeff           = bold ? BoldCoefficient : 1.0;
    // Unless font size is too small, make bold lines at least 1px wider than regular lines
    const qreal minWidth            = bold && fontWidth >= 7 ? baseWidth + 1.0 : 1.0;
    const uint  lightWidth          = qRound(qMax(baseWidth * boldCoeff, minWidth));
    const uint  heavyHalfExtraWidth = qRound(qMax(lightWidth * HeavyHalfExtraToLightRatio, 1.0));

    return heavy ? lightWidth + 2 * heavyHalfExtraWidth : lightWidth;
}

// Draws characters composed of straight solid lines
static bool drawBasicLineCharacter(QPainter& paint, int x, int y, int w, int h, uchar code,
                                   bool bold)
{
    quint8 packedLineTypes = code >= sizeof(PackedLineTypesLut) ? 0 : PackedLineTypesLut[code];
    if (packedLineTypes == 0) {
        return false;
    }

    const uint lightLineWidth      = lineWidth(w, false, bold);
    const uint heavyLineWidth      = lineWidth(w, true, bold);
    // Distance from double line's parallel axis to each line's parallel axis
    const uint doubleLinesDistance = lightLineWidth;

    const QPen lightPen = pen(paint, lightLineWidth);
    const QPen heavyPen = pen(paint, heavyLineWidth);

    static constexpr const unsigned LinesNum = 4;

    // Pixel aligned center point
    const QPointF center = {
        x + int(w/2) + 0.5 * (lightLineWidth % 2),
        y + int(h/2) + 0.5 * (lightLineWidth % 2),
    };

    // Lines starting points, on the cell edges
    const QPointF origin[] = {
        QPointF(center.x(), y             ),
        QPointF(x+w       , center.y()    ),
        QPointF(center.x(), y+h           ),
        QPointF(x         , center.y()    ),
    };
    // Unit vectors with directions from center to the line's origin point
    static const QPointF dir[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};

    const auto removeLineType = [&packedLineTypes](quint8 lineId)->void {
        lineId = LinesNum - 1 - lineId % LinesNum;
        packedLineTypes &= ~(3 << (2 * lineId));
    };
    const auto getLineType = [&packedLineTypes](quint8 lineId)->LineType {
        lineId = LinesNum - 1 - lineId % LinesNum;
        return LineType(packedLineTypes >> 2 * lineId & 3);
    };

    QPainterPath lightPath; // PainterPath for light lines (Painter Path Light)
    QPainterPath heavyPath; // PainterPath for heavy lines (Painter Path Heavy)
    // Returns ppl or pph depending on line type
    const auto pathForLine = [&](quint8 lineId) -> QPainterPath& {
        Q_ASSERT(getLineType(lineId) != LtNone);
        return getLineType(lineId) == LtHeavy ? heavyPath : lightPath;
    };

    // Process all single up-down/left-right lines for every character that has them. Doing it here
    // reduces amount of combinations below.
    // Fully draws: ╋ ╂ ┃ ┿ ┼ │ ━ ─
    for (unsigned int topIndex = 0; topIndex < LinesNum/2; topIndex++) {
        unsigned iB = (topIndex + 2) % LinesNum;
        const bool isSingleLine = (getLineType(topIndex) == LtLight
                                || getLineType(topIndex) == LtHeavy);
        if (isSingleLine && getLineType(topIndex) == getLineType(iB)) {
            pathForLine(topIndex).moveTo(origin[topIndex]);
            pathForLine(topIndex).lineTo(origin[iB]);
            removeLineType(topIndex);
            removeLineType(iB);
        }
    }

    // Find base rotation of a character and map rotated line indices to the original rotation's
    // indices. The base rotation is defined as the one with largest packedLineTypes value. This way
    // we can use the same code for drawing 4 possible character rotations (see switch() below)

    uint topIndex = 0; // index of an original top line in a base rotation
    quint8 basePackedLineTypes = packedLineTypes;
    for (uint i = 0; i < LinesNum; i++) {
        const quint8 rotatedPackedLineTypes = rotateBitsLeft(packedLineTypes, i * 2);
        if (rotatedPackedLineTypes > basePackedLineTypes) {
            topIndex = i;
            basePackedLineTypes = rotatedPackedLineTypes;
        }
    }
    uint rightIndex  = (topIndex + 1) % LinesNum;
    uint bottomIndex = (topIndex + 2) % LinesNum;
    uint leftIndex   = (topIndex + 3) % LinesNum;

    // Common paths
    const auto drawDoubleUpRightShorterLine = [&](quint8 top, quint8 right) { // ╚
        lightPath.moveTo(origin[top] + dir[right] * doubleLinesDistance);
        lightPath.lineTo(center + (dir[right] + dir[top]) * doubleLinesDistance);
        lightPath.lineTo(origin[right] + dir[top] * doubleLinesDistance);
    };
    const auto drawUpRight = [&](quint8 top, quint8 right) { // └┗
        pathForLine(top).moveTo(origin[top]);
        pathForLine(top).lineTo(center);
        pathForLine(top).lineTo(origin[right]);
    };

    switch (basePackedLineTypes) {
    case makePackedLineTypes(LtHeavy , LtNone  , LtLight , LtNone  ): // ╿ ; ╼ ╽ ╾ ╊ ╇ ╉ ╈ ╀ ┾ ╁ ┽
        lightPath.moveTo(origin[bottomIndex]);
        lightPath.lineTo(center + dir[topIndex] * lightLineWidth / 2.0);
        Q_FALLTHROUGH();
    case makePackedLineTypes(LtHeavy , LtNone  , LtNone  , LtNone  ): // ╹ ; ╺ ╻ ╸ ┻ ┣ ┳ ┫ ┸ ┝ ┰ ┥
    case makePackedLineTypes(LtLight , LtNone  , LtNone  , LtNone  ): // ╵ ; ╶ ╷ ╴ ┷ ┠ ┯ ┨ ┴ ├ ┬ ┤
        pathForLine(topIndex).moveTo(origin[topIndex]);
        pathForLine(topIndex).lineTo(center);
        break;

    case makePackedLineTypes(LtHeavy , LtHeavy , LtLight , LtLight ): // ╄ ; ╃ ╆ ╅
        drawUpRight(bottomIndex, leftIndex);
        Q_FALLTHROUGH();
    case makePackedLineTypes(LtHeavy , LtHeavy , LtNone  , LtNone  ): // ┗ ; ┛ ┏ ┓
    case makePackedLineTypes(LtLight , LtLight , LtNone  , LtNone  ): // └ ; ┘ ┌ ┐
        drawUpRight(topIndex, rightIndex);
        break;

    case makePackedLineTypes(LtHeavy , LtLight , LtNone  , LtNone  ): // ┖ ; ┙ ┍ ┒
        qSwap(leftIndex, rightIndex);
        Q_FALLTHROUGH();
    case makePackedLineTypes(LtHeavy , LtNone  , LtNone  , LtLight ): // ┚ ; ┕ ┎ ┑
        lightPath.moveTo(origin[leftIndex]);
        lightPath.lineTo(center);
        heavyPath.moveTo(origin[topIndex]);
        heavyPath.lineTo(center + dir[bottomIndex] * lightLineWidth / 2.0);
        break;

    case makePackedLineTypes(LtLight , LtDouble, LtNone  , LtNone  ): // ╘ ; ╜ ╓ ╕
        qSwap(leftIndex, rightIndex);
        Q_FALLTHROUGH();
    case makePackedLineTypes(LtLight , LtNone  , LtNone  , LtDouble): // ╛ ; ╙ ╒ ╖
        lightPath.moveTo(origin[topIndex]);
        lightPath.lineTo(center + dir[bottomIndex] * doubleLinesDistance);
        lightPath.lineTo(origin[leftIndex] + dir[bottomIndex] * doubleLinesDistance);
        lightPath.moveTo(origin[leftIndex] - dir[bottomIndex] * doubleLinesDistance);
        lightPath.lineTo(center - dir[bottomIndex] * doubleLinesDistance);
        break;

    case makePackedLineTypes(LtHeavy , LtHeavy , LtLight , LtNone  ): // ┡ ; ┹ ┪ ┲
        qSwap(leftIndex, bottomIndex);
        qSwap(rightIndex, topIndex);
        Q_FALLTHROUGH();
    case makePackedLineTypes(LtHeavy , LtHeavy , LtNone  , LtLight ): // ┺ ; ┩ ┢ ┱
        drawUpRight(topIndex, rightIndex);
        lightPath.moveTo(origin[leftIndex]);
        lightPath.lineTo(center);
        break;

    case makePackedLineTypes(LtHeavy , LtLight , LtLight , LtNone  ): // ┞ ; ┵ ┧ ┮
        qSwap(leftIndex, rightIndex);
        Q_FALLTHROUGH();
    case makePackedLineTypes(LtHeavy , LtNone  , LtLight , LtLight ): // ┦ ; ┶ ┟ ┭
        heavyPath.moveTo(origin[topIndex]);
        heavyPath.lineTo(center + dir[bottomIndex] * lightLineWidth / 2.0);
        drawUpRight(bottomIndex, leftIndex);
        break;

    case makePackedLineTypes(LtLight , LtDouble, LtNone  , LtDouble): // ╧ ; ╟ ╢ ╤
        lightPath.moveTo(origin[topIndex]);
        lightPath.lineTo(center - dir[bottomIndex] * doubleLinesDistance);
        qSwap(leftIndex, bottomIndex);
        qSwap(rightIndex, topIndex);
        Q_FALLTHROUGH();
    case makePackedLineTypes(LtDouble, LtNone  , LtDouble, LtNone  ): // ║ ; ╫ ═ ╪
        lightPath.moveTo(origin[topIndex] + dir[leftIndex] * doubleLinesDistance);
        lightPath.lineTo(origin[bottomIndex] + dir[leftIndex] * doubleLinesDistance);
        lightPath.moveTo(origin[topIndex] + dir[rightIndex] * doubleLinesDistance);
        lightPath.lineTo(origin[bottomIndex] + dir[rightIndex] * doubleLinesDistance);
        break;

    case makePackedLineTypes(LtDouble, LtNone  , LtNone  , LtNone  ): // ╨ ; ╞ ╥ ╡
        lightPath.moveTo(origin[topIndex] + dir[leftIndex] * doubleLinesDistance);
        lightPath.lineTo(center + dir[leftIndex] * doubleLinesDistance);
        lightPath.moveTo(origin[topIndex] + dir[rightIndex] * doubleLinesDistance);
        lightPath.lineTo(center + dir[rightIndex] * doubleLinesDistance);
        break;

    case makePackedLineTypes(LtDouble, LtDouble, LtDouble, LtDouble): // ╬
        drawDoubleUpRightShorterLine(topIndex, rightIndex);
        drawDoubleUpRightShorterLine(bottomIndex, rightIndex);
        drawDoubleUpRightShorterLine(topIndex, leftIndex);
        drawDoubleUpRightShorterLine(bottomIndex, leftIndex);
        break;

    case makePackedLineTypes(LtDouble, LtDouble, LtDouble, LtNone  ): // ╠ ; ╩ ╣ ╦
        lightPath.moveTo(origin[topIndex] + dir[leftIndex] * doubleLinesDistance);
        lightPath.lineTo(origin[bottomIndex] + dir[leftIndex] * doubleLinesDistance);
        drawDoubleUpRightShorterLine(topIndex, rightIndex);
        drawDoubleUpRightShorterLine(bottomIndex, rightIndex);
        break;

    case makePackedLineTypes(LtDouble, LtDouble, LtNone  , LtNone  ): // ╚ ; ╝ ╔ ╗
        lightPath.moveTo(origin[topIndex] + dir[leftIndex] * doubleLinesDistance);
        lightPath.lineTo(center + (dir[leftIndex] + dir[bottomIndex]) * doubleLinesDistance);
        lightPath.lineTo(origin[rightIndex] + dir[bottomIndex] * doubleLinesDistance);
        drawDoubleUpRightShorterLine(topIndex, rightIndex);
        break;
    }

    // Draw paths
    if (!lightPath.isEmpty()) {
        paint.strokePath(lightPath, lightPen);
    }
    if (!heavyPath.isEmpty()) {
        paint.strokePath(heavyPath, heavyPen);
    }

    return true;
}

static inline bool drawDashedLineCharacter(QPainter &paint, int x, int y, int w, int h, uchar code,
                                           bool bold)
{
    if (!((0x04 <= code && code <= 0x0B) || (0x4C <= code && code <= 0x4F))) {
        return false;
    }

    const uint lightLineWidth = lineWidth(w, false, bold);
    const uint heavyLineWidth = lineWidth(w, true, bold);

    const auto lightPen = pen(paint, lightLineWidth);
    const auto heavyPen = pen(paint, heavyLineWidth);

    // Pixel aligned center point
    const QPointF center = {
        int(x + w/2.0) + 0.5 * (lightLineWidth%2),
        int(y + h/2.0) + 0.5 * (lightLineWidth%2),
    };

    const qreal halfGapH   = qMax(w / 20.0, 0.5);
    const qreal halfGapV   = qMax(h / 26.0, 0.5);
    // For some reason vertical double dash has bigger gap
    const qreal halfGapDDV = qMax(h / 14.0, 0.5);

    static const int LinesNumMax = 4;

    enum Orientation {Horizontal, Vertical};
    struct {
        int         linesNum;
        Orientation orientation;
        QPen        pen;
        qreal       halfGap;
    } lineProps;

    switch (code) {
    case 0x4C: lineProps = {2, Horizontal, lightPen, halfGapH  }; break; // ╌
    case 0x4D: lineProps = {2, Horizontal, heavyPen, halfGapH  }; break; // ╍
    case 0x4E: lineProps = {2, Vertical  , lightPen, halfGapDDV}; break; // ╎
    case 0x4F: lineProps = {2, Vertical  , heavyPen, halfGapDDV}; break; // ╏
    case 0x04: lineProps = {3, Horizontal, lightPen, halfGapH  }; break; // ┄
    case 0x05: lineProps = {3, Horizontal, heavyPen, halfGapH  }; break; // ┅
    case 0x06: lineProps = {3, Vertical  , lightPen, halfGapV  }; break; // ┆
    case 0x07: lineProps = {3, Vertical  , heavyPen, halfGapV  }; break; // ┇
    case 0x08: lineProps = {4, Horizontal, lightPen, halfGapH  }; break; // ┈
    case 0x09: lineProps = {4, Horizontal, heavyPen, halfGapH  }; break; // ┉
    case 0x0A: lineProps = {4, Vertical  , lightPen, halfGapV  }; break; // ┊
    case 0x0B: lineProps = {4, Vertical  , heavyPen, halfGapV  }; break; // ┋
    }

    Q_ASSERT(lineProps.linesNum <= LinesNumMax);
    const int size = (lineProps.orientation == Horizontal ? w : h);
    const int pos  = (lineProps.orientation == Horizontal ? x : y);
    QLineF lines[LinesNumMax];

    for (int i = 0; i < lineProps.linesNum; i++) {
        const qreal start = pos + qreal(size * (i  )) / lineProps.linesNum;
        const qreal end   = pos + qreal(size * (i+1)) / lineProps.linesNum;
        if (lineProps.orientation == Horizontal) {
            lines[i] = QLineF{start + lineProps.halfGap, center.y(),
                              end   - lineProps.halfGap, center.y()};
        } else {
            lines[i] = QLineF{center.x(), start + lineProps.halfGap,
                              center.x(), end   - lineProps.halfGap};
        }
    }

    const auto origPen = paint.pen();

    paint.setPen(lineProps.pen);
    paint.drawLines(lines, lineProps.linesNum);

    paint.setPen(origPen);
    return true;
}

static inline bool drawRoundedCornerLineCharacter(QPainter &paint, int x, int y, int w, int h,
                                                  uchar code, bool bold)
{
    if (!(0x6D <= code && code <= 0x70)) {
        return false;
    }

    const uint lightLineWidth = lineWidth(w, false, bold);
    const auto lightPen = pen(paint, lightLineWidth);

    // Pixel aligned center point
    const QPointF center = {
        int(x + w/2.0) + 0.5 * (lightLineWidth%2),
        int(y + h/2.0) + 0.5 * (lightLineWidth%2),
    };

    const int r = w * 3 / 8;
    const int d = 2 * r;

    QPainterPath path;

    // lineTo() between moveTo and arcTo is redundant - arcTo() draws line
    // to the arc's starting point
    switch (code) {
    case 0x6D: // BOX DRAWINGS LIGHT ARC DOWN AND RIGHT
        path.moveTo(center.x(), y + h);
        path.arcTo(center.x(), center.y(), d, d, 180, -90);
        path.lineTo(x + w, center.y());
        break;
    case 0x6E: // BOX DRAWINGS LIGHT ARC DOWN AND LEFT
        path.moveTo(center.x(), y + h);
        path.arcTo(center.x() - d, center.y(), d, d, 0, 90);
        path.lineTo(x, center.y());
        break;
    case 0x6F: // BOX DRAWINGS LIGHT ARC UP AND LEFT
        path.moveTo(center.x(), y);
        path.arcTo(center.x() - d, center.y() - d, d, d, 0, -90);
        path.lineTo(x, center.y());
        break;
    case 0x70: // BOX DRAWINGS LIGHT ARC UP AND RIGHT
        path.moveTo(center.x(), y);
        path.arcTo(center.x(), center.y() - d, d, d, 180, 90);
        path.lineTo(x + w, center.y());
        break;
    }
    paint.strokePath(path, lightPen);

    return true;
}

static inline bool drawDiagonalLineCharacter(QPainter &paint, int x, int y, int w, int h,
                                             uchar code, bool bold)
{
    if (!(0x71 <= code && code <= 0x73)) {
        return false;
    }

    const uint lightLineWidth = lineWidth(w, false, bold);
    const auto lightPen = pen(paint, lightLineWidth);

    const QLineF lines[] = {
        QLineF(x+w, y, x  , y+h), // '/'
        QLineF(x  , y, x+w, y+h), // '\'
    };

    const auto origPen = paint.pen();

    paint.setPen(lightPen);
    switch (code) {
    case 0x71: // BOX DRAWINGS LIGHT DIAGONAL UPPER RIGHT TO LOWER LEFT
        paint.drawLine(lines[0]);
        break;
    case 0x72: // BOX DRAWINGS LIGHT DIAGONAL UPPER LEFT TO LOWER RIGHT
        paint.drawLine(lines[1]);
        break;
    case 0x73: // BOX DRAWINGS LIGHT DIAGONAL CROSS
        paint.drawLines(lines, 2);
        break;
    }

    paint.setPen(origPen);
    return true;
}

static inline bool drawBlockCharacter(QPainter &paint, int x, int y, int w, int h, uchar code,
                                      bool bold)
{
    Q_UNUSED(bold)

    const QColor color = paint.pen().color();

    // Center point
    const QPointF center = {
        x + w/2.0,
        y + h/2.0,
    };

    // Default rect fills entire cell
    QRectF rect(x, y, w, h);

    // LOWER ONE EIGHTH BLOCK to LEFT ONE EIGHTH BLOCK
    if (code >= 0x81 && code <= 0x8f) {
        if (code < 0x88) { // Horizontal
            const qreal height = h * (0x88 - code) / 8.0;
            rect.setY(y + height);
            rect.setHeight(h - height);
        } else if (code > 0x88) { // Vertical
            const qreal width = w * (0x90 - code) / 8.0;
            rect.setWidth(width);
        }
        paint.fillRect(rect, color);

        return true;
    }

    // Combinations of quarter squares
    // LEFT ONE EIGHTH BLOCK to QUADRANT UPPER RIGHT AND LOWER LEFT AND LOWER RIGHT
    if (code >= 0x96 && code <= 0x9F) {
        const QRectF upperLeft (x         , y         , w/2.0, h/2.0);
        const QRectF upperRight(center.x(), y         , w/2.0, h/2.0);
        const QRectF lowerLeft (x         , center.y(), w/2.0, h/2.0);
        const QRectF lowerRight(center.x(), center.y(), w/2.0, h/2.0);

        QPainterPath path;

        switch (code) {
        case 0x96: // ▖
            path.addRect(lowerLeft);
            break;
        case 0x97: // ▗
            path.addRect(lowerRight);
            break;
        case 0x98: // ▘
            path.addRect(upperLeft);
            break;
        case 0x99: // ▙
            path.addRect(upperLeft);
            path.addRect(lowerLeft);
            path.addRect(lowerRight);
            break;
        case 0x9a: // ▚
            path.addRect(upperLeft);
            path.addRect(lowerRight);
            break;
        case 0x9b: // ▛
            path.addRect(upperLeft);
            path.addRect(upperRight);
            path.addRect(lowerLeft);
            break;
        case 0x9c: // ▜
            path.addRect(upperLeft);
            path.addRect(upperRight);
            path.addRect(lowerRight);
            break;
        case 0x9d: // ▝
            path.addRect(upperRight);
            break;
        case 0x9e: // ▞
            path.addRect(upperRight);
            path.addRect(lowerLeft);
            break;
        case 0x9f: // ▟
            path.addRect(upperRight);
            path.addRect(lowerLeft);
            path.addRect(lowerRight);
            break;
        }
        paint.fillPath(path, color);

        return true;
    }

    QBrush lightShade;
    QBrush mediumShade;
    QBrush darkShade;
    if (paint.testRenderHint(QPainter::Antialiasing)) {
        lightShade  = QColor(color.red(), color.green(), color.blue(), 64);
        mediumShade = QColor(color.red(), color.green(), color.blue(), 128);
        darkShade   = QColor(color.red(), color.green(), color.blue(), 192);
    } else {
        lightShade  = QBrush(color, Qt::Dense6Pattern);
        mediumShade = QBrush(color, Qt::Dense4Pattern);
        darkShade   = QBrush(color, Qt::Dense2Pattern);
    }
    // And the random stuff
    switch (code) {
    case 0x80: // Top half block
        rect.setHeight(h/2.0);
        paint.fillRect(rect, color);
        return true;
    case 0x90: // Right half block
        rect.moveLeft(center.x());
        paint.fillRect(rect, color);
        return true;
    case 0x94: // Top one eighth block
        rect.setHeight(h/8.0);
        paint.fillRect(rect, color);
        return true;
    case 0x95: { // Right one eighth block
            const qreal width = 7 * w / 8.0;
            rect.moveLeft(x + width);
            paint.fillRect(rect, color);
            return true;
        }
    case 0x91: // Light shade
        paint.fillRect(rect, lightShade);
        return true;
    case 0x92: // Medium shade
        paint.fillRect(rect, mediumShade);
        return true;
    case 0x93: // Dark shade
        paint.fillRect(rect, darkShade);
        return true;

    default:
        return false;
    }
}

void draw(QPainter &paint, const QRect &cellRect, const QChar &chr, bool bold)
{
    static const ushort FirstBoxDrawingCharacterCodePoint = 0x2500;
    const uchar code = chr.unicode() - FirstBoxDrawingCharacterCodePoint;

    int x = cellRect.x();
    int y = cellRect.y();
    int w = cellRect.width();
    int h = cellRect.height();

    // Each function below returns true when it has drawn the character, false otherwise.
    drawBasicLineCharacter(paint, x, y, w, h, code, bold)
            || drawDashedLineCharacter(paint, x, y, w, h, code, bold)
            || drawRoundedCornerLineCharacter(paint, x, y, w, h, code, bold)
            || drawDiagonalLineCharacter(paint, x, y, w, h, code, bold)
            || drawBlockCharacter(paint, x, y, w, h, code, bold);
}

} // namespace LineBlockCharacters
} // namespace Konsole

/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ColorFilter.h"

using namespace Konsole;

#include "ColorFilterHotSpot.h"

// This matches:
//   - an RGB-style string (e.g., #3e3, #feed) delimited by non-alphanumerics;
//   - or, a sequence of ASCII characters (e.g., foobar, Aquamarine, TOMATO).
// See the docs for `QColor::setNamedColor`.
const QRegularExpression
    ColorFilter::ColorRegExp(QStringLiteral("((?<![[:alnum:]])"
                                            "#[[:xdigit:]]{3,12}"
                                            "(?![[:alnum:]])|"
                                            "\\b[a-zA-Z]{3,20}\\b)"));

ColorFilter::ColorFilter()
{
    setRegExp(ColorRegExp);
}

QSharedPointer<HotSpot> ColorFilter::newHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QStringList &capturedTexts)
{
    QColor color(capturedTexts.at(1));

    // Make sure we've actually matched a color.
    if (!color.isValid()) {
        return nullptr;
    }

    return QSharedPointer<HotSpot>(new ColorFilterHotSpot(startLine, startColumn, endLine, endColumn, capturedTexts, color));
}

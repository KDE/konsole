/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ColorFilter.h"

using namespace Konsole;

#include "ColorFilterHotSpot.h"

const QRegularExpression ColorFilter::ColorRegExp(QStringLiteral("(#([[:xdigit:]]{12}|[[:xdigit:]]{9}|[[:xdigit:]]{6}))(?:[^[:alnum:]]|$)"));

ColorFilter::ColorFilter() 
{
    setRegExp(ColorRegExp);
}

QSharedPointer<HotSpot> ColorFilter::newHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QStringList &capturedTexts)
{
    QString colorName = capturedTexts.at(1);
    return QSharedPointer<HotSpot>(new ColorFilterHotSpot(startLine, startColumn, endLine, endColumn, capturedTexts, colorName));
}

/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COLORFILTER_H
#define COLORFILTER_H

#include "RegExpFilter.h"

namespace Konsole
{
class ColorFilter : public RegExpFilter
{
public:
    ColorFilter();

    static const QRegularExpression ColorRegExp;

protected:
    QSharedPointer<HotSpot> newHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QStringList &capturedTexts) override;
};
}

#endif

/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "RegExpFilterHotspot.h"

using namespace Konsole;

RegExpFilterHotSpot::RegExpFilterHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QStringList &capturedTexts)
    : HotSpot(startLine, startColumn, endLine, endColumn)
    , _capturedTexts(capturedTexts)
{
    setType(Marker);
}

void RegExpFilterHotSpot::activate(QObject *)
{
}

QStringList RegExpFilterHotSpot::capturedTexts() const
{
    return _capturedTexts;
}

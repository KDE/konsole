/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef REGEXP_FILTER_HOTSPOT
#define REGEXP_FILTER_HOTSPOT

#include "HotSpot.h"
#include <QStringList>

namespace Konsole
{
/**
 * Type of hotspot created by RegExpFilter.  The capturedTexts() method can be used to find the text
 * matched by the filter's regular expression.
 */
class RegExpFilterHotSpot : public HotSpot
{
public:
    RegExpFilterHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QStringList &capturedTexts);

    void activate(QObject *object = nullptr) override;

    /** Returns the texts found by the filter when matching the filter's regular expression */
    QStringList capturedTexts() const;

private:
    QStringList _capturedTexts;
};

}
#endif

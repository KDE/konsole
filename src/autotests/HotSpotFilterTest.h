/*
    SPDX-FileCopyrightText: 2022 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef HOTSPOTFILTERTEST_H
#define HOTSPOTFILTERTEST_H

#include "filterHotSpots/UrlFilter.h"

class HotSpotFilterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testUrlFilterRegex_data();
    void testUrlFilterRegex();
};

#endif // HOTSPOTFILTERTEST_H

/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef ESCAPE_SEQUENCE_URL_HOTSPOT
#define ESCAPE_SEQUENCE_URL_HOTSPOT

#include "HotSpot.h"

namespace Konsole
{
class EscapeSequenceUrlHotSpot : public HotSpot
{
public:
    EscapeSequenceUrlHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QString &text, const QString &url);
    void activate(QObject *obj) override;

private:
    QString _text;
    QString _url;
};

}
#endif

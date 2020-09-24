/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 2020 by Tomaz Canabrava <tcanabrava@gmail.com>

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

#ifndef ESCAPE_SEQUENCE_URL_HOTSPOT
#define ESCAPE_SEQUENCE_URL_HOTSPOT

#include "HotSpot.h"

namespace Konsole {

class EscapeSequenceUrlHotSpot : public HotSpot
{
public:
    EscapeSequenceUrlHotSpot(int startLine, int startColumn, int endLine, int endColumn,
            const QString &text, const QString &url);
    void activate(QObject *obj) override;

private:
    QString _text;
    QString _url;
};

}
#endif

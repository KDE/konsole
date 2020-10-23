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

#include "RegExpFilter.h"


#include "RegExpFilterHotspot.h"

using namespace Konsole;

RegExpFilter::RegExpFilter() :
    _searchText(QRegularExpression())
{
}


void RegExpFilter::setRegExp(const QRegularExpression &regExp)
{
    _searchText = regExp;
    _searchText.optimize();
}

QRegularExpression RegExpFilter::regExp() const
{
    return _searchText;
}

void RegExpFilter::process()
{
    const QString *text = buffer();

    Q_ASSERT(text);

    if (!_searchText.isValid() || _searchText.pattern().isEmpty()) {
        return;
    }

    QRegularExpressionMatchIterator iterator(_searchText.globalMatch(*text));
    while (iterator.hasNext()) {
        QRegularExpressionMatch match(iterator.next());
        std::pair<int, int> start = getLineColumn(match.capturedStart());
        std::pair<int, int> end = getLineColumn(match.capturedEnd());

        QSharedPointer<HotSpot> spot(
            newHotSpot(start.first, start.second,
                       end.first, end.second,
                       match.capturedTexts()
            )
        );

        if (spot == nullptr) {
            continue;
        }

        addHotSpot(spot);
    }
}

QSharedPointer<HotSpot> RegExpFilter::newHotSpot(int startLine, int startColumn, int endLine,
                                                int endColumn, const QStringList &capturedTexts)
{
    return QSharedPointer<HotSpot>(new RegExpFilterHotSpot(startLine, startColumn,
                                     endLine, endColumn, capturedTexts));
}

/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "RegExpFilter.h"

#include "RegExpFilterHotspot.h"

using namespace Konsole;

RegExpFilter::RegExpFilter()
    : _searchText(QRegularExpression())
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

        QSharedPointer<HotSpot> spot(newHotSpot(start.first, start.second, end.first, end.second, match.capturedTexts()));

        if (spot == nullptr) {
            continue;
        }

        addHotSpot(spot);
    }
}

QSharedPointer<HotSpot> RegExpFilter::newHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QStringList &capturedTexts)
{
    return QSharedPointer<HotSpot>(new RegExpFilterHotSpot(startLine, startColumn, endLine, endColumn, capturedTexts));
}

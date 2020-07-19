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

#include "UrlFilter.h"

using namespace Konsole;

#include "UrlFilterHotspot.h"

// Note:  Altering these regular expressions can have a major effect on the performance of the filters
// used for finding URLs in the text, especially if they are very general and could match very long
// pieces of text.
// Please be careful when altering them.
// protocolname:// or www. followed by anything other than whitespaces, <, >, ' or ", and ends before whitespaces, <, >, ', ", ], !, ), :, comma and dot
const QRegularExpression UrlFilter::FullUrlRegExp(
    QStringLiteral("(www\\.(?!\\.)|[a-z][a-z0-9+.-]*://)[^\\s<>'\"]+[^!,\\.\\s<>'\"\\]\\)\\:]"),
    QRegularExpression::OptimizeOnFirstUsageOption);

// email address:
// [word chars, dots or dashes]@[word chars, dots or dashes].[word chars]
const QRegularExpression UrlFilter::EmailAddressRegExp(
    QStringLiteral("\\b(\\w|\\.|-|\\+)+@(\\w|\\.|-)+\\.\\w+\\b"),
    QRegularExpression::OptimizeOnFirstUsageOption);

// matches full url or email address
const QRegularExpression UrlFilter::CompleteUrlRegExp(
    QLatin1Char('(') + FullUrlRegExp.pattern() + QLatin1Char('|') + EmailAddressRegExp.pattern() + QLatin1Char(')'),
    QRegularExpression::OptimizeOnFirstUsageOption);


UrlFilter::UrlFilter()
{
    setRegExp(CompleteUrlRegExp);
}

QSharedPointer<HotSpot> UrlFilter::newHotSpot(int startLine, int startColumn, int endLine,
                                             int endColumn, const QStringList &capturedTexts)
{
    return QSharedPointer<HotSpot>(new UrlFilterHotSpot(startLine, startColumn,
                                  endLine, endColumn, capturedTexts));
}

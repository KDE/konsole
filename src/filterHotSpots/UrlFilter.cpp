/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "UrlFilter.h"

using namespace Konsole;

#include "UrlFilterHotspot.h"

// Note:  Altering these regular expressions can have a major effect on the performance of the filters
// used for finding URLs in the text, especially if they are very general and could match very long
// pieces of text.
// Please be careful when altering them.
// protocolname:// or www. followed by anything other than whitespaces, <, >, ' or ", and ends before whitespaces, <, >, ', ", ], !, ), :, comma and dot
const QRegularExpression UrlFilter::FullUrlRegExp(QStringLiteral("(www\\.(?!\\.)|[a-z][a-z0-9+.-]*://)[^\\s<>'\"]+[^!,\\.\\s<>'\"\\]\\)\\:]"));

// email address:
// [word chars, dots or dashes]@[word chars, dots or dashes].[word chars]
const QRegularExpression UrlFilter::EmailAddressRegExp(QStringLiteral("\\b(\\w|\\.|-|\\+)+@(\\w|\\.|-)+\\.\\w+\\b"));

// matches full url or email address
const QRegularExpression UrlFilter::CompleteUrlRegExp(QLatin1Char('(') + FullUrlRegExp.pattern() + QLatin1Char('|') + EmailAddressRegExp.pattern()
                                                      + QLatin1Char(')'));

UrlFilter::UrlFilter()
{
    setRegExp(CompleteUrlRegExp);
}

QSharedPointer<HotSpot> UrlFilter::newHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QStringList &capturedTexts)
{
    return QSharedPointer<HotSpot>(new UrlFilterHotSpot(startLine, startColumn, endLine, endColumn, capturedTexts));
}

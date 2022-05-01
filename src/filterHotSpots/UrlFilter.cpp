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

// FullUrlRegExp is implemented based on:
// https://datatracker.ietf.org/doc/html/rfc3986
// See above URL for what "unreserved", "pct-encoded" ...etc mean, also
// for the regex used for each part of the url being matched against

// unreserved / pct-encoded / sub-delims
// [a-z0-9\\-._~%!$&'()*+,;=]
// The above string is used in various char[] below

// All () groups are non-capturing (by using "(?:)" notation)
// less bookkeeping on the PCRE engine side

// scheme://
// - Must start with an ASCII letter, preceeded by any non-word character,
//   so "http" but not "mhttp"
static const char scheme[] = "(?<=^|\\s|\\W)(?:[a-z][a-z0-9+\\-.]*://)";

// user:password@
static const char userInfo[] =
    "(?:"
    "[a-z0-9\\-._~%!$&'()*+,;=]+?:?"
    "[a-z0-9\\-._~%!$&'()*+,;=]+@"
    ")?";
static const char host[] = "(?:[a-z0-9\\-._~%!$&'()*+,;=]+)"; // www.foo.bar
static const char port[] = "(?::[0-9]+)?"; // :1234
static const char path[] = "(?:[a-zA-Z0-9\\-._~%!$&'()*+,;=:@/]+)?"; // /path/to/some/place
static const char query[] = "(?:\\?[a-z0-9\\-._~%!$&'()*+,;=:@/]+)?"; // "?somequery=bar"
static const char fragment[] = "(?:#[a-z0-9/?]+)?";

using LS1 = QLatin1String;

/* clang-format off */
const QRegularExpression UrlFilter::FullUrlRegExp(
    LS1(scheme)
    + LS1(userInfo)
    + LS1(host)
    + LS1(port)
    + LS1(path)
    + LS1(query)
    + LS1(fragment)
    );
/* clang-format on */

/////////////////////////////////////////////

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

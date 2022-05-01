/*
    SPDX-FileCopyrightText: 2022 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "HotSpotFilterTest.h"
#include <QTest>

QTEST_GUILESS_MAIN(HotSpotFilterTest)

void HotSpotFilterTest::testUrlFilterRegex_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<bool>("matchResult");

    // A space, \n, or \t before the url to match what happens at runtime,
    // i.e. to match "http" but not "foohttp"
    QTest::newRow("url_simple") << " https://api.kde.org" << true;
    QTest::newRow("url_with_port") << "\nhttps://api.kde.org:2098" << true;
    QTest::newRow("url_with_path") << "https://api.kde.org/path/to/somewhere" << true;
    QTest::newRow("url_with_query") << "https://user:pass@api.kde.org?somequery=foo" << true;
    QTest::newRow("url_with_port_path") << " https://api.kde.org:2098/path/to/somewhere" << true;
    QTest::newRow("url_with_user_password") << "\thttps://user:blah@api.kde.org" << true;
    QTest::newRow("url_with_user_password_port_fragment") << " https://user:blah@api.kde.org:2098#fragment" << true;
    QTest::newRow("url_all_bells") << " https://user:pass@api.kde.org:2098/path/to/somewhere?somequery=foo#fragment" << true;
    QTest::newRow("uppercase") << " https://invent.kde.org/frameworks/ktexteditor/-/blob/master/README.md" << true;
    QTest::newRow("markup") << " [https://foobar](https://foobar)" << true;

    QTest::newRow("bad_url_no_scheme") << QStringLiteral(" www.kde.org") << false;
}

void HotSpotFilterTest::testUrlFilterRegex()
{
    QFETCH(QString, url);
    QFETCH(bool, matchResult);

    const QRegularExpression &regex = Konsole::UrlFilter::FullUrlRegExp;
    const QRegularExpressionMatch match = regex.match(url);
    QCOMPARE(match.hasMatch(), matchResult);
    if (strcmp(QTest::currentDataTag(), "markup") == 0) {
        QCOMPARE(match.capturedView(0), u"https://foobar");
    } else if (matchResult) {
        QCOMPARE(match.capturedView(0), url.trimmed());
    }
}

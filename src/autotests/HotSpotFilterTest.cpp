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
    QTest::addColumn<QString>("expectedUrl");
    QTest::addColumn<bool>("matchResult");

    // A space, \n, or \t before the url to match what happens at runtime,
    // i.e. to match "http" but not "foohttp"
    QTest::newRow("url_simple") << " https://api.kde.org"
                                << "https://api.kde.org" << true;
    QTest::newRow("url_with_port") << "\nhttps://api.kde.org:2098"
                                   << "https://api.kde.org:2098" << true;
    QTest::newRow("url_with_port_trailing_slash") << "\nhttps://api.kde.org:2098/"
                                                  << "https://api.kde.org:2098/" << true;
    QTest::newRow("url_with_numeric_host") << "\nhttp://127.0.0.1"
                                           << "http://127.0.0.1" << true;
    QTest::newRow("url_with_numeric_host_port") << "\nhttp://127.0.0.1:4000"
                                                << "http://127.0.0.1:4000" << true;
    QTest::newRow("url_with_numeric_host_port_slash") << "\nhttp://127.0.0.1:4000/"
                                                      << "http://127.0.0.1:4000/" << true;
    QTest::newRow("url_with_path") << "https://api.kde.org/path/to/somewhere"
                                   << "https://api.kde.org/path/to/somewhere" << true;
    QTest::newRow("url_with_query") << "https://user:pass@api.kde.org?somequery=foo"
                                    << "https://user:pass@api.kde.org?somequery=foo" << true;
    QTest::newRow("url_with_port_path") << " https://api.kde.org:2098/path/to/somewhere"
                                        << "https://api.kde.org:2098/path/to/somewhere" << true;
    QTest::newRow("url_with_user_password") << "\thttps://user:blah@api.kde.org"
                                            << "https://user:blah@api.kde.org" << true;
    QTest::newRow("url_with_user_password_port_fragment") << " https://user:blah@api.kde.org:2098#fragment"
                                                          << "https://user:blah@api.kde.org:2098#fragment" << true;
    QTest::newRow("url_all_bells") << " https://user:pass@api.kde.org:2098/path/to/somewhere?somequery=foo#fragment"
                                   << "https://user:pass@api.kde.org:2098/path/to/somewhere?somequery=foo#fragment" << true;
    QTest::newRow("uppercase") << " https://invent.kde.org/frameworks/ktexteditor/-/blob/master/README.md"
                               << "https://invent.kde.org/frameworks/ktexteditor/-/blob/master/README.md" << true;
    QTest::newRow("markup") << " [https://foobar](https://foobar)"
                            << "https://foobar" << true;
    QTest::newRow("markup_parens") << "[unix-history-repo](https://github.com/dspinellis/unix-history-repo)"
                                   << "https://github.com/dspinellis/unix-history-repo" << true;
    QTest::newRow("markup_with_parens_inside_parens") << "[*Das verrückte Labyrinth*](https://en.wikipedia.org/wiki/Labyrinth_(board_game))"
                                                      << "https://en.wikipedia.org/wiki/Labyrinth_(board_game)" << true;

    QTest::newRow("bracket_before") << "[198]http://www.ietf.org/rfc/rfc2396.txt"
                                    << "http://www.ietf.org/rfc/rfc2396.txt" << true;
    QTest::newRow("quote_before") << "\"http://www.ietf.org/rfc/rfc2396.txt"
                                  << "http://www.ietf.org/rfc/rfc2396.txt" << true;

    QTest::newRow("grave_before") << "`https://foo.bar`"
                                  << "https://foo.bar" << true;

    QTest::newRow("file_scheme") << "file:///some/file"
                                 << "file:///some/file" << true;

    QTest::newRow("uppercase_host") << "https://EXAMPLE.com"
                                    << "https://EXAMPLE.com" << true;
    QTest::newRow("uppercase_query") << "https://example.com?fooOpt=barVal"
                                     << "https://example.com?fooOpt=barVal" << true;
    QTest::newRow("uppercase_fragment") << "https://example.com?fooOpt=barVal#FRAG"
                                        << "https://example.com?fooOpt=barVal#FRAG" << true;

    QTest::newRow("www") << " www.kde.org"
                         << "www.kde.org" << true;

    QTest::newRow("with_comma_in_path") << "https://example.com/foo,bar"
                                        << "https://example.com/foo,bar" << true;

    QTest::newRow("empty_query") << "http://example.com/?"
                                 << "http://example.com/?" << true;
    QTest::newRow("empty_fragment") << "http://example.com/#"
                                    << "http://example.com/#" << true;

    QTest::newRow("www_followed_by_colon") << "www.example.com:foo@bar.com"
                                           << "www.example.com" << true;

    QTest::newRow("ipv6") << "http://[2a00:1450:4001:829::200e]/"
                          << "http://[2a00:1450:4001:829::200e]/" << true;
    QTest::newRow("ipv6_with_port") << "http://[2a00:1450:4001:829::200e]:80/"
                                    << "http://[2a00:1450:4001:829::200e]:80/" << true;

    QTest::newRow("query_with_question_marks") << "ldap://[2001:db8::7]/c=GB?objectClass?one"
                                               << "ldap://[2001:db8::7]/c=GB?objectClass?one" << true;

    QTest::newRow("path_with_parens") << "https://en.wikipedia.org/wiki/C_(programming_language)"
                                      << "https://en.wikipedia.org/wiki/C_(programming_language)" << true;
    QTest::newRow("query_with_parens") << "http://en.wikipedia.org/w/index.php?title=Thresholding_(image_processing)&oldid=132306976"
                                       << "http://en.wikipedia.org/w/index.php?title=Thresholding_(image_processing)&oldid=132306976" << true;
    QTest::newRow("fragment_with_parens") << "https://en.wikipedia.org/wiki/ANSI_escape_code#CSI_(Control_Sequence_Introducer)_sequences"
                                          << "https://en.wikipedia.org/wiki/ANSI_escape_code#CSI_(Control_Sequence_Introducer)_sequences" << true;
    QTest::newRow("url_with_lots_of_parens") << "(https://example.com/foo(bar(baz(qux)quux)quuux))))"
                                             << "https://example.com/foo(bar(baz(qux)quux)quuux)" << true;
}

void HotSpotFilterTest::testUrlFilterRegex()
{
    QFETCH(QString, url);
    QFETCH(QString, expectedUrl);
    QFETCH(bool, matchResult);

    const QRegularExpression &regex = Konsole::UrlFilter::FullUrlRegExp;
    const QRegularExpressionMatch match = regex.match(url);
    // qDebug() << match;

    QCOMPARE(match.hasMatch(), matchResult);
    if (matchResult) {
        QCOMPARE(match.capturedView(0), expectedUrl);
    }
}

/*
    SPDX-FileCopyrightText: 2019 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "BookMarkTest.h"

// Qt
#include <qtest.h>

// KDE
#include <KBookmarkManager>

using namespace Konsole;

/* This does not use Konsole's BookmarkHandler directly; it is used to
 *  test the code copied from there and to test any changes.
 */

/* Test that the URL (command) does not get mangled by KBookMark's encoding */
void BookMarkTest::testBookMarkURLs_data()
{
    auto testData = QFINDTESTDATA(QStringLiteral("data/bookmarks.xml"));
    auto bookmarkManager = KBookmarkManager::managerForFile(testData, QStringLiteral("KonsoleTest"));
    auto groupUrlList = bookmarkManager->root().groupUrlList();

    // text explaining test, correct test result
    QStringList bm_urls = {QStringLiteral("simple command"),
                           QStringLiteral("ssh machine"),
                           QStringLiteral("command with pipe (|)"),
                           QStringLiteral("ssh machine | tee -a /var/log/system.log"),
                           QStringLiteral("file URL w/ non ASCII part"),
                           QStringLiteral("file:///home/user/aκόσμε"),
                           QStringLiteral("command with double quotes"),
                           QStringLiteral("isql-fb -u sysdba -p example \"test\""),
                           QStringLiteral("command with single quotes"),
                           QStringLiteral("isql-fb -u sysdba -p example 'test'"),
                           QStringLiteral("command with %"),
                           QStringLiteral("date +%m-%d-%Y")};

    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("result");
    for (int i = 0; i < groupUrlList.size(); ++i) {
        const auto &bm_url = groupUrlList.at(i);
        // Verify this line matching SessionControll.cpp to validate tests
        auto bm = QUrl::fromPercentEncoding(bm_url.toEncoded());
        QTest::newRow(bm_urls.at(i * 2).toUtf8().data()) << bm_urls.at((i * 2) + 1) << bm;
    }
}

// Only test top level URLs (no folders)
void BookMarkTest::testBookMarkURLs()
{
    QFETCH(QString, text);
    QFETCH(QString, result);

    QCOMPARE(text, result);
}

/*
 * When testing more than just URLs, the below loop can be used
    auto bm = root.first();
    int i = 0;
    while (!bm.isNull()) {
        auto bm_url = QUrl::fromPercentEncoding(bm.url().toString().toUtf8());
        qWarning()<< i << ":" <<bm.url().toString();
        i++;
        bm = root.next(bm);
    }
*/

QTEST_GUILESS_MAIN(BookMarkTest)

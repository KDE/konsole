/*
    SPDX-FileCopyrightText: 2025 Akseli Lahtinen <akselmo@akselmo.dev>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ViewManagerTest.h"
#include <QFile>
#include <QTest>

#include "../MainWindow.h"
#include "../ViewManager.h"
#include "../ViewProperties.h"
#include "../widgets/ViewContainer.h"
#include <QStandardPaths>

using namespace Konsole;

void ViewManagerTest::initTestCase()
{
    m_testDir = new QTemporaryDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/konsoleviewmanagertest"));
}

void ViewManagerTest::testSaveLayout()
{
    // Single tab:
    // - Horizontally split view, with one view that is vertically split
    // The numeric values mean the view number, which is not relevant for this test, since we create new views
    QStringList expectedHierarchy = {QStringLiteral("(0)[0|(1){1|2}]")};

    auto mw = MainWindow();
    mw.viewManager()->newSession(mw.viewManager()->defaultProfile(), m_testDir->path());
    mw.viewManager()->splitLeftRight();
    mw.viewManager()->splitTopBottom();

    // Only one view has m_testDir->path(), rest have empty path.
    // Verify that at least one of them has the m_testDir->path().
    QList<QUrl> urlList = {};
    for (const auto &v : mw.viewManager()->viewProperties()) {
        urlList.append(v->url());
    }
    QVERIFY(urlList.contains(QUrl::fromLocalFile(m_testDir->path())));

    mw.viewManager()->saveLayout(m_testDir->filePath(QStringLiteral("test.json")));
    QCOMPARE(mw.viewManager()->viewHierarchy(), expectedHierarchy);

    QFile layoutFile(m_testDir->filePath(QStringLiteral("test.json")));
    QVERIFY(layoutFile.exists());
}

void ViewManagerTest::testLoadLayout()
{
    // Two tabs:
    // - First tab: Has only single view. We expect the layout to be opened in new tab.
    // - Second tab: Horizontally split view, with one view that is vertically split
    // The numeric values mean the view number, which is not relevant for this test, since we create new views
    QStringList expectedHierarchy = {QStringLiteral("(2)[3]"), QStringLiteral("(3)[4|(4){5|6}]")};

    auto mw = MainWindow();
    mw.viewManager()->newSession(mw.viewManager()->defaultProfile(), m_testDir->path());

    QFile layoutFile(m_testDir->filePath(QStringLiteral("test.json")));
    QVERIFY(layoutFile.exists());

    mw.viewManager()->loadLayout(m_testDir->filePath(QStringLiteral("test.json")));
    QCOMPARE(mw.viewManager()->viewHierarchy(), expectedHierarchy);

    // Only one view has m_testDir->path(), rest have empty path.
    // Verify that at least one of them has the m_testDir->path().
    QList<QUrl> urlList = {};
    for (const auto &v : mw.viewManager()->viewProperties()) {
        urlList.append(v->url());
    }
    QVERIFY(urlList.contains(QUrl::fromLocalFile(m_testDir->path())));
}

QTEST_MAIN(ViewManagerTest)

#include "moc_ViewManagerTest.cpp"

/*
    SPDX-FileCopyrightText: 2025 Akseli Lahtinen <akselmo@akselmo.dev>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ViewManagerTest.h"
#include <QFile>
#include <QTest>

#include "../MainWindow.h"
#include "../ViewManager.h"
#include "../containers/ContainerSessionState.h"
#include "../containers/IContainerDetector.h"
#include "../session/Session.h"
#include "../session/SessionController.h"
#include "../widgets/ViewContainer.h"
#include "../KonsoleSettings.h"
#include <QSignalSpy>
#include <QStandardPaths>

using namespace Konsole;

namespace
{
class TestContainerDetector : public IContainerDetector
{
public:
    using IContainerDetector::IContainerDetector;

    QString typeId() const override
    {
        return QStringLiteral("distrobox");
    }

    QString displayName() const override
    {
        return QStringLiteral("Distrobox");
    }

    QString iconName() const override
    {
        return QStringLiteral("distrobox");
    }

    std::optional<ContainerInfo> detect(int) const override
    {
        return std::nullopt;
    }

    QStringList entryCommand(const QString &containerName) const override
    {
        return {QStringLiteral("distrobox"), QStringLiteral("enter"), containerName};
    }

    void startListContainers() override
    {
        Q_EMIT listContainersFinished({});
    }
};
}

void ViewManagerTest::initTestCase()
{
    m_testDir = new QTemporaryDir(QDir::tempPath() + QDir::separator() + QStringLiteral("konsoleviewmanagertest-XXXXXX"));
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
}

void ViewManagerTest::testContainerMenuLaunchKeepsPendingColor()
{
    auto mw = MainWindow();

    TestContainerDetector detector;
    ContainerInfo container;
    container.detector = &detector;
    container.name = QStringLiteral("codex");
    container.displayName = QStringLiteral("codex");
    container.iconName = QStringLiteral("distrobox");

    const bool invoked = QMetaObject::invokeMethod(&mw, "newInContainer", Qt::DirectConnection, Q_ARG(Konsole::ContainerInfo, container));
    QVERIFY(invoked);

    auto *controller = mw.viewManager()->activeViewController();
    QVERIFY(controller != nullptr);
    Session *session = controller->session();
    QVERIFY(session != nullptr);

    const QString key = QStringLiteral("%1:%2").arg(detector.typeId(), container.name);
    QCOMPARE(session->property(ContainerSessionState::PendingContainerKeyProperty).toString(), key);
    QCOMPARE(session->color(), ContainerSessionState::colorForContainerKey(key));

    // Simulate transient host-side process state before in-container shell is confirmed.
    session->setContainerContext(ContainerInfo{});
    QCOMPARE(session->property(ContainerSessionState::PendingContainerKeyProperty).toString(), key);
    QCOMPARE(session->color(), ContainerSessionState::colorForContainerKey(key));

    // Once container detection confirms context, pending state is cleared.
    session->setContainerContext(container);
    QCOMPARE(session->property(ContainerSessionState::PendingContainerKeyProperty).toString(), QString());
    QCOMPARE(session->color(), ContainerSessionState::colorForContainerKey(key));
}

void ViewManagerTest::testContainerTabColorSettingHidesAutoColor()
{
    auto mw = MainWindow();

    TestContainerDetector detector;
    ContainerInfo container;
    container.detector = &detector;
    container.name = QStringLiteral("fedora-41");
    container.displayName = QStringLiteral("fedora-41");
    container.iconName = QStringLiteral("distrobox");

    const bool invoked = QMetaObject::invokeMethod(&mw, "newInContainer", Qt::DirectConnection, Q_ARG(Konsole::ContainerInfo, container));
    QVERIFY(invoked);

    Session *session = mw.viewManager()->activeViewController()->session();
    auto *viewContainer = mw.viewManager()->activeContainer();

    QVERIFY(!session->isTabColorSetByUser());
    QVERIFY(session->color().isValid());

    QSignalSpy colorSpy(viewContainer, &TabbedViewContainer::setColor);

    // Setting ON: config refresh must emit the container color
    KonsoleSettings::setShowContainerTabColor(true);
    Q_EMIT KonsoleSettings::self()->configChanged();
    QVERIFY(!colorSpy.isEmpty());
    QVERIFY(colorSpy.last().at(1).value<QColor>().isValid());

    colorSpy.clear();

    // Setting OFF: config refresh must suppress the auto color
    KonsoleSettings::setShowContainerTabColor(false);
    Q_EMIT KonsoleSettings::self()->configChanged();
    QVERIFY(!colorSpy.isEmpty());
    QVERIFY(!colorSpy.last().at(1).value<QColor>().isValid());

    KonsoleSettings::setShowContainerTabColor(true);
}

void ViewManagerTest::testContainerTabColorSettingPreservesUserColor()
{
    auto mw = MainWindow();
    mw.viewManager()->newSession(mw.viewManager()->defaultProfile(), m_testDir->path());

    Session *session = mw.viewManager()->activeViewController()->session();
    auto *viewContainer = mw.viewManager()->activeContainer();

    session->setColor(QColor(Qt::red));
    session->tabColorSetByUser(true);

    QSignalSpy colorSpy(viewContainer, &TabbedViewContainer::setColor);

    // Setting OFF must NOT suppress a user-explicitly-set color
    KonsoleSettings::setShowContainerTabColor(false);
    Q_EMIT KonsoleSettings::self()->configChanged();
    QVERIFY(!colorSpy.isEmpty());
    QCOMPARE(colorSpy.last().at(1).value<QColor>(), QColor(Qt::red));

    KonsoleSettings::setShowContainerTabColor(true);
    session->tabColorSetByUser(false);
}

QTEST_MAIN(ViewManagerTest)

#include "moc_ViewManagerTest.cpp"

/*
    SPDX-FileCopyrightText: 2025 Akseli Lahtinen <akselmo@akselmo.dev>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ViewManagerTest.h"
#include <QFile>
#include <QTest>

#include "../KonsoleSettings.h"
#include "../MainWindow.h"
#include "../ViewManager.h"
#include "../containers/ContainerSessionState.h"
#include "../containers/IContainerDetector.h"
#include "../profile/Profile.h"
#include "../profile/ProfileManager.h"
#include "../session/Session.h"
#include "../session/SessionController.h"
#include "../widgets/ViewContainer.h"
#include <QSignalSpy>
#include <QStandardPaths>

using namespace Konsole;

namespace
{
class ProfilePropertyGuard
{
public:
    ProfilePropertyGuard(Profile::Ptr profile, Profile::Property property)
        : m_profile(std::move(profile))
        , m_property(property)
        , m_value(m_profile->property<QVariant>(property))
    {
    }

    ~ProfilePropertyGuard()
    {
        m_profile->setProperty(m_property, m_value);
    }

private:
    Profile::Ptr m_profile;
    Profile::Property m_property;
    QVariant m_value;
};

class TestContainerDetector : public IContainerDetector
{
public:
    explicit TestContainerDetector(QString type = QStringLiteral("distrobox"), QObject *parent = nullptr)
        : IContainerDetector(parent)
        , m_type(std::move(type))
    {
    }

    QString typeId() const override
    {
        return m_type;
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

private:
    QString m_type;
};

ContainerInfo containerInfo(const IContainerDetector *detector, const QString &name)
{
    return ContainerInfo{.detector = detector, .name = name, .displayName = name, .iconName = detector->iconName(), .hostPid = std::nullopt};
}

QString containerKey(const ContainerInfo &container)
{
    return container.isValid() ? container.detector->typeId() + QLatin1Char(':') + container.name : QString();
}
}

void ViewManagerTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    m_testDir = std::make_unique<QTemporaryDir>(QDir::tempPath() + QDir::separator() + QStringLiteral("konsoleviewmanagertest-XXXXXX"));
    QVERIFY(m_testDir->isValid());
}

void ViewManagerTest::cleanup()
{
    KonsoleSettings::setShowContainerTabColor(true);
    KonsoleSettings::setShowContainerStatusBar(true);
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

void ViewManagerTest::testContainerContextSelection_data()
{
    QTest::addColumn<bool>("inheritContext");
    QTest::addColumn<QString>("activeContainerKey");
    QTest::addColumn<QString>("profileContainerKey");
    QTest::addColumn<QString>("expectedContainerKey");

    QTest::newRow("host-default") << false << QString() << QString() << QString();
    QTest::newRow("profile-container") << false << QStringLiteral("toolbox:active") << QStringLiteral("distrobox:profile")
                                       << QStringLiteral("distrobox:profile");
    QTest::newRow("inherit-host-falls-back-to-profile") << true << QString() << QStringLiteral("distrobox:profile") << QStringLiteral("distrobox:profile");
    QTest::newRow("inherit-active") << true << QStringLiteral("toolbox:active") << QString() << QStringLiteral("toolbox:active");
    QTest::newRow("inherit-wins-over-profile") << true << QStringLiteral("toolbox:active") << QStringLiteral("distrobox:profile")
                                               << QStringLiteral("toolbox:active");
}

void ViewManagerTest::testContainerContextSelection()
{
    QFETCH(bool, inheritContext);
    QFETCH(QString, activeContainerKey);
    QFETCH(QString, profileContainerKey);
    QFETCH(QString, expectedContainerKey);

    TestContainerDetector toolboxDetector(QStringLiteral("toolbox"));
    MainWindow mw;
    auto *manager = mw.viewManager();

    if (!activeContainerKey.isEmpty()) {
        Profile::Ptr activeProfile(new Profile);
        Session *activeSession = manager->createSession(activeProfile);
        activeSession->setContainerContext(containerInfo(&toolboxDetector, activeContainerKey.section(QLatin1Char(':'), 1)));
        manager->activeContainer()->addView(manager->createView(activeSession));
    }

    Profile::Ptr profile(new Profile);
    profile->setProperty(Profile::InheritContainerContext, inheritContext);
    profile->setProperty(Profile::ContainerName, profileContainerKey);

    Session *session = manager->createSession(profile);
    QCOMPARE(containerKey(session->containerContext()), expectedContainerKey);
}

void ViewManagerTest::testExplicitContainerOverridesAutomaticContext()
{
    TestContainerDetector toolboxDetector(QStringLiteral("toolbox"));
    TestContainerDetector distroboxDetector(QStringLiteral("distrobox"));
    MainWindow mw;
    auto *manager = mw.viewManager();
    const Profile::Ptr defaultProfile = ProfileManager::instance()->defaultProfile();
    const ProfilePropertyGuard inheritGuard(defaultProfile, Profile::InheritContainerContext);
    const ProfilePropertyGuard containerGuard(defaultProfile, Profile::ContainerName);

    defaultProfile->setProperty(Profile::InheritContainerContext, true);
    defaultProfile->setProperty(Profile::ContainerName, QStringLiteral("distrobox:profile"));
    Session *activeSession = manager->createSession(defaultProfile);
    activeSession->setContainerContext(containerInfo(&toolboxDetector, QStringLiteral("active")));
    manager->activeContainer()->addView(manager->createView(activeSession));

    const ContainerInfo explicitContainer = containerInfo(&distroboxDetector, QStringLiteral("explicit"));
    const QList<Session *> previousSessionList = manager->sessions();
    const QSet<Session *> previousSessions(previousSessionList.cbegin(), previousSessionList.cend());
    QVERIFY(QMetaObject::invokeMethod(&mw, "newInContainer", Qt::DirectConnection, Q_ARG(Konsole::ContainerInfo, explicitContainer)));
    const QList<Session *> newSessionList = manager->sessions();
    const QSet<Session *> newSessions(newSessionList.cbegin(), newSessionList.cend());
    const QSet<Session *> addedSessions = newSessions - previousSessions;
    QCOMPARE(addedSessions.size(), 1);
    QCOMPARE(containerKey((*addedSessions.cbegin())->containerContext()), QStringLiteral("distrobox:explicit"));
}

void ViewManagerTest::testExplicitHostOverridesAutomaticContext()
{
    TestContainerDetector toolboxDetector(QStringLiteral("toolbox"));
    MainWindow mw;
    auto *manager = mw.viewManager();
    const Profile::Ptr defaultProfile = ProfileManager::instance()->defaultProfile();
    const ProfilePropertyGuard inheritGuard(defaultProfile, Profile::InheritContainerContext);
    const ProfilePropertyGuard containerGuard(defaultProfile, Profile::ContainerName);

    defaultProfile->setProperty(Profile::InheritContainerContext, true);
    defaultProfile->setProperty(Profile::ContainerName, QStringLiteral("distrobox:profile"));
    Session *activeSession = manager->createSession(defaultProfile);
    activeSession->setContainerContext(containerInfo(&toolboxDetector, QStringLiteral("active")));
    manager->activeContainer()->addView(manager->createView(activeSession));

    const QList<Session *> previousSessionList = manager->sessions();
    const QSet<Session *> previousSessions(previousSessionList.cbegin(), previousSessionList.cend());
    QVERIFY(QMetaObject::invokeMethod(&mw, "newInContainer", Qt::DirectConnection, Q_ARG(Konsole::ContainerInfo, ContainerInfo{})));
    const QList<Session *> newSessionList = manager->sessions();
    const QSet<Session *> newSessions(newSessionList.cbegin(), newSessionList.cend());
    const QSet<Session *> addedSessions = newSessions - previousSessions;
    QCOMPARE(addedSessions.size(), 1);
    QVERIFY(!(*addedSessions.cbegin())->containerContext().isValid());
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

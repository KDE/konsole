/*
    SPDX-FileCopyrightText: 2026 Konsole Developers

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "../containers/ContainerRegistry.h"

#include <QSignalSpy>
#include <QTest>

using namespace Konsole;

namespace
{
class TestContainerDetector : public IContainerDetector
{
public:
    explicit TestContainerDetector(QString type, QObject *parent = nullptr)
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
        return m_type;
    }

    QString iconName() const override
    {
        return m_type + QStringLiteral("-icon");
    }

    std::optional<ContainerInfo> detect(int pid) const override
    {
        if (pid == m_detectedPid) {
            return info(m_detectedName);
        }
        return std::nullopt;
    }

    QStringList entryCommand(const QString &containerName) const override
    {
        return {m_type, QStringLiteral("enter"), containerName};
    }

    void startListContainers() override
    {
        ++startCount;
    }

    ContainerInfo info(const QString &name) const
    {
        return ContainerInfo{.detector = this, .name = name, .displayName = name, .iconName = iconName(), .hostPid = std::nullopt};
    }

    void complete(const QStringList &names = {})
    {
        QList<ContainerInfo> containers;
        for (const QString &name : names) {
            containers.append(info(name));
        }
        Q_EMIT listContainersFinished(containers);
    }

    void setDetection(int pid, const QString &name)
    {
        m_detectedPid = pid;
        m_detectedName = name;
    }

    void notifyContainersChanged()
    {
        Q_EMIT containersChanged();
    }

    int startCount = 0;

private:
    QString m_type;
    int m_detectedPid = -1;
    QString m_detectedName;
};

struct RegistryFixture {
    QList<TestContainerDetector *> detectors;
    std::unique_ptr<ContainerRegistry> registry;
};
}

namespace Konsole
{
class ContainerRegistryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void keyRoundTrip_data();
    void keyRoundTrip();
    void invalidKey_data();
    void invalidKey();
    void osc777Parsing();
    void detectionUsesRegistrationOrder();
    void delegatesEntryCommand();
    void refreshWaitsForAllDetectors();
    void refreshAcceptsEmptyResults();
    void overlappingRefreshIsIgnored();
    void detectorChangeStartsRefresh();

private:
    static RegistryFixture makeFixture(const QStringList &types);
};

RegistryFixture ContainerRegistryTest::makeFixture(const QStringList &types)
{
    RegistryFixture fixture;
    std::vector<std::unique_ptr<IContainerDetector>> ownedDetectors;
    for (const QString &type : types) {
        auto detector = std::make_unique<TestContainerDetector>(type);
        fixture.detectors.append(detector.get());
        ownedDetectors.push_back(std::move(detector));
    }
    fixture.registry = std::unique_ptr<ContainerRegistry>(new ContainerRegistry(std::move(ownedDetectors)));
    return fixture;
}

void ContainerRegistryTest::keyRoundTrip_data()
{
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("name");

    QTest::newRow("toolbox") << QStringLiteral("toolbox") << QStringLiteral("fedora-41");
    QTest::newRow("distrobox") << QStringLiteral("distrobox") << QStringLiteral("ubuntu:24.04");
    QTest::newRow("kapsule-default") << QStringLiteral("kapsule") << QString();
}

void ContainerRegistryTest::keyRoundTrip()
{
    QFETCH(QString, type);
    QFETCH(QString, name);

    RegistryFixture fixture = makeFixture({type});
    const ContainerInfo original = fixture.detectors.constFirst()->info(name);
    const QString key = ContainerRegistry::keyFromContainerInfo(original);
    const ContainerInfo restored = fixture.registry->containerInfoFromKey(key);

    QCOMPARE(key, type + QLatin1Char(':') + name);
    QVERIFY(restored.isValid());
    QCOMPARE(restored.detector, fixture.detectors.constFirst());
    QCOMPARE(restored.name, name);
}

void ContainerRegistryTest::invalidKey_data()
{
    QTest::addColumn<QString>("key");

    QTest::newRow("empty") << QString();
    QTest::newRow("missing-separator") << QStringLiteral("kapsule");
    QTest::newRow("missing-type") << QStringLiteral(":container");
    QTest::newRow("unknown-type") << QStringLiteral("unknown:container");
}

void ContainerRegistryTest::invalidKey()
{
    QFETCH(QString, key);
    RegistryFixture fixture = makeFixture({QStringLiteral("kapsule")});
    QVERIFY(!fixture.registry->containerInfoFromKey(key).isValid());
}

void ContainerRegistryTest::osc777Parsing()
{
    RegistryFixture fixture = makeFixture({QStringLiteral("toolbox"), QStringLiteral("kapsule")});

    const auto push =
        fixture.registry->containerInfoFromOsc777({QStringLiteral("container"), QStringLiteral("push"), QStringLiteral("dev"), QStringLiteral("kapsule")});
    QVERIFY(push.has_value());
    QVERIFY(push->isValid());
    QCOMPARE(push->detector, fixture.detectors.at(1));
    QCOMPARE(push->name, QStringLiteral("dev"));

    const auto pop = fixture.registry->containerInfoFromOsc777({QStringLiteral("container"), QStringLiteral("pop"), QString(), QString()});
    QVERIFY(pop.has_value());
    QVERIFY(!pop->isValid());

    QVERIFY(!fixture.registry->containerInfoFromOsc777({QStringLiteral("notify"), QStringLiteral("title"), QStringLiteral("body")}).has_value());
    QVERIFY(!fixture.registry->containerInfoFromOsc777({QStringLiteral("container"), QStringLiteral("push"), QStringLiteral("dev"), QStringLiteral("unknown")})
                 .has_value());
    QVERIFY(!fixture.registry->containerInfoFromOsc777({QStringLiteral("container"), QStringLiteral("push")}).has_value());
}

void ContainerRegistryTest::detectionUsesRegistrationOrder()
{
    RegistryFixture fixture = makeFixture({QStringLiteral("first"), QStringLiteral("second")});
    fixture.detectors.at(0)->setDetection(42, QStringLiteral("one"));
    fixture.detectors.at(1)->setDetection(42, QStringLiteral("two"));

    const ContainerInfo detected = fixture.registry->detectContainer(42);
    QCOMPARE(detected.detector, fixture.detectors.at(0));
    QCOMPARE(detected.name, QStringLiteral("one"));
    QVERIFY(!fixture.registry->detectContainer(0).isValid());
}

void ContainerRegistryTest::delegatesEntryCommand()
{
    RegistryFixture fixture = makeFixture({QStringLiteral("kapsule")});
    const ContainerInfo container = fixture.detectors.constFirst()->info(QString());
    QCOMPARE(fixture.registry->entryCommand(container), QStringList({QStringLiteral("kapsule"), QStringLiteral("enter"), QString()}));
    QVERIFY(fixture.registry->entryCommand({}).isEmpty());
}

void ContainerRegistryTest::refreshWaitsForAllDetectors()
{
    RegistryFixture fixture = makeFixture({QStringLiteral("first"), QStringLiteral("second")});
    QSignalSpy updatedSpy(fixture.registry.get(), &ContainerRegistry::containersUpdated);

    fixture.registry->refreshContainers();
    QCOMPARE(fixture.detectors.at(0)->startCount, 1);
    QCOMPARE(fixture.detectors.at(1)->startCount, 1);

    fixture.detectors.at(1)->complete({QStringLiteral("two")});
    QCOMPARE(updatedSpy.count(), 0);
    QVERIFY(fixture.registry->cachedContainers().isEmpty());

    fixture.detectors.at(0)->complete({QStringLiteral("one")});
    QCOMPARE(updatedSpy.count(), 1);
    QCOMPARE(fixture.registry->cachedContainers().size(), 2);
    QCOMPARE(fixture.registry->cachedContainers().at(0).name, QStringLiteral("two"));
    QCOMPARE(fixture.registry->cachedContainers().at(1).name, QStringLiteral("one"));
}

void ContainerRegistryTest::refreshAcceptsEmptyResults()
{
    RegistryFixture fixture = makeFixture({QStringLiteral("first"), QStringLiteral("second")});
    QSignalSpy updatedSpy(fixture.registry.get(), &ContainerRegistry::containersUpdated);

    fixture.registry->refreshContainers();
    fixture.detectors.at(0)->complete();
    fixture.detectors.at(1)->complete({QStringLiteral("available")});

    QCOMPARE(updatedSpy.count(), 1);
    QCOMPARE(fixture.registry->cachedContainers().size(), 1);
    QCOMPARE(fixture.registry->cachedContainers().constFirst().name, QStringLiteral("available"));
}

void ContainerRegistryTest::overlappingRefreshIsIgnored()
{
    RegistryFixture fixture = makeFixture({QStringLiteral("kapsule")});

    fixture.registry->refreshContainers();
    fixture.registry->refreshContainers();
    QCOMPARE(fixture.detectors.constFirst()->startCount, 1);

    fixture.detectors.constFirst()->complete();
    fixture.registry->refreshContainers();
    QCOMPARE(fixture.detectors.constFirst()->startCount, 2);
}

void ContainerRegistryTest::detectorChangeStartsRefresh()
{
    RegistryFixture fixture = makeFixture({QStringLiteral("kapsule")});

    fixture.detectors.constFirst()->notifyContainersChanged();
    QCOMPARE(fixture.detectors.constFirst()->startCount, 1);
}

}

QTEST_GUILESS_MAIN(Konsole::ContainerRegistryTest)

#include "ContainerRegistryTest.moc"

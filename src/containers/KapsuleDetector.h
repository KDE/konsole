/*
    SPDX-FileCopyrightText: 2026 Konsole Developers

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KAPSULEDETECTOR_H
#define KAPSULEDETECTOR_H

#include "IContainerDetector.h"

#include <QCoro/QCoroTask>

namespace Kapsule
{
class KapsuleClient;
}

namespace Konsole
{
class KapsuleDetector : public IContainerDetector
{
    Q_OBJECT
public:
    explicit KapsuleDetector(QObject *parent = nullptr);

    QString typeId() const override;
    QString displayName() const override;
    QString iconName() const override;
    std::optional<ContainerInfo> detect(int pid) const override;
    QStringList entryCommand(const QString &containerName) const override;
    void startListContainers() override;

private:
    QCoro::Task<void> fetchContainerList();
    ContainerInfo buildContainerInfo(const QString &name, const QString &displayName, const QString &icon = {}) const;
    Kapsule::KapsuleClient *_client = nullptr;
};
} // namespace Konsole

#endif // KAPSULEDETECTOR_H

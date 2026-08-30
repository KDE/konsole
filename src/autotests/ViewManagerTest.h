/*
    SPDX-FileCopyrightText: 2025 Akseli Lahtinen <akselmo@akselmo.dev>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VIEWMANAGERTEST_H
#define VIEWMANAGERTEST_H

#include <QObject>
#include <QTemporaryDir>

#include <memory>

namespace Konsole
{
class ViewManagerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void testSaveLayout();
    void testLoadLayout();
    void testContainerContextSelection_data();
    void testContainerContextSelection();
    void testExplicitContainerOverridesAutomaticContext();
    void testExplicitHostOverridesAutomaticContext();
    void testContainerMenuLaunchKeepsPendingColor();
    void testContainerTabColorSettingHidesAutoColor();
    void testContainerTabColorSettingPreservesUserColor();

private:
    std::unique_ptr<QTemporaryDir> m_testDir;
};

}

#endif // VIEWMANAGERTEST_H

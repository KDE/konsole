/*
    SPDX-FileCopyrightText: 2025 Akseli Lahtinen <akselmo@akselmo.dev>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VIEWMANAGERTEST_H
#define VIEWMANAGERTEST_H

#include <QObject>
#include <QTemporaryDir>

namespace Konsole
{
class ViewManagerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void testSaveLayout();
    void testLoadLayout();
    void testContainerMenuLaunchKeepsPendingColor();

private:
    QTemporaryDir *m_testDir;
};

}

#endif // VIEWMANAGERTEST_H

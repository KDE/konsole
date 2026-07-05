/*
    SPDX-FileCopyrightText: 2026 Zhuxuan Huang <raycppwizard@outlook.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TERMINALDISPLAYACCESSIBLETEST_H
#define TERMINALDISPLAYACCESSIBLETEST_H

#include <QObject>

namespace Konsole {

class TerminalDisplayAccessibleTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    // Basic API Tests (Factory, Role, State)
    void testFactoryAndBasicProperties();

    // Regression Tests
    void testTextExtractionOutOfBounds();
    void testBlockSelectionBypass();
};

} // namespace Konsole

#endif // TERMINALDISPLAYACCESSIBLETEST_H

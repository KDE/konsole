/*
    SPDX-FileCopyrightText: 2019 Tomaz Canabrava <tomaz.canabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

namespace Konsole
{
class CharacterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testCanBeGrouped();
};

}

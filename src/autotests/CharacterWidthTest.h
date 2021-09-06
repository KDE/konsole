/*
    SPDX-FileCopyrightText: 2014 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CHARACTERWIDTHTEST_H
#define CHARACTERWIDTHTEST_H

#include <QObject>

namespace Konsole
{
class CharacterWidthTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void testWidth_data();
    void testWidth();
};

}

#endif // CHARACTERWIDTHTEST_H

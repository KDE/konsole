/*
    SPDX-FileCopyrightText: 2018 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VT102EMULATIONTEST_H
#define VT102EMULATIONTEST_H

#include <QObject>

namespace Konsole
{

class Vt102EmulationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testTokenFunctions();

private:
};

}

#endif // VT102EMULATIONTEST_H


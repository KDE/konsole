/*
    SPDX-FileCopyrightText: 2018 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VT102EMULATIONTEST_H
#define VT102EMULATIONTEST_H

#include <QObject>

#include "Vt102Emulation.h"

namespace Konsole
{
class TestEmulation;

class Vt102EmulationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testTokenFunctions();

    void testParse();

private:
    static void sendAndCompare(TestEmulation *em, const char *input, size_t inputLen, const QString &expectedPrint, const QByteArray &expectedSent);
};

struct TestEmulation : public Vt102Emulation {
    Q_OBJECT
    // Give us access to protected functions
    friend class Vt102EmulationTest;

    QByteArray lastSent;

public:
    void sendString(const QByteArray &string) override
    {
        lastSent = string;
        Vt102Emulation::sendString(string);
    }
};

}

#endif // VT102EMULATIONTEST_H

/*
    SPDX-FileCopyrightText: 2018 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VT102EMULATIONTEST_H
#define VT102EMULATIONTEST_H

#include <variant>
#include <vector>

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
    void testTokenizing_data();
    void testTokenizing();

    void testTokenizingVT52_data();
    void testTokenizingVT52();

private:
    static void sendAndCompare(TestEmulation *em, const char *input, size_t inputLen, const QString &expectedPrint, const QByteArray &expectedSent);
};

class TestEmulation : public Vt102Emulation
{
    Q_OBJECT
    // Give us access to protected functions
    friend class Vt102EmulationTest;

    QByteArray lastSent;

public:
    struct ProcessToken {
        int code;
        int p;
        int q;

        bool operator==(const ProcessToken &rhs) const
        {
            return code == rhs.code && p == rhs.p && q == rhs.q;
        }
    };
    struct ProcessSessionAttributeRequest {
        std::vector<uint> chars;

        bool operator==(const ProcessSessionAttributeRequest &rhs) const
        {
            return chars == rhs.chars;
        }
    };
    struct ProcessChecksumRequest {
        std::vector<int> args;

        bool operator==(const ProcessChecksumRequest &rhs) const
        {
            return args == rhs.args;
        }
    };
    struct DecodingError {
        bool operator==(const DecodingError &) const
        {
            return true;
        }
    };
    using Item = std::variant<ProcessToken, ProcessSessionAttributeRequest, ProcessChecksumRequest, DecodingError>;

private:
    std::vector<Item> items;

    bool blockFurtherProcessing = false;

public:
    void receiveChars(const QVector<uint> &c) override
    {
        Vt102Emulation::receiveChars(c);
    }

public:
    void sendString(const QByteArray &string) override
    {
        lastSent = string;
        Vt102Emulation::sendString(string);
    }

    void reportDecodingError(int /*token*/) override
    {
        items.push_back(DecodingError{});
    }

    void processToken(int code, int p, int q) override
    {
        items.push_back(ProcessToken{code, p, q});
        if (!blockFurtherProcessing) {
            Vt102Emulation::processToken(code, p, q);
        }
    }

    void processSessionAttributeRequest(const int tokenSize, const uint terminator) override
    {
        items.push_back(ProcessSessionAttributeRequest{std::vector<uint>(tokenBuffer, tokenBuffer + tokenSize)});
        if (!blockFurtherProcessing) {
            Vt102Emulation::processSessionAttributeRequest(tokenSize, terminator);
        }
    }

    void processChecksumRequest(int argc, int argv[]) override
    {
        ProcessChecksumRequest item;
        for (int i = 0; i < argc; i++) {
            item.args.push_back(argv[i]);
        }
        items.push_back(item);
        if (!blockFurtherProcessing) {
            Vt102Emulation::processChecksumRequest(argc, argv);
        }
    }
};

}

#endif // VT102EMULATIONTEST_H

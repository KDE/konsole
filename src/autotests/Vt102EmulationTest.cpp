/*
    SPDX-FileCopyrightText: 2018 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "Vt102EmulationTest.h"

#include "qtest.h"

// The below is to verify the old #defines match the new constexprs
// Just copy/paste for now from Vt102Emulation.cpp
/* clang-format off */
#define TY_CONSTRUCT(T, A, N) (((((int)(N)) & 0xffff) << 16) | ((((int)(A)) & 0xff) << 8) | (((int)(T)) & 0xff))
#define TY_CHR()        TY_CONSTRUCT(0, 0, 0)
#define TY_CTL(A)       TY_CONSTRUCT(1, A, 0)
#define TY_ESC(A)       TY_CONSTRUCT(2, A, 0)
#define TY_ESC_CS(A, B) TY_CONSTRUCT(3, A, B)
#define TY_ESC_DE(A)    TY_CONSTRUCT(4, A, 0)
#define TY_CSI_PS(A, N) TY_CONSTRUCT(5, A, N)
#define TY_CSI_PN(A)    TY_CONSTRUCT(6, A, 0)
#define TY_CSI_PR(A, N) TY_CONSTRUCT(7, A, N)
#define TY_VT52(A)      TY_CONSTRUCT(8, A, 0)
#define TY_CSI_PG(A)    TY_CONSTRUCT(9, A, 0)
#define TY_CSI_PE(A)    TY_CONSTRUCT(10, A, 0)
/* clang-format on */

using namespace Konsole;

constexpr int token_construct(int t, int a, int n)
{
    return (((n & 0xffff) << 16) | ((a & 0xff) << 8) | (t & 0xff));
}
constexpr int token_chr()
{
    return token_construct(0, 0, 0);
}
constexpr int token_ctl(int a)
{
    return token_construct(1, a, 0);
}
constexpr int token_esc(int a)
{
    return token_construct(2, a, 0);
}
constexpr int token_esc_cs(int a, int b)
{
    return token_construct(3, a, b);
}
constexpr int token_esc_de(int a)
{
    return token_construct(4, a, 0);
}
constexpr int token_csi_ps(int a, int n)
{
    return token_construct(5, a, n);
}
constexpr int token_csi_pn(int a)
{
    return token_construct(6, a, 0);
}
constexpr int token_csi_pr(int a, int n)
{
    return token_construct(7, a, n);
}
constexpr int token_vt52(int a)
{
    return token_construct(8, a, 0);
}
constexpr int token_csi_pg(int a)
{
    return token_construct(9, a, 0);
}
constexpr int token_csi_pe(int a)
{
    return token_construct(10, a, 0);
}
constexpr int token_csi_sp(int a)
{
    return token_construct(11, a, 0);
}
constexpr int token_csi_psp(int a, int n)
{
    return token_construct(12, a, n);
}
constexpr int token_csi_pq(int a)
{
    return token_construct(13, a, 0);
}

void Vt102EmulationTest::sendAndCompare(TestEmulation *em, const char *input, size_t inputLen, const QString &expectedPrint, const QByteArray &expectedSent)
{
    em->_currentScreen->clearEntireScreen();

    em->receiveData(input, inputLen);
    QString printed = em->_currentScreen->text(0, em->_currentScreen->getColumns(), Screen::PlainText);
    printed.chop(2); // Remove trailing space and newline
    QCOMPARE(printed, expectedPrint);
    QCOMPARE(em->lastSent, expectedSent);
}

void Vt102EmulationTest::testParse()
{
    TestEmulation em;
    em.reset();
    em.setCodec(TestEmulation::Utf8Codec);
    Q_ASSERT(em._currentScreen != nullptr);

    sendAndCompare(&em, "a", 1, QStringLiteral("a"), "");

    const char tertiaryDeviceAttributes[] = {0x1b, '[', '=', '0', 'c'};
    sendAndCompare(&em, tertiaryDeviceAttributes, sizeof tertiaryDeviceAttributes, QStringLiteral(""), "\033P!|7E4B4445\033\\");
}

Q_DECLARE_METATYPE(std::vector<TestEmulation::Item>)

struct ItemToString {
    QString operator()(const TestEmulation::ProcessToken &item)
    {
        return QStringLiteral("processToken(0x%0, %1, %2)").arg(QString::number(item.code, 16), QString::number(item.p), QString::number(item.q));
    }

    QString operator()(TestEmulation::ProcessSessionAttributeRequest)
    {
        return QStringLiteral("ProcessSessionAttributeRequest");
    }

    QString operator()(TestEmulation::ProcessChecksumRequest)
    {
        return QStringLiteral("ProcessChecksumRequest");
    }

    QString operator()(TestEmulation::DecodingError)
    {
        return QStringLiteral("DecodingError");
    }
};

namespace QTest
{
template<>
char *toString(const std::vector<TestEmulation::Item> &items)
{
    QStringList res;
    for (auto item : items) {
        res.append(std::visit(ItemToString{}, item));
    }
    return toString(res.join(QStringLiteral(",")));
}
}

void Vt102EmulationTest::testTokenizing_data()
{
    QTest::addColumn<QVector<uint>>("input");
    QTest::addColumn<std::vector<TestEmulation::Item>>("expectedItems");

    using ProcessToken = TestEmulation::ProcessToken;

    using C = QVector<uint>;
    using I = std::vector<TestEmulation::Item>;

    /* clang-format off */
    QTest::newRow("NUL") << C{'@' - '@'} << I{ProcessToken{token_ctl('@'), 0, 0}};
    QTest::newRow("SOH") << C{'A' - '@'} << I{ProcessToken{token_ctl('A'), 0, 0}};
    QTest::newRow("STX") << C{'B' - '@'} << I{ProcessToken{token_ctl('B'), 0, 0}};
    QTest::newRow("ETX") << C{'C' - '@'} << I{ProcessToken{token_ctl('C'), 0, 0}};
    QTest::newRow("EOT") << C{'D' - '@'} << I{ProcessToken{token_ctl('D'), 0, 0}};
    QTest::newRow("ENQ") << C{'E' - '@'} << I{ProcessToken{token_ctl('E'), 0, 0}};
    QTest::newRow("ACK") << C{'F' - '@'} << I{ProcessToken{token_ctl('F'), 0, 0}};
    QTest::newRow("BEL") << C{'G' - '@'} << I{ProcessToken{token_ctl('G'), 0, 0}};
    QTest::newRow("BS")  << C{'H' - '@'} << I{ProcessToken{token_ctl('H'), 0, 0}};
    QTest::newRow("TAB") << C{'I' - '@'} << I{ProcessToken{token_ctl('I'), 0, 0}};
    QTest::newRow("LF")  << C{'J' - '@'} << I{ProcessToken{token_ctl('J'), 0, 0}};
    QTest::newRow("VT")  << C{'K' - '@'} << I{ProcessToken{token_ctl('K'), 0, 0}};
    QTest::newRow("FF")  << C{'L' - '@'} << I{ProcessToken{token_ctl('L'), 0, 0}};
    QTest::newRow("CR")  << C{'M' - '@'} << I{ProcessToken{token_ctl('M'), 0, 0}};
    QTest::newRow("SO")  << C{'N' - '@'} << I{ProcessToken{token_ctl('N'), 0, 0}};
    QTest::newRow("SI")  << C{'O' - '@'} << I{ProcessToken{token_ctl('O'), 0, 0}};

    QTest::newRow("DLE") << C{'P' - '@'} << I{ProcessToken{token_ctl('P'), 0, 0}};
    QTest::newRow("XON") << C{'Q' - '@'} << I{ProcessToken{token_ctl('Q'), 0, 0}};
    QTest::newRow("DC2") << C{'R' - '@'} << I{ProcessToken{token_ctl('R'), 0, 0}};
    QTest::newRow("XOFF") << C{'S' - '@'} << I{ProcessToken{token_ctl('S'), 0, 0}};
    QTest::newRow("DC4") << C{'T' - '@'} << I{ProcessToken{token_ctl('T'), 0, 0}};
    QTest::newRow("NAK") << C{'U' - '@'} << I{ProcessToken{token_ctl('U'), 0, 0}};
    QTest::newRow("SYN") << C{'V' - '@'} << I{ProcessToken{token_ctl('V'), 0, 0}};
    QTest::newRow("ETB") << C{'W' - '@'} << I{ProcessToken{token_ctl('W'), 0, 0}};
    QTest::newRow("CAN") << C{'X' - '@'} << I{ProcessToken{token_ctl('X'), 0, 0}};
    QTest::newRow("EM")  << C{'Y' - '@'} << I{ProcessToken{token_ctl('Y'), 0, 0}};
    QTest::newRow("SUB") << C{'Z' - '@'} << I{ProcessToken{token_ctl('Z'), 0, 0}};
    // [ is ESC and parsed differently
    QTest::newRow("FS")  << C{'\\' - '@'} << I{ProcessToken{token_ctl('\\'), 0, 0}};
    QTest::newRow("GS")  << C{']' - '@'} << I{ProcessToken{token_ctl(']'), 0, 0}};
    QTest::newRow("RS")  << C{'^' - '@'} << I{ProcessToken{token_ctl('^'), 0, 0}};
    QTest::newRow("US")  << C{'_' - '@'} << I{ProcessToken{token_ctl('_'), 0, 0}};

    QTest::newRow("DEL") << C{127} << I{};

    const uint ESC = '\033';

    QTest::newRow("ESC 7") << C{ESC, '7'} << I{ProcessToken{token_esc('7'), 0, 0}};
    QTest::newRow("ESC 8") << C{ESC, '8'} << I{ProcessToken{token_esc('8'), 0, 0}};
    QTest::newRow("ESC D") << C{ESC, 'D'} << I{ProcessToken{token_esc('D'), 0, 0}};
    QTest::newRow("ESC E") << C{ESC, 'E'} << I{ProcessToken{token_esc('E'), 0, 0}};
    QTest::newRow("ESC H") << C{ESC, 'H'} << I{ProcessToken{token_esc('H'), 0, 0}};
    QTest::newRow("ESC M") << C{ESC, 'M'} << I{ProcessToken{token_esc('M'), 0, 0}};
    QTest::newRow("ESC Z") << C{ESC, 'Z'} << I{ProcessToken{token_esc('Z'), 0, 0}};

    QTest::newRow("ESC c") << C{ESC, 'c'} << I{ProcessToken{token_esc('c'), 0, 0}};

    QTest::newRow("ESC n") << C{ESC, 'n'} << I{ProcessToken{token_esc('n'), 0, 0}};
    QTest::newRow("ESC o") << C{ESC, 'o'} << I{ProcessToken{token_esc('o'), 0, 0}};
    QTest::newRow("ESC >") << C{ESC, '>'} << I{ProcessToken{token_esc('>'), 0, 0}};
    QTest::newRow("ESC <") << C{ESC, '<'} << I{ProcessToken{token_esc('<'), 0, 0}};
    QTest::newRow("ESC =") << C{ESC, '='} << I{ProcessToken{token_esc('='), 0, 0}};

    QTest::newRow("ESC #3") << C{ESC, '#', '3'} << I{ProcessToken{token_esc_de('3'), 0, 0}};
    QTest::newRow("ESC #4") << C{ESC, '#', '4'} << I{ProcessToken{token_esc_de('4'), 0, 0}};
    QTest::newRow("ESC #5") << C{ESC, '#', '5'} << I{ProcessToken{token_esc_de('5'), 0, 0}};
    QTest::newRow("ESC #6") << C{ESC, '#', '6'} << I{ProcessToken{token_esc_de('6'), 0, 0}};
    QTest::newRow("ESC #8") << C{ESC, '#', '8'} << I{ProcessToken{token_esc_de('8'), 0, 0}};

    QTest::newRow("ESC %G") << C{ESC, '%', 'G'} << I{ProcessToken{token_esc_cs('%', 'G'), 0, 0}};
    QTest::newRow("ESC %@") << C{ESC, '%', '@'} << I{ProcessToken{token_esc_cs('%', '@'), 0, 0}};

    QTest::newRow("ESC (0") << C{ESC, '(', '0'} << I{ProcessToken{token_esc_cs('(', '0'), 0, 0}};
    QTest::newRow("ESC (A") << C{ESC, '(', 'A'} << I{ProcessToken{token_esc_cs('(', 'A'), 0, 0}};
    QTest::newRow("ESC (B") << C{ESC, '(', 'B'} << I{ProcessToken{token_esc_cs('(', 'B'), 0, 0}};

    QTest::newRow("ESC )0") << C{ESC, ')', '0'} << I{ProcessToken{token_esc_cs(')', '0'), 0, 0}};
    QTest::newRow("ESC )A") << C{ESC, ')', 'A'} << I{ProcessToken{token_esc_cs(')', 'A'), 0, 0}};
    QTest::newRow("ESC )B") << C{ESC, ')', 'B'} << I{ProcessToken{token_esc_cs(')', 'B'), 0, 0}};

    QTest::newRow("ESC *0") << C{ESC, '*', '0'} << I{ProcessToken{token_esc_cs('*', '0'), 0, 0}};
    QTest::newRow("ESC *A") << C{ESC, '*', 'A'} << I{ProcessToken{token_esc_cs('*', 'A'), 0, 0}};
    QTest::newRow("ESC *B") << C{ESC, '*', 'B'} << I{ProcessToken{token_esc_cs('*', 'B'), 0, 0}};

    QTest::newRow("ESC +0") << C{ESC, '+', '0'} << I{ProcessToken{token_esc_cs('+', '0'), 0, 0}};
    QTest::newRow("ESC +A") << C{ESC, '+', 'A'} << I{ProcessToken{token_esc_cs('+', 'A'), 0, 0}};
    QTest::newRow("ESC +B") << C{ESC, '+', 'B'} << I{ProcessToken{token_esc_cs('+', 'B'), 0, 0}};

    QTest::newRow("ESC [8;12;45t") << C{ESC, '[', '8', ';', '1', '2', ';', '4', '5', 't'} << I{ProcessToken{token_csi_ps('t', 8), 12, 45}};
    QTest::newRow("ESC [18t")      << C{ESC, '[', '1', '8', 't'} << I{ProcessToken{token_csi_ps('t', 18), 0, 0}};
    QTest::newRow("ESC [18;1;2t")  << C{ESC, '[', '1', '8', ';', '1', ';', '2', 't'} << I{ProcessToken{token_csi_ps('t', 18), 1, 2}};

    QTest::newRow("ESC [K")  << C{ESC, '[', 'K'} << I{ProcessToken{token_csi_ps('K', 0), 0, 0}};
    QTest::newRow("ESC [0K") << C{ESC, '[', '0', 'K'} << I{ProcessToken{token_csi_ps('K', 0), 0, 0}};
    QTest::newRow("ESC [1K") << C{ESC, '[', '1', 'K'} << I{ProcessToken{token_csi_ps('K', 1), 0, 0}};

    QTest::newRow("ESC [@")   << C{ESC, '[', '@'} << I{ProcessToken{token_csi_pn('@'), 0, 0}};
    QTest::newRow("ESC [12@") << C{ESC, '[', '1', '2', '@'} << I{ProcessToken{token_csi_pn('@'), 12, 0}};
    QTest::newRow("ESC [H")   << C{ESC, '[', 'H'} << I{ProcessToken{token_csi_pn('H'), 0, 0}};
    QTest::newRow("ESC [24H") << C{ESC, '[', '2', '4', 'H'} << I{ProcessToken{token_csi_pn('H'), 24, 0}};
    QTest::newRow("ESC [32,13H") << C{ESC, '[', '3', '2', ';', '1', '3', 'H'} << I{ProcessToken{token_csi_pn('H'), 32, 13}};

    QTest::newRow("ESC [m")   << C{ESC, '[', 'm'} << I{ProcessToken{token_csi_ps('m', 0), 0, 0}};
    QTest::newRow("ESC [1m")  << C{ESC, '[', '1', 'm'} << I{ProcessToken{token_csi_ps('m', 1), 0, 0}};
    QTest::newRow("ESC [1;2m") << C{ESC, '[', '1', ';', '2', 'm'} << I{ProcessToken{token_csi_ps('m', 1), 0, 0}, ProcessToken{token_csi_ps('m', 2), 0, 0}};
    QTest::newRow("ESC [38;2;193;202;218m") << C{ESC, '[', '3', '8', ';', '2', ';', '1', '9', '3', ';', '2', '0', '2', ';', '2', '1', '8', 'm'}
                                            << I{ProcessToken{token_csi_ps('m', 38), 4, 0xC1CADA}};
    QTest::newRow("ESC [38;2;193;202;218;2m") << C{ESC, '[', '3', '8', ';', '2', ';', '1', '9', '3', ';', '2', '0', '2', ';', '2', '1', '8', ';', '2', 'm'}
                                              << I{ProcessToken{token_csi_ps('m', 38), 4, 0xC1CADA}, ProcessToken{token_csi_ps('m', 2), 0, 0}};
    QTest::newRow("ESC [38:2:193:202:218m") << C{ESC, '[', '3', '8', ':', '2', ':', '1', '9', '3', ':', '2', '0', '2', ':', '2', '1', '8', 'm'}
                                            << I{ProcessToken{token_csi_ps('m', 38), 4, 0xC1CADA}};
    QTest::newRow("ESC [38:2:193:202:218;2m") << C{ESC, '[', '3', '8', ':', '2', ':', '1', '9', '3', ':', '2', '0', '2', ':', '2', '1', '8', ';', '2', 'm'}
                                              << I{ProcessToken{token_csi_ps('m', 38), 4, 0xC1CADA}, ProcessToken{token_csi_ps('m', 2), 0, 0}};
    QTest::newRow("ESC [38:2:1:193:202:218m") << C{ESC, '[', '3', '8', ':', '2', ':', '1', ':', '1', '9', '3', ':', '2', '0', '2', ':', '2', '1', '8', 'm'}
                                              << I{ProcessToken{token_csi_ps('m', 38), 4, 0xC1CADA}};
    QTest::newRow("ESC [38;5;255;2m") << C{ESC, '[', '3', '8', ';', '5', ';', '2', '5', '5', ';', '2', 'm'}
                                      << I{ProcessToken{token_csi_ps('m', 38), 3, 255}, ProcessToken{token_csi_ps('m', 2), 0, 0}};
    QTest::newRow("ESC [38:5:255m") << C{ESC, '[', '3', '8', ':', '5', ':', '2', '5', '5', 'm'}
                                    << I{ProcessToken{token_csi_ps('m', 38), 3, 255}};

    QTest::newRow("ESC [5n")  << C{ESC, '[', '5', 'n'} << I{ProcessToken{token_csi_ps('n', 5), 0, 0}};

    QTest::newRow("ESC [?1h") << C{ESC, '[', '?', '1', 'h'} << I{ProcessToken{token_csi_pr('h', 1), 0, 0}};
    QTest::newRow("ESC [?1l") << C{ESC, '[', '?', '1', 'l'} << I{ProcessToken{token_csi_pr('l', 1), 0, 0}};
    QTest::newRow("ESC [?1r") << C{ESC, '[', '?', '1', 'r'} << I{ProcessToken{token_csi_pr('r', 1), 0, 0}};
    QTest::newRow("ESC [?1s") << C{ESC, '[', '?', '1', 's'} << I{ProcessToken{token_csi_pr('s', 1), 0, 0}};

    QTest::newRow("ESC [?1;2h") << C{ESC, '[', '?', '1', ';', '2', 'h'}
                                << I{ProcessToken{token_csi_pr('h', 1), 0, 0}, ProcessToken{token_csi_pr('h', 2), 1, 0}};
    QTest::newRow("ESC [?1;2l") << C{ESC, '[', '?', '1', ';', '2', 'l'}
                                << I{ProcessToken{token_csi_pr('l', 1), 0, 0}, ProcessToken{token_csi_pr('l', 2), 1, 0}};
    QTest::newRow("ESC [?1;2r") << C{ESC, '[', '?', '1', ';', '2', 'r'}
                                << I{ProcessToken{token_csi_pr('r', 1), 0, 0}, ProcessToken{token_csi_pr('r', 2), 1, 0}};
    QTest::newRow("ESC [?1;2s") << C{ESC, '[', '?', '1', ';', '2', 's'}
                                << I{ProcessToken{token_csi_pr('s', 1), 0, 0}, ProcessToken{token_csi_pr('s', 2), 1, 0}};

    QTest::newRow("ESC [? q")  << C{ESC, '[', ' ', 'q'} << I{ProcessToken{token_csi_sp('q'), 0, 0}};
    QTest::newRow("ESC [? 1q") << C{ESC, '[', '1', ' ', 'q'} << I{ProcessToken{token_csi_psp('q', 1), 0, 0}};

    QTest::newRow("ESC [!p") << C{ESC, '[', '!', 'p'} << I{ProcessToken{token_csi_pe('p'), 0, 0}};
    QTest::newRow("ESC [=c") << C{ESC, '[', '=', 'p'} << I{ProcessToken{token_csi_pq('p'), 0, 0}};
    QTest::newRow("ESC [>c") << C{ESC, '[', '>', 'p'} << I{ProcessToken{token_csi_pg('p'), 0, 0}};
    /* clang-format on */
}

void Vt102EmulationTest::testTokenizing()
{
    QFETCH(QVector<uint>, input);
    QFETCH(std::vector<TestEmulation::Item>, expectedItems);

    TestEmulation em;
    em.reset();
    em.blockFurtherProcessing = true;

    em._currentScreen->clearEntireScreen();

    em.receiveChars(input);
    QString printed = em._currentScreen->text(0, em._currentScreen->getColumns(), Screen::PlainText);
    printed.chop(2); // Remove trailing space and newline

    QCOMPARE(printed, QStringLiteral(""));
    QCOMPARE(em.items, expectedItems);
}

void Vt102EmulationTest::testTokenizingVT52_data()
{
    QTest::addColumn<QVector<uint>>("input");
    QTest::addColumn<std::vector<TestEmulation::Item>>("expectedItems");

    using ProcessToken = TestEmulation::ProcessToken;

    using C = QVector<uint>;
    using I = std::vector<TestEmulation::Item>;

    /* clang-format off */
    QTest::newRow("NUL") << C{'@' - '@'} << I{ProcessToken{token_ctl('@'), 0, 0}};
    QTest::newRow("SOH") << C{'A' - '@'} << I{ProcessToken{token_ctl('A'), 0, 0}};
    QTest::newRow("STX") << C{'B' - '@'} << I{ProcessToken{token_ctl('B'), 0, 0}};
    QTest::newRow("ETX") << C{'C' - '@'} << I{ProcessToken{token_ctl('C'), 0, 0}};
    QTest::newRow("EOT") << C{'D' - '@'} << I{ProcessToken{token_ctl('D'), 0, 0}};
    QTest::newRow("ENQ") << C{'E' - '@'} << I{ProcessToken{token_ctl('E'), 0, 0}};
    QTest::newRow("ACK") << C{'F' - '@'} << I{ProcessToken{token_ctl('F'), 0, 0}};
    QTest::newRow("BEL") << C{'G' - '@'} << I{ProcessToken{token_ctl('G'), 0, 0}};
    QTest::newRow("BS")  << C{'H' - '@'} << I{ProcessToken{token_ctl('H'), 0, 0}};
    QTest::newRow("TAB") << C{'I' - '@'} << I{ProcessToken{token_ctl('I'), 0, 0}};
    QTest::newRow("LF")  << C{'J' - '@'} << I{ProcessToken{token_ctl('J'), 0, 0}};
    QTest::newRow("VT")  << C{'K' - '@'} << I{ProcessToken{token_ctl('K'), 0, 0}};
    QTest::newRow("FF")  << C{'L' - '@'} << I{ProcessToken{token_ctl('L'), 0, 0}};
    QTest::newRow("CR")  << C{'M' - '@'} << I{ProcessToken{token_ctl('M'), 0, 0}};
    QTest::newRow("SO")  << C{'N' - '@'} << I{ProcessToken{token_ctl('N'), 0, 0}};
    QTest::newRow("SI")  << C{'O' - '@'} << I{ProcessToken{token_ctl('O'), 0, 0}};

    QTest::newRow("DLE") << C{'P' - '@'} << I{ProcessToken{token_ctl('P'), 0, 0}};
    QTest::newRow("XON") << C{'Q' - '@'} << I{ProcessToken{token_ctl('Q'), 0, 0}};
    QTest::newRow("DC2") << C{'R' - '@'} << I{ProcessToken{token_ctl('R'), 0, 0}};
    QTest::newRow("XOFF") << C{'S' - '@'} << I{ProcessToken{token_ctl('S'), 0, 0}};
    QTest::newRow("DC4") << C{'T' - '@'} << I{ProcessToken{token_ctl('T'), 0, 0}};
    QTest::newRow("NAK") << C{'U' - '@'} << I{ProcessToken{token_ctl('U'), 0, 0}};
    QTest::newRow("SYN") << C{'V' - '@'} << I{ProcessToken{token_ctl('V'), 0, 0}};
    QTest::newRow("ETB") << C{'W' - '@'} << I{ProcessToken{token_ctl('W'), 0, 0}};
    QTest::newRow("CAN") << C{'X' - '@'} << I{ProcessToken{token_ctl('X'), 0, 0}};
    QTest::newRow("EM")  << C{'Y' - '@'} << I{ProcessToken{token_ctl('Y'), 0, 0}};
    QTest::newRow("SUB") << C{'Z' - '@'} << I{ProcessToken{token_ctl('Z'), 0, 0}};
    // [ is ESC and parsed differently
    QTest::newRow("FS")  << C{'\\' - '@'} << I{ProcessToken{token_ctl('\\'), 0, 0}};
    QTest::newRow("GS")  << C{']' - '@'} << I{ProcessToken{token_ctl(']'), 0, 0}};
    QTest::newRow("RS")  << C{'^' - '@'} << I{ProcessToken{token_ctl('^'), 0, 0}};
    QTest::newRow("US")  << C{'_' - '@'} << I{ProcessToken{token_ctl('_'), 0, 0}};

    QTest::newRow("DEL") << C{127} << I{};

    const uint ESC = '\033';

    QTest::newRow("ESC A") << C{ESC, 'A'} << I{ProcessToken{token_vt52('A'), 0, 0}};
    QTest::newRow("ESC B") << C{ESC, 'B'} << I{ProcessToken{token_vt52('B'), 0, 0}};
    QTest::newRow("ESC C") << C{ESC, 'C'} << I{ProcessToken{token_vt52('C'), 0, 0}};
    QTest::newRow("ESC D") << C{ESC, 'D'} << I{ProcessToken{token_vt52('D'), 0, 0}};
    QTest::newRow("ESC F") << C{ESC, 'F'} << I{ProcessToken{token_vt52('F'), 0, 0}};
    QTest::newRow("ESC G") << C{ESC, 'G'} << I{ProcessToken{token_vt52('G'), 0, 0}};
    QTest::newRow("ESC H") << C{ESC, 'H'} << I{ProcessToken{token_vt52('H'), 0, 0}};
    QTest::newRow("ESC I") << C{ESC, 'I'} << I{ProcessToken{token_vt52('I'), 0, 0}};
    QTest::newRow("ESC J") << C{ESC, 'J'} << I{ProcessToken{token_vt52('J'), 0, 0}};
    QTest::newRow("ESC K") << C{ESC, 'K'} << I{ProcessToken{token_vt52('K'), 0, 0}};
    QTest::newRow("ESC Yab") << C{ESC, 'Y', 'a', 'b'} << I{ProcessToken{token_vt52('Y'), 'a', 'b'}};
    QTest::newRow("ESC Z") << C{ESC, 'Z'} << I{ProcessToken{token_vt52('Z'), 0, 0}};
    QTest::newRow("ESC <") << C{ESC, '<'} << I{ProcessToken{token_vt52('<'), 0, 0}};
    QTest::newRow("ESC =") << C{ESC, '='} << I{ProcessToken{token_vt52('='), 0, 0}};
    QTest::newRow("ESC >") << C{ESC, '>'} << I{ProcessToken{token_vt52('>'), 0, 0}};
    /* clang-format on */
}

void Vt102EmulationTest::testTokenizingVT52()
{
    QFETCH(QVector<uint>, input);
    QFETCH(std::vector<TestEmulation::Item>, expectedItems);

    TestEmulation em;
    em.reset();
    em.resetMode(MODE_Ansi);
    em.blockFurtherProcessing = true;

    em._currentScreen->clearEntireScreen();

    em.receiveChars(input);
    QString printed = em._currentScreen->text(0, em._currentScreen->getColumns(), Screen::PlainText);
    printed.chop(2); // Remove trailing space and newline

    QCOMPARE(printed, QStringLiteral(""));
    QCOMPARE(em.items, expectedItems);
}

void Vt102EmulationTest::testTokenFunctions()
{
    QCOMPARE(token_construct(0, 0, 0), TY_CONSTRUCT(0, 0, 0));
    QCOMPARE(token_chr(), TY_CHR());
    QCOMPARE(token_ctl(8 + '@'), TY_CTL(8 + '@'));
    QCOMPARE(token_ctl('G'), TY_CTL('G'));
    QCOMPARE(token_csi_pe('p'), TY_CSI_PE('p'));
    QCOMPARE(token_csi_pg('c'), TY_CSI_PG('c'));
    QCOMPARE(token_csi_pn(8), TY_CSI_PN(8));
    QCOMPARE(token_csi_pn('N'), TY_CSI_PN('N'));
    QCOMPARE(token_csi_pr('r', 2), TY_CSI_PR('r', 2));
    QCOMPARE(token_csi_pr('s', 1000), TY_CSI_PR('s', 1000));
    QCOMPARE(token_csi_ps('m', 8), TY_CSI_PS('m', 8));
    QCOMPARE(token_csi_ps('m', 48), TY_CSI_PS('m', 48));
    QCOMPARE(token_csi_ps('K', 2), TY_CSI_PS('K', 2));
    QCOMPARE(token_esc(8), TY_ESC(8));
    QCOMPARE(token_esc('='), TY_ESC('='));
    QCOMPARE(token_esc('>'), TY_ESC('>'));
    QCOMPARE(token_esc_cs(8, 0), TY_ESC_CS(8, 0));
    QCOMPARE(token_esc_cs('(', '0'), TY_ESC_CS('(', '0'));
    QCOMPARE(token_esc_cs(')', 'B'), TY_ESC_CS(')', 'B'));
    QCOMPARE(token_esc_de(8), TY_ESC_DE(8));
    QCOMPARE(token_esc_de('3'), TY_ESC_DE('3'));
    QCOMPARE(token_vt52('A'), TY_VT52('A'));
    QCOMPARE(token_vt52('Z'), TY_VT52('Z'));
    QCOMPARE(token_vt52('='), TY_VT52('='));
    QCOMPARE(token_vt52('>'), TY_VT52('>'));
}

QTEST_GUILESS_MAIN(Vt102EmulationTest)

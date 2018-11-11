/*
    Copyright 2018 by Kurt Hindenburg <kurt.hindenburg@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own
#include "Vt102EmulationTest.h"

#include "qtest.h"

// The below is to verify the old #defines match the new constexprs
// Just copy/paste for now from Vt102Emulation.cpp
#define TY_CONSTRUCT(T,A,N) ( ((((int)(N)) & 0xffff) << 16) | ((((int)(A)) & 0xff) << 8) | (((int)(T)) & 0xff) )
#define TY_CHR(   )     TY_CONSTRUCT(0,0,0)
#define TY_CTL(A  )     TY_CONSTRUCT(1,A,0)
#define TY_ESC(A  )     TY_CONSTRUCT(2,A,0)
#define TY_ESC_CS(A,B)  TY_CONSTRUCT(3,A,B)
#define TY_ESC_DE(A  )  TY_CONSTRUCT(4,A,0)
#define TY_CSI_PS(A,N)  TY_CONSTRUCT(5,A,N)
#define TY_CSI_PN(A  )  TY_CONSTRUCT(6,A,0)
#define TY_CSI_PR(A,N)  TY_CONSTRUCT(7,A,N)
#define TY_VT52(A)      TY_CONSTRUCT(8,A,0)
#define TY_CSI_PG(A)    TY_CONSTRUCT(9,A,0)
#define TY_CSI_PE(A)    TY_CONSTRUCT(10,A,0)

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

void Vt102EmulationTest::testTokenFunctions()
{
    QCOMPARE(token_construct(0, 0, 0), TY_CONSTRUCT(0, 0, 0));
    QCOMPARE(token_chr(), TY_CHR());
    QCOMPARE(token_ctl(8+'@'), TY_CTL(8+'@'));
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

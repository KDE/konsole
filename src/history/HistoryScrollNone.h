/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef HISTORYSCROLLNONE_H
#define HISTORYSCROLLNONE_H

#include "konsoleprivate_export.h"

#include "HistoryScroll.h"

namespace Konsole
{

//////////////////////////////////////////////////////////////////////
// Nothing-based history (no history :-)
//////////////////////////////////////////////////////////////////////
class KONSOLEPRIVATE_EXPORT HistoryScrollNone : public HistoryScroll
{
public:
    HistoryScrollNone();
    ~HistoryScrollNone() override;

    bool hasScroll() override;

    int  getLines() override;
    int  getMaxLines() override;
    int  getLineLen(int lineno) override;
    void getCells(int lineno, int colno, int count, Character res[]) override;
    bool isWrappedLine(int lineno) override;

    void addCells(const Character a[], int count) override;
    void addLine(bool previousWrapped = false) override;

    void insertCellsVector(int position, const QVector<Character> &cells) override;
    void insertCells(int position, const Character a[], int count) override;
    void removeCells(int position) override;
    void setCellsAt(int position, const Character a[], int count) override;
    void setCellsVectorAt(int position, const QVector<Character> &cells) override;
    void setLineAt(int position, bool previousWrapped) override;
};

}

#endif

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

    bool hasScroll() const override;

    int getLines() const override;
    int getMaxLines() const override;
    int getLineLen(const int lineno) const override;
    void getCells(const int lineno, const int colno, const int count, Character res[]) const override;
    bool isWrappedLine(const int lineno) const override;
    LineProperty getLineProperty(const int lineno) const override;

    void addCells(const Character a[], const int count) override;
    void addCellsMove(Character a[], const int count) override;
    void addLine(const LineProperty lineProperty = 0) override;

    // Modify history (do nothing here)
    void removeCells() override;
    int reflowLines(const int, std::map<int, int> * = nullptr) override;
};

}

#endif

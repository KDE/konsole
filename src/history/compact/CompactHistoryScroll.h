/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COMPACTHISTORYSCROLL_H
#define COMPACTHISTORYSCROLL_H

#include "konsoleprivate_export.h"

#include "history/HistoryScroll.h"
#include "history/compact/CompactHistoryLine.h"

namespace Konsole
{

class KONSOLEPRIVATE_EXPORT CompactHistoryScroll : public HistoryScroll
{
    typedef QList<CompactHistoryLine *> HistoryArray;

public:
    explicit CompactHistoryScroll(unsigned int maxLineCount = 1000);
    ~CompactHistoryScroll() override;

    int  getLines() override;
    int  getLineLen(int lineNumber) override;
    void getCells(int lineNumber, int startColumn, int count, Character buffer[]) override;
    bool isWrappedLine(int lineNumber) override;

    void addCells(const Character a[], int count) override;
    void addCellsVector(const TextLine &cells) override;
    void addLine(bool previousWrapped = false) override;

    void setMaxNbLines(unsigned int lineCount);

private:
    bool hasDifferentColors(const TextLine &line) const;
    HistoryArray _lines;
    CompactHistoryBlockList _blockList;

    unsigned int _maxLineCount;
};

}

#endif

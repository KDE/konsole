/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COMPACTHISTORYSCROLL_H
#define COMPACTHISTORYSCROLL_H

// STD
#include <deque>

#include "konsoleprivate_export.h"

#include "history/HistoryScroll.h"

namespace Konsole
{

class KONSOLEPRIVATE_EXPORT CompactHistoryScroll : public HistoryScroll
{
    typedef QVector<Character> TextLine;

public:
    explicit CompactHistoryScroll(unsigned int maxLineCount = 1000);
    ~CompactHistoryScroll() = default;

    int  getLines() override;
    int  getMaxLines() override;
    int  getLineLen(int lineNumber) override;
    void getCells(int lineNumber, int startColumn, int count, Character buffer[]) override;
    bool isWrappedLine(int lineNumber) override;
    LineProperty getLineProperty(int lineNumber) override;

    void addCells(const Character a[], int count) override;
    void addLine(LineProperty lineProperty = 0) override;

    void removeCells() override;

    void setMaxNbLines(int lineCount);

    int reflowLines(int columns) override;

private:
    QList<Character> _cells;
    QList<int> _index;
    QList<LineProperty> _flags;

    int _maxLineCount;

    void removeFirstLine();
    inline int lineLen(const int line);
    inline int startOfLine(int line);
};

}

#endif

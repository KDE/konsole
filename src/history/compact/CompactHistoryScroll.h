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
    explicit CompactHistoryScroll(const unsigned int maxLineCount = 1000);
    ~CompactHistoryScroll() = default;

    int  getLines() const override;
    int  getMaxLines() const override;
    int  getLineLen(const int lineNumber) const override;
    void getCells(const int lineNumber, const int startColumn, const int count, Character buffer[]) const override;
    bool isWrappedLine(const int lineNumber) const override;
    LineProperty getLineProperty(const int lineNumber) const override;

    void addCells(const Character a[], const int count) override;
    void addLine(const LineProperty lineProperty = 0) override;

    void removeCells() override;

    void setMaxNbLines(const int lineCount);

    int reflowLines(const int columns) override;

private:
    QList<Character> _cells;
    QList<int> _index;
    QList<LineProperty> _flags;

    int _maxLineCount;

    void removeFirstLine();
    inline int lineLen(const int line) const;
    inline int startOfLine(const int line) const;
};

}

#endif

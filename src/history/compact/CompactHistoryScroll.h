/*
    SPDX-FileCopyrightText: 2021-2021 Carlos Alves <cbcalves@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COMPACTHISTORYSCROLL_H
#define COMPACTHISTORYSCROLL_H

#include "history/HistoryScroll.h"
#include "konsoleprivate_export.h"
#include <deque>

namespace Konsole
{
class KONSOLEPRIVATE_EXPORT CompactHistoryScroll final : public HistoryScroll
{
    typedef QVector<Character> TextLine;

public:
    explicit CompactHistoryScroll(const unsigned int maxLineCount = 1000);
    ~CompactHistoryScroll() override = default;

    int getLines() const override;
    int getMaxLines() const override;
    int getLineLen(const int lineNumber) const override;
    void getCells(const int lineNumber, const int startColumn, const int count, Character buffer[]) const override;
    bool isWrappedLine(const int lineNumber) const override;
    LineProperty getLineProperty(const int lineNumber) const override;

    void addCells(const Character a[], const int count) override;
    void addLine(const LineProperty lineProperty = 0) override;

    void removeCells() override;

    void setMaxNbLines(const int lineCount);

    int reflowLines(const int columns) override;

private:
    /**
     * This is the actual buffer that contains the cells
     */
    std::deque<Character> _cells;

    /**
     * This buffer contains the length of each line in _cells
     * The size of this buffer is the number of lines we have.
     */
    std::vector<int> _lineLengths;

    /**
     * This contains the flag for each line
     * size is always equal to _index
     */
    std::vector<LineProperty> _flags;

    /**
     * Max number of lines we can hold
     */
    size_t _maxLineCount;

    /**
     * Remove @p lines from the "start" of above buffers
     */
    void removeLinesFromTop(int lines);

    inline int lineLen(const int line) const
    {
        return _lineLengths.at(line);
    }

    /**
     * Get the start of @p line in _cells buffer
     *
     * Since _index contains lengths of lines, we need
     * to add up till we reach @p line. That will be the starting point
     * of @p line.
     */
    inline int startOfLine(const int line) const
    {
        return std::accumulate(_lineLengths.begin(), _lineLengths.begin() + line, 0);
    }
};

}

#endif

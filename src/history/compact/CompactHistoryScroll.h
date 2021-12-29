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
     * Each entry contains the start of the next line and the current line's
     * properties.  The start of lines is biased by _indexBias, i.e. an index
     * value of _indexBias corresponds to _cells' zero index.
     *
     * The use of a biased line start means we don't need to traverse the
     * vector recalculating line starts when removing lines from the top, and
     * also we don't need to traverse the vector to compute the start of a line
     * as we would have to do if we stored line lengths.
     *
     * unsigned int means we're limited in common architectures to 4 million
     * characters, but CompactHistoryScroll is limited by the UI to 1_000_000
     * lines (see historyLineSpinner in src/widgets/HistorySizeWidget.ui), so
     * enough for 1_000_000 lines of an average ~4295 length (and each
     * Character takes 16 bytes, so that's 64Gb!).
     */
    struct LineData {
        unsigned int index;
        LineProperty flag;
    };
    /**
     * This buffer contains the data about each line
     * The size of this buffer is the number of lines we have.
     */
    std::vector<LineData> _lineDatas;
    unsigned int _indexBias;

    /**
     * Max number of lines we can hold
     */
    size_t _maxLineCount;

    /**
     * Remove @p lines from the "start" of above buffers
     */
    void removeLinesFromTop(size_t lines);

    inline int lineLen(const int line) const
    {
        return line == 0 ? _lineDatas.at(0).index - _indexBias : _lineDatas.at(line).index - _lineDatas.at(line - 1).index;
    }

    /**
     * Get the start of @p line in _cells buffer
     *
     * index actually contains the start of the next line.
     */
    inline int startOfLine(const int line) const
    {
        return line == 0 ? 0 : _lineDatas.at(line - 1).index - _indexBias;
    }
};

}

#endif

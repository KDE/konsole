/*
    SPDX-FileCopyrightText: 2021-2021 Carlos Alves <cbcalves@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "CompactHistoryScroll.h"
#include "CompactHistoryType.h"

using namespace Konsole;

struct reflowData { // data to reflow lines
    std::vector<int> index;
    std::vector<LineProperty> flags;
};

CompactHistoryScroll::CompactHistoryScroll(const unsigned int maxLineCount)
    : HistoryScroll(new CompactHistoryType(maxLineCount))
    , _maxLineCount(0)
{
    setMaxNbLines(maxLineCount);
}

void CompactHistoryScroll::removeLinesFromTop(int lines)
{
    if (_lineLengths.size() > 1) {
        _flags.erase(_flags.begin(), _flags.begin() + lines);

        const int removing = std::accumulate(_lineLengths.begin(), _lineLengths.begin() + lines, 0);
        _lineLengths.erase(_lineLengths.begin(), _lineLengths.begin() + lines);

        _cells.erase(_cells.begin(), _cells.begin() + removing);
    } else {
        _flags.clear();
        _lineLengths.clear();
        _cells.clear();
    }
}

void CompactHistoryScroll::addCells(const Character a[], const int count)
{
    _cells.insert(_cells.end(), a, a + count);

    // store the length of line
    _lineLengths.push_back(count);
    // and the line's flag
    _flags.push_back(LINE_DEFAULT);

    if (_lineLengths.size() > _maxLineCount + 5) {
        removeLinesFromTop(5);
    }
}

void CompactHistoryScroll::addLine(const LineProperty lineProperty)
{
    auto &flag = _flags.back();
    flag = lineProperty;
}

int CompactHistoryScroll::getLines() const
{
    return _lineLengths.size();
}

int CompactHistoryScroll::getMaxLines() const
{
    return _maxLineCount;
}

int CompactHistoryScroll::getLineLen(int lineNumber) const
{
    if (size_t(lineNumber) >= _lineLengths.size()) {
        return 0;
    }

    return lineLen(lineNumber);
}

void CompactHistoryScroll::getCells(const int lineNumber, const int startColumn, const int count, Character buffer[]) const
{
    if (count == 0) {
        return;
    }
    Q_ASSERT((size_t)lineNumber < _lineLengths.size());

    Q_ASSERT(startColumn >= 0);
    Q_ASSERT(startColumn <= lineLen(lineNumber) - count);

    auto startCopy = _cells.begin() + startOfLine(lineNumber) + startColumn;
    auto endCopy = startCopy + count;
    std::copy(startCopy, endCopy, buffer);
}

void CompactHistoryScroll::setMaxNbLines(const int lineCount)
{
    Q_ASSERT(lineCount >= 0);
    _maxLineCount = lineCount;

    if (_lineLengths.size() > _maxLineCount) {
        int linesToRemove = _lineLengths.size() - _maxLineCount;
        removeLinesFromTop(linesToRemove);
    }
}

void CompactHistoryScroll::removeCells()
{
    if (_lineLengths.size() > 1) {
        /** Here we remove a line from the "end" of the buffers **/

        // Get last line start
        int lastLineStart = startOfLine(_lineLengths.size() - 1);
        // remove it from _index
        _lineLengths.pop_back();
        // remove the flag for this line
        _flags.pop_back();
        // remove the line data
        _cells.erase(_cells.begin() + lastLineStart, _cells.end());
    } else {
        _cells.clear();
        _lineLengths.clear();
        _flags.clear();
    }
}

bool CompactHistoryScroll::isWrappedLine(const int lineNumber) const
{
    Q_ASSERT((size_t)lineNumber < _lineLengths.size());
    return (_flags.at(lineNumber) & LINE_WRAPPED) > 0;
}

LineProperty CompactHistoryScroll::getLineProperty(const int lineNumber) const
{
    Q_ASSERT((size_t)lineNumber < _lineLengths.size());
    return _flags.at(lineNumber);
}

int CompactHistoryScroll::reflowLines(const int columns)
{
    reflowData newLine;

    auto reflowLineLen = [](int start, int end) {
        return end - start;
    };
    auto setNewLine = [](reflowData &change, int index, LineProperty flag) {
        change.index.push_back(index);
        change.flags.push_back(flag);
    };

    int currentPos = 0;
    while (currentPos < getLines()) {
        int lineStart = startOfLine(currentPos);
        // next line will be startLine + length of startLine
        // i.e., start of next line
        int endLine = lineStart + lineLen(currentPos);
        LineProperty lineProperty = getLineProperty(currentPos);

        // Join the lines if they are wrapped
        while (currentPos < getLines() - 1 && isWrappedLine(currentPos)) {
            currentPos++;
            endLine += lineLen(currentPos);
        }

        // Now reflow the lines
        int last = lineStart;
        while (reflowLineLen(lineStart, endLine) > columns && !(lineProperty & (LINE_DOUBLEHEIGHT_BOTTOM | LINE_DOUBLEHEIGHT_TOP))) {
            lineStart += columns;
            // new length = newLineStart - prevLineStart
            setNewLine(newLine, lineStart - last, lineProperty | LINE_WRAPPED);
            last = lineStart;
        }
        setNewLine(newLine, endLine - last, lineProperty & ~LINE_WRAPPED);
        currentPos++;
    }
    _lineLengths = std::move(newLine.index);
    _flags = std::move(newLine.flags);

    int deletedLines = 0;
    size_t totalLines = getLines();
    if (totalLines > _maxLineCount) {
        deletedLines = totalLines - _maxLineCount;
        removeLinesFromTop(deletedLines);
    }

    return deletedLines;
}

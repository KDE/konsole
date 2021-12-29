/*
    SPDX-FileCopyrightText: 2021-2021 Carlos Alves <cbcalves@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "CompactHistoryScroll.h"
#include "CompactHistoryType.h"

using namespace Konsole;

CompactHistoryScroll::CompactHistoryScroll(const unsigned int maxLineCount)
    : HistoryScroll(new CompactHistoryType(maxLineCount))
    , _maxLineCount(0)
{
    setMaxNbLines(maxLineCount);
}

void CompactHistoryScroll::removeLinesFromTop(size_t lines)
{
    if (_lineDatas.size() > 1) {
        const unsigned int removing = _lineDatas.at(lines - 1).index;
        _lineDatas.erase(_lineDatas.begin(), _lineDatas.begin() + lines);

        _cells.erase(_cells.begin(), _cells.begin() + (removing - _indexBias));
        _indexBias = removing;
    } else {
        _lineDatas.clear();
        _cells.clear();
    }
}

void CompactHistoryScroll::addCells(const Character a[], const int count)
{
    _cells.insert(_cells.end(), a, a + count);

    // store the (biased) start of next line + default flag
    // the flag is later updated when addLine is called
    _lineDatas.push_back({static_cast<unsigned int>(_cells.size() + _indexBias), LINE_DEFAULT});

    if (_lineDatas.size() > _maxLineCount + 5) {
        removeLinesFromTop(5);
    }
}

void CompactHistoryScroll::addLine(const LineProperty lineProperty)
{
    auto &flag = _lineDatas.back().flag;
    flag = lineProperty;
}

int CompactHistoryScroll::getLines() const
{
    return _lineDatas.size();
}

int CompactHistoryScroll::getMaxLines() const
{
    return _maxLineCount;
}

int CompactHistoryScroll::getLineLen(int lineNumber) const
{
    if (size_t(lineNumber) >= _lineDatas.size()) {
        return 0;
    }

    return lineLen(lineNumber);
}

void CompactHistoryScroll::getCells(const int lineNumber, const int startColumn, const int count, Character buffer[]) const
{
    if (count == 0) {
        return;
    }
    Q_ASSERT((size_t)lineNumber < _lineDatas.size());

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

    if (_lineDatas.size() > _maxLineCount) {
        int linesToRemove = _lineDatas.size() - _maxLineCount;
        removeLinesFromTop(linesToRemove);
    }
}

void CompactHistoryScroll::removeCells()
{
    if (_lineDatas.size() > 1) {
        /** Here we remove a line from the "end" of the buffers **/

        // Get last line start
        int lastLineStart = startOfLine(_lineDatas.size() - 1);

        // remove info about this line
        _lineDatas.pop_back();

        // remove the actual line content
        _cells.erase(_cells.begin() + lastLineStart, _cells.end());
    } else {
        _cells.clear();
        _lineDatas.clear();
    }
}

bool CompactHistoryScroll::isWrappedLine(const int lineNumber) const
{
    Q_ASSERT((size_t)lineNumber < _lineDatas.size());
    return (_lineDatas.at(lineNumber).flag & LINE_WRAPPED) > 0;
}

LineProperty CompactHistoryScroll::getLineProperty(const int lineNumber) const
{
    Q_ASSERT((size_t)lineNumber < _lineDatas.size());
    return _lineDatas.at(lineNumber).flag;
}

int CompactHistoryScroll::reflowLines(const int columns)
{
    std::vector<LineData> newLineData;

    auto reflowLineLen = [](int start, int end) {
        return end - start;
    };
    auto setNewLine = [](std::vector<LineData> &change, unsigned int index, LineProperty flag) {
        change.push_back({index, flag});
    };

    int currentPos = 0;
    while (currentPos < getLines()) {
        int startLine = startOfLine(currentPos);
        int endLine = startOfLine(currentPos + 1);
        LineProperty lineProperty = getLineProperty(currentPos);

        // Join the lines if they are wrapped
        while (currentPos < getLines() - 1 && isWrappedLine(currentPos)) {
            currentPos++;
            endLine = startOfLine(currentPos + 1);
        }

        // Now reflow the lines
        while (reflowLineLen(startLine, endLine) > columns && !(lineProperty & (LINE_DOUBLEHEIGHT_BOTTOM | LINE_DOUBLEHEIGHT_TOP))) {
            startLine += columns;
            setNewLine(newLineData, startLine + _indexBias, lineProperty | LINE_WRAPPED);
        }
        setNewLine(newLineData, endLine + _indexBias, lineProperty & ~LINE_WRAPPED);
        currentPos++;
    }
    _lineDatas = std::move(newLineData);

    int deletedLines = 0;
    size_t totalLines = getLines();
    if (totalLines > _maxLineCount) {
        deletedLines = totalLines - _maxLineCount;
        removeLinesFromTop(deletedLines);
    }

    return deletedLines;
}

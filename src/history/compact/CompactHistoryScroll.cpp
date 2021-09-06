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
    QList<int> index;
    QList<LineProperty> flags;
};

CompactHistoryScroll::CompactHistoryScroll(const unsigned int maxLineCount)
    : HistoryScroll(new CompactHistoryType(maxLineCount))
    , _cells()
    , _index()
    , _flags()
    , _maxLineCount(0)
{
    setMaxNbLines(maxLineCount);
}

void CompactHistoryScroll::removeFirstLine()
{
    if (_index.size() > 1) {
        _flags.removeFirst();

        const int removing = _index.first();
        _index.removeFirst();
        std::transform(_index.begin(), _index.end(), _index.begin(), [removing](int i) {
            return i - removing;
        });

        _cells.erase(_cells.begin(), _cells.begin() + removing);
    } else {
        _flags.clear();
        _index.clear();
        _cells.clear();
    }
}

void CompactHistoryScroll::addCells(const Character a[], const int count)
{
    std::copy(a, a + count, std::back_inserter(_cells));

    _index.append(_cells.size());
    _flags.append(LINE_DEFAULT);

    if (_index.size() > _maxLineCount) {
        removeFirstLine();
    }
}

void CompactHistoryScroll::addLine(const LineProperty lineProperty)
{
    auto &flag = _flags.last();
    flag = lineProperty;
}

int CompactHistoryScroll::getLines() const
{
    return _index.size();
}

int CompactHistoryScroll::getMaxLines() const
{
    return _maxLineCount;
}

int CompactHistoryScroll::getLineLen(int lineNumber) const
{
    if (lineNumber < 0 || lineNumber >= _index.size()) {
        return 0;
    }

    return lineLen(lineNumber);
}

void CompactHistoryScroll::getCells(const int lineNumber, const int startColumn, const int count, Character buffer[]) const
{
    if (count == 0) {
        return;
    }
    Q_ASSERT(lineNumber < _index.size());

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

    while (_index.size() > lineCount) {
        removeFirstLine();
    }
}

void CompactHistoryScroll::removeCells()
{
    if (_index.size() > 1) {
        _index.removeLast();
        _flags.removeLast();

        _cells.erase(_cells.begin() + _index.last(), _cells.end());
    } else {
        _cells.clear();
        _index.clear();
        _flags.clear();
    }
}

bool CompactHistoryScroll::isWrappedLine(const int lineNumber) const
{
    Q_ASSERT(lineNumber < _index.size());
    return (_flags.at(lineNumber) & LINE_WRAPPED) > 0;
}

LineProperty CompactHistoryScroll::getLineProperty(const int lineNumber) const
{
    Q_ASSERT(lineNumber < _index.size());
    return _flags.at(lineNumber);
}

int CompactHistoryScroll::reflowLines(const int columns)
{
    reflowData newLine;

    auto reflowLineLen = [](int start, int end) {
        return end - start;
    };
    auto setNewLine = [](reflowData &change, int index, LineProperty flag) {
        change.index.append(index);
        change.flags.append(flag);
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
            setNewLine(newLine, startLine, lineProperty | LINE_WRAPPED);
        }
        setNewLine(newLine, endLine, lineProperty & ~LINE_WRAPPED);
        currentPos++;
    }
    _index = std::move(newLine.index);
    _flags = std::move(newLine.flags);

    int deletedLines = 0;
    while (getLines() > _maxLineCount) {
        removeFirstLine();
        ++deletedLines;
    }

    return deletedLines;
}

/*
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

CompactHistoryScroll::CompactHistoryScroll(unsigned int maxLineCount) :
    HistoryScroll(new CompactHistoryType(maxLineCount)),
    _cells(),
    _index(),
    _flags(),
    _maxLineCount(0)
{
    setMaxNbLines(maxLineCount);
}

void CompactHistoryScroll::removeFirstLine()
{
    _flags.pop_front();

    auto removing = _index.first();
    _index.pop_front();
    std::transform(_index.begin(), _index.end(), _index.begin(), [removing](int i) { return i - removing; });

    while (_cells.size() > _index.last()) {
        _cells.pop_front();
    }
}

inline int CompactHistoryScroll::lineLen(int line)
{
    return line == 0 ? _index[0] : _index[line] - _index[line - 1];
}

inline int CompactHistoryScroll::startOfLine(int line)
{
    return line == 0 ? 0 : _index[line - 1];
}

void CompactHistoryScroll::addCells(const Character a[], int count)
{
    std::copy(a, a + count, std::back_inserter(_cells));

    _index.append(_cells.size());
    _flags.append(LINE_DEFAULT);

    if (_index.size() > _maxLineCount) {
        removeFirstLine();
    }
}

void CompactHistoryScroll::addLine(LineProperty lineProperty)
{
    auto &flag = _flags.last();
    flag = lineProperty;
}

int CompactHistoryScroll::getLines()
{
    return _index.size();
}

int CompactHistoryScroll::getMaxLines()
{
    return _maxLineCount;
}

int CompactHistoryScroll::getLineLen(int lineNumber)
{
    if (lineNumber < 0 || lineNumber >= _index.size()) {
        return 0;
    }

    return lineLen(lineNumber);
}

void CompactHistoryScroll::getCells(int lineNumber, int startColumn, int count, Character buffer[])
{
    if (count == 0) {
        return;
    }
    Q_ASSERT(lineNumber < _index.size());

    Q_ASSERT(startColumn >= 0);
    Q_ASSERT(startColumn <= lineLen(lineNumber) - count);

    auto startCopy = _cells.begin() + startOfLine(lineNumber);
    auto endCopy = startCopy + count;
    std::copy(startCopy, endCopy, buffer);
}

void CompactHistoryScroll::setMaxNbLines(int lineCount)
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
        _index.pop_back();
        _flags.pop_back();

        while (_cells.size() > _index.last()) {
            _cells.pop_back();
        }
    } else {
        _cells.clear();
        _index.clear();
        _flags.clear();
    }
}

bool CompactHistoryScroll::isWrappedLine(int lineNumber)
{
    Q_ASSERT(lineNumber < _index.size());
    return _flags[lineNumber] & LINE_WRAPPED;
}

LineProperty CompactHistoryScroll::getLineProperty(int lineNumber)
{
    Q_ASSERT(lineNumber < _index.size());
    return _flags[lineNumber];
}

int CompactHistoryScroll::reflowLines(int columns)
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
        while (isWrappedLine(currentPos)) {
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
    _index = newLine.index;
    _flags = newLine.flags;

    int deletedLines = 0;
    while (getLines() > _maxLineCount) {
        removeFirstLine();
        ++deletedLines;
    }

    return deletedLines;
}

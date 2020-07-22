/*
    This file is part of Konsole, an X terminal.
    Copyright 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own
#include "CompactHistoryScroll.h"

#include "CompactHistoryType.h"

using namespace Konsole;

CompactHistoryScroll::CompactHistoryScroll(unsigned int maxLineCount) :
    HistoryScroll(new CompactHistoryType(maxLineCount)),
    _lines(),
    _blockList(),
    _maxLineCount(0)
{
    ////qDebug() << "scroll of length " << maxLineCount << " created";
    setMaxNbLines(maxLineCount);
}

CompactHistoryScroll::~CompactHistoryScroll()
{
    qDeleteAll(_lines.begin(), _lines.end());
    _lines.clear();
}

void CompactHistoryScroll::addCellsVector(const TextLine &cells)
{
    CompactHistoryLine *line;
    line = new(_blockList) CompactHistoryLine(cells, _blockList);

    if (_lines.size() > static_cast<int>(_maxLineCount)) {
        delete _lines.takeAt(0);
    }
    _lines.append(line);
}

void CompactHistoryScroll::addCells(const Character a[], int count)
{
    TextLine newLine(count);
    std::copy(a, a + count, newLine.begin());
    addCellsVector(newLine);
}

void CompactHistoryScroll::addLine(bool previousWrapped)
{
    CompactHistoryLine *line = _lines.last();
    ////qDebug() << "last line at address " << line;
    line->setWrapped(previousWrapped);
}

int CompactHistoryScroll::getLines()
{
    return _lines.size();
}

int CompactHistoryScroll::getLineLen(int lineNumber)
{
    if ((lineNumber < 0) || (lineNumber >= _lines.size())) {
        //qDebug() << "requested line invalid: 0 < " << lineNumber << " < " <<_lines.size();
        //Q_ASSERT(lineNumber >= 0 && lineNumber < _lines.size());
        return 0;
    }
    CompactHistoryLine *line = _lines[lineNumber];
    ////qDebug() << "request for line at address " << line;
    return line->getLength();
}

void CompactHistoryScroll::getCells(int lineNumber, int startColumn, int count, Character buffer[])
{
    if (count == 0) {
        return;
    }
    Q_ASSERT(lineNumber < _lines.size());
    CompactHistoryLine *line = _lines[lineNumber];
    Q_ASSERT(startColumn >= 0);
    Q_ASSERT(static_cast<unsigned int>(startColumn) <= line->getLength() - count);
    line->getCharacters(buffer, count, startColumn);
}

void CompactHistoryScroll::setMaxNbLines(unsigned int lineCount)
{
    _maxLineCount = lineCount;

    while (_lines.size() > static_cast<int>(lineCount)) {
        delete _lines.takeAt(0);
    }
    ////qDebug() << "set max lines to: " << _maxLineCount;
}

bool CompactHistoryScroll::isWrappedLine(int lineNumber)
{
    Q_ASSERT(lineNumber < _lines.size());
    return _lines[lineNumber]->isWrapped();
}

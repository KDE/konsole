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

#include "HistoryScrollFile.h"

#include "HistoryTypeFile.h"

/*
   The history scroll makes a Row(Row(Cell)) from
   two history buffers. The index buffer contains
   start of line positions which refer to the cells
   buffer.

   Note that index[0] addresses the second line
   (line #1), while the first line (line #0) starts
   at 0 in cells.
*/

using namespace Konsole;

HistoryScrollFile::HistoryScrollFile() :
    HistoryScroll(new HistoryTypeFile())
{
}

HistoryScrollFile::~HistoryScrollFile() = default;

int HistoryScrollFile::getLines()
{
    return _index.len() / sizeof(qint64);
}

int HistoryScrollFile::getLineLen(int lineno)
{
    return (startOfLine(lineno + 1) - startOfLine(lineno)) / sizeof(Character);
}

bool HistoryScrollFile::isWrappedLine(int lineno)
{
    if (lineno >= 0 && lineno <= getLines()) {
        unsigned char flag = 0;
        _lineflags.get(reinterpret_cast<char *>(&flag), sizeof(unsigned char),
                       (lineno)*sizeof(unsigned char));
        return flag != 0u;
    }
    return false;
}

qint64 HistoryScrollFile::startOfLine(int lineno)
{
    if (lineno <= 0) {
        return 0;
    }
    if (lineno <= getLines()) {
        qint64 res = 0;
        _index.get(reinterpret_cast<char*>(&res), sizeof(qint64), (lineno - 1)*sizeof(qint64));
        return res;
    }
    return _cells.len();
}

void HistoryScrollFile::getCells(int lineno, int colno, int count, Character res[])
{
    _cells.get(reinterpret_cast<char*>(res), count * sizeof(Character), startOfLine(lineno) + colno * sizeof(Character));
}

void HistoryScrollFile::addCells(const Character text[], int count)
{
    _cells.add(reinterpret_cast<const char*>(text), count * sizeof(Character));
}

void HistoryScrollFile::addLine(bool previousWrapped)
{
    qint64 locn = _cells.len();
    _index.add(reinterpret_cast<char *>(&locn), sizeof(qint64));
    unsigned char flags = previousWrapped ? 0x01 : 0x00;
    _lineflags.add(reinterpret_cast<char *>(&flags), sizeof(char));
}

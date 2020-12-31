/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
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

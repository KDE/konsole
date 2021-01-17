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

int HistoryScrollFile::getMaxLines()
{
    return getLines();
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

void HistoryScrollFile::insertCells(int, const Character[], int)
{
}

void HistoryScrollFile::removeCells(int)
{
    qint64 res = (getLines() - 2) * sizeof(qint64);
    if (getLines() < 2) {
        _cells.removeLast(0);
    } else {
        _index.get(reinterpret_cast<char *>(&res), sizeof(qint64), res);
        _cells.removeLast(res);
    }
    res = qMax(0, getLines() - 1);
    _index.removeLast(res * sizeof(qint64));
    _lineflags.removeLast(res * sizeof(unsigned char));
}

void HistoryScrollFile::insertCellsVector(int, const QVector<Character> &)
{
}

void HistoryScrollFile::setCellsAt(int, const Character text[], int count)
{
    qint64 res = (getLines() - 2) * sizeof(qint64);
    if (getLines() < 2) {
        _cells.removeLast(0);
    } else {
        _index.get(reinterpret_cast<char *>(&res), sizeof(qint64), res);
        _cells.removeLast(res);
    }
    _cells.add(reinterpret_cast<const char *>(text), count * sizeof(Character));
}

void HistoryScrollFile::setCellsVectorAt(int, const QVector<Character> &)
{
}

int HistoryScrollFile::reflowLines(int columns)
{
    HistoryFile *reflowFile = new HistoryFile;
    reflowData newLine;

    auto reflowLineLen = [] (qint64 start, qint64 end) {
        return (int)((end - start) / sizeof(Character));
    };
    auto setNewLine = [] (reflowData &change, qint64 index, bool lineflag) {
        change.index = index;
        change.lineFlag = lineflag;
    };

    // First all changes are saved on an auxiliary file, no real index is changed
    int currentPos = 0;
    while (currentPos < getLines()) {
        qint64 startLine = startOfLine(currentPos);
        qint64 endLine = startOfLine(currentPos + 1);

        // Join the lines if they are wrapped
        while (isWrappedLine(currentPos)) {
            currentPos++;
            endLine = startOfLine(currentPos + 1);
        }

        // Now reflow the lines
        while (reflowLineLen(startLine, endLine) > columns) {
            startLine += (qint64)columns * sizeof(Character);
            setNewLine(newLine, startLine, true);
            reflowFile->add(reinterpret_cast<const char *>(&newLine), sizeof(reflowData));
        }
        setNewLine(newLine, endLine, false);
        reflowFile->add(reinterpret_cast<const char *>(&newLine), sizeof(reflowData));
        currentPos++;
    }

    // Erase data from index and flag data
    _index.removeLast(0);
    _lineflags.removeLast(0);

    // Now save the new indexes and properties to proper files
    int totalLines = reflowFile->len() / sizeof(reflowData);
    currentPos = 0;
    while (currentPos < totalLines) {
        reflowFile->get(reinterpret_cast<char *>(&newLine), sizeof(reflowData), currentPos * sizeof(reflowData));

        _index.add(reinterpret_cast<char *>(&newLine.index), sizeof(qint64));

        unsigned char flags = newLine.lineFlag ? 0x01 : 0x00;
        _lineflags.add(reinterpret_cast<char *>(&flags), sizeof(char));
        currentPos++;
    }

    delete reflowFile;
    return 0;
}

void HistoryScrollFile::setLineAt(int, bool previousWrapped)
{
    qint64 locn = qMax(0, getLines() - 1);
    _index.removeLast(locn * sizeof(qint64));
    _lineflags.removeLast(locn * sizeof(unsigned char));

    locn = _cells.len();
    _index.add(reinterpret_cast<char *>(&locn), sizeof(qint64));

    unsigned char flags = previousWrapped ? 0x01 : 0x00;
    _lineflags.add(reinterpret_cast<char *>(&flags), sizeof(char));
}

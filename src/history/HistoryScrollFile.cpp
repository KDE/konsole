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

HistoryScrollFile::HistoryScrollFile()
    : HistoryScroll(new HistoryTypeFile())
{
}

HistoryScrollFile::~HistoryScrollFile() = default;

int HistoryScrollFile::getLines() const
{
    return _index.len() / sizeof(qint64);
}

int HistoryScrollFile::getMaxLines() const
{
    return getLines();
}

int HistoryScrollFile::getLineLen(const int lineno) const
{
    return (startOfLine(lineno + 1) - startOfLine(lineno)) / sizeof(Character);
}

bool HistoryScrollFile::isWrappedLine(const int lineno) const
{
    return (getLineProperty(lineno) & LINE_WRAPPED) > 0;
}

LineProperty HistoryScrollFile::getLineProperty(const int lineno) const
{
    if (lineno >= 0 && lineno <= getLines()) {
        LineProperty flag = 0;
        _lineflags.get(reinterpret_cast<char *>(&flag), sizeof(unsigned char), (lineno) * sizeof(unsigned char));
        return flag;
    }
    return 0;
}

qint64 HistoryScrollFile::startOfLine(const int lineno) const
{
    Q_ASSERT(lineno >= 0 && lineno <= getLines());

    if (lineno == 0) {
        return 0;
    }
    if (lineno < getLines()) {
        qint64 res = 0;
        _index.get(reinterpret_cast<char *>(&res), sizeof(qint64), (lineno - 1) * sizeof(qint64));
        return res;
    }
    return _cells.len();
}

void HistoryScrollFile::getCells(const int lineno, const int colno, const int count, Character res[]) const
{
    _cells.get(reinterpret_cast<char *>(res), count * sizeof(Character), startOfLine(lineno) + colno * sizeof(Character));
}

void HistoryScrollFile::addCells(const Character text[], const int count)
{
    _cells.add(reinterpret_cast<const char *>(text), count * sizeof(Character));
}

void HistoryScrollFile::addLine(LineProperty lineProperty)
{
    qint64 locn = _cells.len();
    _index.add(reinterpret_cast<char *>(&locn), sizeof(qint64));
    _lineflags.add(reinterpret_cast<char *>(&lineProperty), sizeof(char));
}

void HistoryScrollFile::removeCells()
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

int Konsole::HistoryScrollFile::reflowLines(const int columns, std::map<int, int> *)
{
    auto reflowFile = std::make_unique<HistoryFile>();
    reflowData newLine;

    auto reflowLineLen = [](qint64 start, qint64 end) {
        return (int)((end - start) / sizeof(Character));
    };
    auto setNewLine = [](reflowData &change, qint64 index, LineProperty lineflag) {
        change.index = index;
        change.lineFlag = lineflag;
    };

    // First all changes are saved on an auxiliary file, no real index is changed
    int currentPos = 0;
    if (getLines() > MAX_REFLOW_LINES) {
        currentPos = getLines() - MAX_REFLOW_LINES;
    }
    while (currentPos < getLines()) {
        qint64 startLine = startOfLine(currentPos);
        qint64 endLine = startOfLine(currentPos + 1);
        LineProperty lineProperty = getLineProperty(currentPos);

        // Join the lines if they are wrapped
        while (currentPos < getLines() - 1 && isWrappedLine(currentPos)) {
            currentPos++;
            endLine = startOfLine(currentPos + 1);
        }

        // Now reflow the lines
        while (reflowLineLen(startLine, endLine) > columns && !(lineProperty & (LINE_DOUBLEHEIGHT_BOTTOM | LINE_DOUBLEHEIGHT_TOP))) {
            startLine += (qint64)columns * sizeof(Character);
            setNewLine(newLine, startLine, lineProperty | LINE_WRAPPED);
            reflowFile->add(reinterpret_cast<const char *>(&newLine), sizeof(reflowData));
        }
        setNewLine(newLine, endLine, lineProperty & ~LINE_WRAPPED);
        reflowFile->add(reinterpret_cast<const char *>(&newLine), sizeof(reflowData));
        currentPos++;
    }

    // Erase data from index and flag data
    if (getLines() > MAX_REFLOW_LINES) {
        currentPos = getLines() - MAX_REFLOW_LINES;
        _index.removeLast(currentPos * sizeof(qint64));
        _lineflags.removeLast(currentPos * sizeof(char));
    } else {
        _index.removeLast(0);
        _lineflags.removeLast(0);
    }

    // Now save the new indexes and properties to proper files
    int totalLines = reflowFile->len() / sizeof(reflowData);
    currentPos = 0;
    while (currentPos < totalLines) {
        reflowFile->get(reinterpret_cast<char *>(&newLine), sizeof(reflowData), currentPos * sizeof(reflowData));

        _lineflags.add(reinterpret_cast<char *>(&newLine.lineFlag), sizeof(char));
        _index.add(reinterpret_cast<char *>(&newLine.index), sizeof(qint64));
        currentPos++;
    }

    return 0;
}

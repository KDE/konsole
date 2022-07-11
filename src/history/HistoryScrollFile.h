/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef HISTORYSCROLLFILE_H
#define HISTORYSCROLLFILE_H

#include "konsoleprivate_export.h"

// History
#include "HistoryFile.h"
#include "HistoryScroll.h"

namespace Konsole
{
//////////////////////////////////////////////////////////////////////
// File-based history (e.g. file log, no limitation in length)
//////////////////////////////////////////////////////////////////////

class KONSOLEPRIVATE_EXPORT HistoryScrollFile : public HistoryScroll
{
public:
    explicit HistoryScrollFile();
    ~HistoryScrollFile() override;

    int getLines() const override;
    int getMaxLines() const override;
    int getLineLen(const int lineno) const override;
    void getCells(const int lineno, const int colno, const int count, Character res[]) const override;
    bool isWrappedLine(const int lineno) const override;
    LineProperty getLineProperty(const int lineno) const override;

    void addCells(const Character text[], const int count) override;
    void addCellsMove(Character text[], const int count) override
    {
        addCells(text, count);
    } // TODO: optimize, if there's any point
    void addLine(LineProperty lineProperty = 0) override;

    // Modify history
    void removeCells() override;
    int reflowLines(const int columns, std::map<int, int> * = nullptr) override;

private:
    qint64 startOfLine(const int lineno) const;

    mutable HistoryFile _index; // lines Row(qint64)
    mutable HistoryFile _cells; // text  Row(Character)
    mutable HistoryFile _lineflags; // flags Row(unsigned char)

    struct reflowData { // data to reflow lines
        qint64 index;
        LineProperty lineFlag;
    };
};

}

#endif

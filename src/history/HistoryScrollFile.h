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

    int  getLines() override;
    int  getMaxLines() override;
    int  getLineLen(int lineno) override;
    void getCells(int lineno, int colno, int count, Character res[]) override;
    bool isWrappedLine(int lineno) override;
    LineProperty getLineProperty(int lineno) override;

    void addCells(const Character text[], int count) override;
    void addLine(LineProperty lineProperty = 0) override;

    // Modify history
    void removeCells() override;
    int reflowLines(int columns) override;

private:
    qint64 startOfLine(int lineno);

    HistoryFile _index; // lines Row(qint64)
    HistoryFile _cells; // text  Row(Character)
    HistoryFile _lineflags; // flags Row(unsigned char)

    struct reflowData { // data to reflow lines
        qint64 index;
        LineProperty lineFlag;
    };
};

}

#endif

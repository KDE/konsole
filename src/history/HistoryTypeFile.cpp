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

#include "HistoryTypeFile.h"
#include "HistoryFile.h"
#include "HistoryScrollFile.h"

using namespace Konsole;

// Reasonable line size
static const int LINE_SIZE = 1024;

HistoryTypeFile::HistoryTypeFile() = default;

bool HistoryTypeFile::isEnabled() const
{
    return true;
}

HistoryScroll *HistoryTypeFile::scroll(HistoryScroll *old) const
{
    if (dynamic_cast<HistoryFile *>(old) != nullptr) {
        return old; // Unchanged.
    }
    HistoryScroll *newScroll = new HistoryScrollFile();

    Character line[LINE_SIZE];
    int lines = (old != nullptr) ? old->getLines() : 0;
    for (int i = 0; i < lines; i++) {
        int size = old->getLineLen(i);
        if (size > LINE_SIZE) {
            auto tmp_line = new Character[size];
            old->getCells(i, 0, size, tmp_line);
            newScroll->addCells(tmp_line, size);
            newScroll->addLine(old->isWrappedLine(i));
            delete [] tmp_line;
        } else {
            old->getCells(i, 0, size, line);
            newScroll->addCells(line, size);
            newScroll->addLine(old->isWrappedLine(i));
        }
    }

    delete old;
    return newScroll;
}

int HistoryTypeFile::maximumLineCount() const
{
    return -1;
}

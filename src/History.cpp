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
#include "History.h"

#include "HistoryScrollFile.h"

#include "konsoledebug.h"
#include "KonsoleSettings.h"

// System
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <sys/types.h>
#include <unistd.h>

// KDE
#include <QDir>
#include <qplatformdefs.h>
#include <QStandardPaths>
#include <KConfigGroup>
#include <KSharedConfig>

// Reasonable line size
static const int LINE_SIZE = 1024;

using namespace Konsole;

/*
   An arbitrary long scroll.

   One can modify the scroll only by adding either cells
   or newlines, but access it randomly.

   The model is that of an arbitrary wide typewriter scroll
   in that the scroll is a series of lines and each line is
   a series of cells with no overwriting permitted.

   The implementation provides arbitrary length and numbers
   of cells and line/column indexed read access to the scroll
   at constant costs.
*/

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

//////////////////////////////

CompactHistoryType::CompactHistoryType(unsigned int nbLines) :
    _maxLines(nbLines)
{
}

bool CompactHistoryType::isEnabled() const
{
    return true;
}

int CompactHistoryType::maximumLineCount() const
{
    return _maxLines;
}

HistoryScroll *CompactHistoryType::scroll(HistoryScroll *old) const
{
    if (old != nullptr) {
        auto *oldBuffer = dynamic_cast<CompactHistoryScroll *>(old);
        if (oldBuffer != nullptr) {
            oldBuffer->setMaxNbLines(_maxLines);
            return oldBuffer;
        }
        delete old;
    }
    return new CompactHistoryScroll(_maxLines);
}

/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "CompactHistoryType.h"

#include "CompactHistoryScroll.h"

using namespace Konsole;

// Reasonable line size
static const int LINE_SIZE = 1024;

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
        auto *newBuffer = dynamic_cast<CompactHistoryScroll *>(old);
        if (newBuffer != nullptr) {
            newBuffer->setMaxNbLines(_maxLines);
            return newBuffer;
        }

        newBuffer = new CompactHistoryScroll(_maxLines);
        
        Character line[LINE_SIZE];
        int lines = old->getLines();
        int i = qMax((lines - (int)_maxLines - 1), 0);
        for (; i < lines; i++) {
            int size = old->getLineLen(i);
            if (size > LINE_SIZE) {
                auto tmp_line = new Character[size];
                old->getCells(i, 0, size, tmp_line);
                newBuffer->addCells(tmp_line, size);
                newBuffer->addLine(old->isWrappedLine(i));
                delete[] tmp_line;
            } else {
                old->getCells(i, 0, size, line);
                newBuffer->addCells(line, size);
                newBuffer->addLine(old->isWrappedLine(i));
            }
        }

        delete old;
        return newBuffer;
    }
    return new CompactHistoryScroll(_maxLines);
}

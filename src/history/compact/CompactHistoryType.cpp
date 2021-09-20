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

CompactHistoryType::CompactHistoryType(unsigned int nbLines)
    : _maxLines(nbLines)
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

void CompactHistoryType::scroll(std::unique_ptr<HistoryScroll> &old) const
{
    if (auto *newBuffer = dynamic_cast<CompactHistoryScroll *>(old.get())) {
        newBuffer->setMaxNbLines(_maxLines);
        return;
    }
    auto newScroll = std::make_unique<CompactHistoryScroll>(_maxLines);

    Character line[LINE_SIZE];
    int lines = (old != nullptr) ? old->getLines() : 0;
    int i = qMax((lines - (int)_maxLines), 0);
    std::vector<Character> tmp_line;
    for (; i < lines; i++) {
        int size = old->getLineLen(i);
        if (size > LINE_SIZE) {
            tmp_line.resize(size);
            old->getCells(i, 0, size, tmp_line.data());
            newScroll->addCellsMove(tmp_line.data(), size);
            newScroll->addLine(old->getLineProperty(i));
        } else {
            old->getCells(i, 0, size, line);
            newScroll->addCells(line, size);
            newScroll->addLine(old->getLineProperty(i));
        }
    }
    old = std::move(newScroll);
}

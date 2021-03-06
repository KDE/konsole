/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
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

void HistoryTypeFile::scroll(std::unique_ptr<HistoryScroll> &old) const
{
    if (dynamic_cast<HistoryFile *>(old.get()) != nullptr) {
        return; // Unchanged.
    }
    auto newScroll = std::make_unique<HistoryScrollFile>();

    Character line[LINE_SIZE];
    int lines = (old != nullptr) ? old->getLines() : 0;
    for (int i = 0; i < lines; i++) {
        int size = old->getLineLen(i);
        if (size > LINE_SIZE) {
            auto tmp_line = std::make_unique<Character[]>(size);
            old->getCells(i, 0, size, tmp_line.get());
            newScroll->addCells(tmp_line.get(), size);
            newScroll->addLine(old->getLineProperty(i));
        } else {
            old->getCells(i, 0, size, line);
            newScroll->addCells(line, size);
            newScroll->addLine(old->getLineProperty(i));
        }
    }

    old = std::move(newScroll);
}

int HistoryTypeFile::maximumLineCount() const
{
    return -1;
}

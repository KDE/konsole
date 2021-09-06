/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "HistoryScrollNone.h"

#include "HistoryTypeNone.h"

using namespace Konsole;

// History Scroll None //////////////////////////////////////

HistoryScrollNone::HistoryScrollNone()
    : HistoryScroll(new HistoryTypeNone())
{
}

HistoryScrollNone::~HistoryScrollNone() = default;

bool HistoryScrollNone::hasScroll() const
{
    return false;
}

int HistoryScrollNone::getLines() const
{
    return 0;
}

int HistoryScrollNone::getMaxLines() const
{
    return 0;
}

int HistoryScrollNone::getLineLen(int) const
{
    return 0;
}

bool HistoryScrollNone::isWrappedLine(int /*lineno*/) const
{
    return false;
}

LineProperty HistoryScrollNone::getLineProperty(int /*lineno*/) const
{
    return 0;
}

void HistoryScrollNone::getCells(int, int, int, Character[]) const
{
}

void HistoryScrollNone::addCells(const Character[], int)
{
}

void HistoryScrollNone::addLine(LineProperty)
{
}

void HistoryScrollNone::removeCells()
{
}

int HistoryScrollNone::reflowLines(int)
{
    return 0;
}

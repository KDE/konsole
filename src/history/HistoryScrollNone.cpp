/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "HistoryScrollNone.h"

#include "HistoryTypeNone.h"

using namespace Konsole;

// History Scroll None //////////////////////////////////////

HistoryScrollNone::HistoryScrollNone() :
    HistoryScroll(new HistoryTypeNone())
{
}

HistoryScrollNone::~HistoryScrollNone() = default;

bool HistoryScrollNone::hasScroll()
{
    return false;
}

int HistoryScrollNone::getLines()
{
    return 0;
}

int HistoryScrollNone::getMaxLines()
{
    return 0;
}

int HistoryScrollNone::getLineLen(int)
{
    return 0;
}

bool HistoryScrollNone::isWrappedLine(int /*lineno*/)
{
    return false;
}

LineProperty HistoryScrollNone::getLineProperty(int /*lineno*/)
{
    return 0;
}

void HistoryScrollNone::getCells(int, int, int, Character [])
{
}

void HistoryScrollNone::addCells(const Character [], int)
{
}

void HistoryScrollNone::addLine(LineProperty)
{
}

void HistoryScrollNone::insertCells(int , const Character [], int)
{
}

void HistoryScrollNone::removeCells(int )
{
}

void HistoryScrollNone::insertCellsVector(int , const QVector<Character> &)
{
}

void HistoryScrollNone::setCellsAt(int , const Character [], int)
{
}

void HistoryScrollNone::setCellsVectorAt(int , const QVector<Character> &)
{
}

int HistoryScrollNone::reflowLines(int)
{
    return 0;
}

void HistoryScrollNone::setLineAt(int , LineProperty)
{
}

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

#ifndef HISTORY_H
#define HISTORY_H

// System
#include <sys/mman.h>

// Qt
#include <QList>
#include <QVector>
#include <QTemporaryFile>

#include "konsoleprivate_export.h"

// History
#include "HistoryFile.h"
#include "HistoryScroll.h"
#include "HistoryScrollFile.h"
#include "HistoryScrollNone.h"
#include "CharacterFormat.h"
#include "CompactHistoryBlock.h"
#include "CompactHistoryBlockList.h"
#include "CompactHistoryLine.h"
#include "CompactHistoryScroll.h"

#include "HistoryType.h"
#include "HistoryTypeNone.h"

// Konsole
#include "Character.h"

namespace Konsole {

//////////////////////////////////////////////////////////////////////
// History using compact storage
// This implementation uses a list of fixed-sized blocks
// where history lines are allocated in (avoids heap fragmentation)
//////////////////////////////////////////////////////////////////////

class KONSOLEPRIVATE_EXPORT HistoryTypeFile : public HistoryType
{
public:
    explicit HistoryTypeFile();

    bool isEnabled() const override;
    int maximumLineCount() const override;

    HistoryScroll *scroll(HistoryScroll *) const override;
};

class KONSOLEPRIVATE_EXPORT CompactHistoryType : public HistoryType
{
public:
    explicit CompactHistoryType(unsigned int nbLines);

    bool isEnabled() const override;
    int maximumLineCount() const override;

    HistoryScroll *scroll(HistoryScroll *) const override;

protected:
    unsigned int _maxLines;
};
}

#endif // HISTORY_H

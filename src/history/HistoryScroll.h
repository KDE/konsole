/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef HISTORYSCROLL_H
#define HISTORYSCROLL_H

// STD
#include <memory>

#include "konsoleprivate_export.h"

// Konsole
#include "../characters/Character.h"

// Qt
#include <QVector>

namespace Konsole
{
//////////////////////////////////////////////////////////////////////
// Abstract base class for file and buffer versions
//////////////////////////////////////////////////////////////////////
class HistoryType;

class KONSOLEPRIVATE_EXPORT HistoryScroll
{
public:
    explicit HistoryScroll(HistoryType *);
    virtual ~HistoryScroll();

    virtual bool hasScroll() const;

    // access to history
    virtual int getLines() const = 0;
    virtual int getMaxLines() const = 0;
    virtual int getLineLen(const int lineno) const = 0;
    virtual void getCells(const int lineno, const int colno, const int count, Character res[]) const = 0;
    virtual bool isWrappedLine(const int lineNumber) const = 0;
    virtual LineProperty getLineProperty(const int lineno) const = 0;

    // adding lines.
    virtual void addCells(const Character a[], const int count) = 0;
    virtual void addCellsMove(Character a[], const int count) = 0;
    // convenience method - this is virtual so that subclasses can take advantage
    // of QVector's implicit copying
    virtual void addCellsVector(const QVector<Character> &cells)
    {
        addCells(cells.data(), cells.size());
    }

    virtual void addLine(const LineProperty lineProperty = 0) = 0;

    // modify history
    virtual void removeCells() = 0;
    virtual int reflowLines(const int columns, std::map<int, int> *deltas = nullptr) = 0;

    //
    // FIXME:  Passing around constant references to HistoryType instances
    // is very unsafe, because those references will no longer
    // be valid if the history scroll is deleted.
    //
    const HistoryType &getType() const
    {
        return *_historyType;
    }

protected:
    std::unique_ptr<HistoryType> _historyType;
    const int MAX_REFLOW_LINES = 20000;
};

}

#endif

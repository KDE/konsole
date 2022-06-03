/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Filter.h"

#include "HotSpot.h"

using namespace Konsole;

Filter::Filter()
    : _linePositions(nullptr)
    , _buffer(nullptr)
{
}

Filter::~Filter()
{
    reset();
}

void Filter::reset()
{
    _hotspots.clear();
    _hotspotList.clear();
}

void Filter::setBuffer(const QString *buffer, const QList<int> *linePositions)
{
    _buffer = buffer;
    _linePositions = linePositions;
}

std::pair<int, int> Filter::getLineColumn(int prevline, int position)
{
    Q_ASSERT(_linePositions);
    Q_ASSERT(_buffer);

    for (int i = prevline; i < _linePositions->count(); i++) {
        const int nextLine = i == _linePositions->count() - 1 ? _buffer->length() + 1 : _linePositions->value(i + 1);

        if (_linePositions->value(i) <= position && position < nextLine) {
            return {i, Character::stringWidth(buffer()->mid(_linePositions->value(i), position - _linePositions->value(i)))};
        }
    }
    return {-1, -1};
}

const QString *Filter::buffer()
{
    return _buffer;
}

void Filter::addHotSpot(QSharedPointer<HotSpot> spot)
{
    _hotspotList << spot;

    for (int line = spot->startLine(); line <= spot->endLine(); line++) {
        _hotspots.insert(line, spot);
    }
}

QList<QSharedPointer<HotSpot>> Filter::hotSpots() const
{
    return _hotspotList;
}

QSharedPointer<HotSpot> Filter::hotSpotAt(int line, int column) const
{
    const auto hotspots = _hotspots.values(line);

    for (auto &spot : hotspots) {
        if (spot->startLine() == line && spot->startColumn() > column) {
            continue;
        }
        if (spot->endLine() == line && spot->endColumn() < column) {
            continue;
        }

        return spot;
    }

    return nullptr;
}

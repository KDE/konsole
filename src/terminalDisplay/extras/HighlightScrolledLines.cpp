/*
    SPDX-FileCopyrightText: 2020-2020 Carlos Alves <cbcalves@gmail.com>
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "HighlightScrolledLines.h"

// Konsole
#include "../TerminalScrollBar.h"

namespace Konsole
{
HighlightScrolledLines::HighlightScrolledLines()
    : _enabled(false)
    , _previousScrollCount(0)
    , _timer(nullptr)
    , _needToClear(false)
{
}

HighlightScrolledLines::~HighlightScrolledLines()
{
}

bool HighlightScrolledLines::isEnabled()
{
    return _enabled;
}

void HighlightScrolledLines::setEnabled(bool enable)
{
    _enabled = enable;
}

bool HighlightScrolledLines::isNeedToClear()
{
    return _needToClear;
}

void HighlightScrolledLines::setNeedToClear(bool isNeeded)
{
    _needToClear = isNeeded;
}

int HighlightScrolledLines::getPreviousScrollCount()
{
    return _previousScrollCount;
}

void HighlightScrolledLines::setPreviousScrollCount(int scrollCount)
{
    _previousScrollCount = scrollCount;
}

void HighlightScrolledLines::setTimer(TerminalScrollBar *parent)
{
    if (_enabled && _timer == nullptr) {
        _timer = std::make_unique<QTimer>();
        _timer->setSingleShot(true);
        _timer->setInterval(250);
        _timer->connect(_timer.get(), &QTimer::timeout, parent, &TerminalScrollBar::highlightScrolledLinesEvent);
    }
}

void HighlightScrolledLines::startTimer()
{
    _timer->start();
}

bool HighlightScrolledLines::isTimerActive()
{
    return _timer->isActive();
}

QRect &HighlightScrolledLines::rect()
{
    return _rect;
}
} // namespace Konsole

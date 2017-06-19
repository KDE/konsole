/*
    Copyright 2015 by Lindsay Roberts <os@lindsayr.com>

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

#include "ScrollState.h"

#include <QWheelEvent>

using namespace Konsole;

void ScrollState::addWheelEvent(const QWheelEvent *wheel)
{
    if ((wheel->angleDelta().y() != 0) && (wheel->pixelDelta().y() == 0)) {
        _remainingScrollPixel = 0;
    } else {
        _remainingScrollPixel += wheel->pixelDelta().y();
    }
    _remainingScrollAngle += wheel->angleDelta().y();
}

void ScrollState::clearAll()
{
    _remainingScrollPixel = 0;
    _remainingScrollAngle = 0;
}

int ScrollState::consumeLegacySteps(int stepsize)
{
    int steps = _remainingScrollAngle / stepsize;
    _remainingScrollAngle -= steps * stepsize;
    if (steps != 0) {
        _remainingScrollPixel = 0;
    }
    return steps;
}

int ScrollState::consumeSteps(int pixelStepSize, int angleStepSize)
{
    if (_remainingScrollPixel != 0) {
        int steps = _remainingScrollPixel / pixelStepSize;
        _remainingScrollPixel -= steps * pixelStepSize;
        _remainingScrollAngle = 0;
        return steps;
    }
    int steps = _remainingScrollAngle / angleStepSize;
    _remainingScrollAngle -= steps * angleStepSize;
    _remainingScrollPixel = 0;
    return steps;
}

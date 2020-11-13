/*
    SPDX-FileCopyrightText: 2015 Lindsay Roberts <os@lindsayr.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
    if (stepsize < 1) {
        stepsize = DEFAULT_ANGLE_SCROLL_LINE;
    }
    const int steps = _remainingScrollAngle / stepsize;
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

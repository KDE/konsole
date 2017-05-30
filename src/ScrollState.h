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

#ifndef SCROLLSTATE_H
#define SCROLLSTATE_H

class QWheelEvent;

namespace Konsole {
/**
 * Represents accumulation of wheel scroll from scroll events.
 *
 * Modern high precision scroll events supply many smaller events
 * that may or may not translate into a UI action to support smooth
 * pixel level scrolling. Builtin classes such as QScrollBar
 * support these events under Qt5, but custom code
 * written to handle scroll events in other ways must be modified
 * to accumulate small deltas and act when suitable thresholds have
 * been reached (ideally 1 for pixel scroll values towards any action
 * that can be mapped to a pixel movement).
 */
struct ScrollState
{
    enum {
        DEFAULT_ANGLE_SCROLL_LINE = 120
    };
    enum {
        DEGREES_PER_ANGLE_UNIT = 8
    };
    static inline int degreesToAngle(const int angle)
    {
        return angle * DEGREES_PER_ANGLE_UNIT;
    }

    int angle() const
    {
        return _remainingScrollAngle;
    }

    int pixel() const
    {
        return _remainingScrollPixel;
    }

    /** Add scroll values from a QWheelEvent to the accumulated totals */
    void addWheelEvent(const QWheelEvent *wheel);

    /** Clear all - used when scroll is consumed by another class like QScrollBar */
    void clearAll();
    /** Return the (signed) multiple of @p stepsize available and subtract it from
     * accumulated totals. Also clears accumulated pixel scroll. */
    int consumeLegacySteps(int stepsize);
    /** Return the (signed) multiple of @p pixelStepSize if any pixel scroll is
     * available - that is, if pixel scroll is being supplied, or the same from
     * the @p angleStepSize else. The corresponding value is subtracted from
     * the accumulated total. The other scroll style value is cleared. */
    int consumeSteps(int pixelStepSize, int angleStepSize);

    ScrollState() : _remainingScrollAngle(0),
        _remainingScrollPixel(0)
    {
    }

    int _remainingScrollAngle;
    int _remainingScrollPixel;
};
}

#endif // SCROLLSTATE_H

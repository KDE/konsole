/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 2020 by Tomaz Canabrava <tcanabrava@gmail.com>

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

#ifndef TERMINAL_IMAGE_FILTER_CHAIN
#define TERMINAL_IMAGE_FILTER_CHAIN

#include <memory>
#include <QString>

#include "FilterChain.h"
#include "../characters/Character.h"

namespace Konsole {
class TerminalDisplay;

/** A filter chain which processes character images from terminal displays */
class TerminalImageFilterChain : public FilterChain
{
public:
    explicit TerminalImageFilterChain(TerminalDisplay *terminalDisplay);
    ~TerminalImageFilterChain() override;

    /**
     * Set the current terminal image to @p image.
     *
     * @param image The terminal image
     * @param lines The number of lines in the terminal image
     * @param columns The number of columns in the terminal image
     * @param lineProperties The line properties to set for image
     */
    void setImage(const Character * const image, int lines, int columns,
                  const QVector<LineProperty> &lineProperties);

private:
    Q_DISABLE_COPY(TerminalImageFilterChain)

/* usually QStrings and QLists are not supposed to be in the heap, here we have a problem:
    we need a shared memory space between many filter objeccts, defined by this TerminalImage. */
    std::unique_ptr<QString> _buffer;
    std::unique_ptr<QList<int>> _linePositions;
};

}
#endif

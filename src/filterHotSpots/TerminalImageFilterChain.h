/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TERMINAL_IMAGE_FILTER_CHAIN
#define TERMINAL_IMAGE_FILTER_CHAIN

#include <QString>
#include <memory>

#include "../characters/Character.h"
#include "FilterChain.h"

namespace Konsole
{
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
    void setImage(const Character *const image, int lines, int columns, const QVector<LineProperty> &lineProperties);

private:
    Q_DISABLE_COPY(TerminalImageFilterChain)

    /* usually QStrings and QLists are not supposed to be in the heap, here we have a problem:
        we need a shared memory space between many filter objeccts, defined by this TerminalImage. */
    std::unique_ptr<QString> _buffer;
    std::unique_ptr<QList<int>> _linePositions;
};

}
#endif

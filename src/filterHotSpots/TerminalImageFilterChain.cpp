/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "TerminalImageFilterChain.h"
#include "profile/Profile.h"

#include <QTextStream>

#include "../decoders/PlainTextDecoder.h"

#include "terminalDisplay/TerminalDisplay.h"

using namespace Konsole;

TerminalImageFilterChain::TerminalImageFilterChain(TerminalDisplay *terminalDisplay)
    : FilterChain(terminalDisplay)
    , _buffer(nullptr)
    , _linePositions(nullptr)
{
}

TerminalImageFilterChain::~TerminalImageFilterChain() = default;

void TerminalImageFilterChain::setImage(const Character *const image, int lines, int columns, const QVector<LineProperty> &lineProperties)
{
    if (_filters.empty()) {
        return;
    }

    // reset all filters and hotspots
    reset();

    PlainTextDecoder decoder;

    // setup new shared buffers for the filters to process on
    _buffer.reset(new QString());
    _linePositions.reset(new QList<int>());

    setBuffer(_buffer.get(), _linePositions.get());

    QTextStream lineStream(_buffer.get());
    decoder.begin(&lineStream);

    for (int i = 0; i < lines; i++) {
        const int lineStart = _buffer->length();
        _linePositions->append(lineStart);
        decoder.decodeLine(image + i * columns, columns, LineProperty());
        const int decodedLength = _buffer->length() - lineStart;

        // A line ends with a real newline ('\n') only when it is not soft-wrapped.
        // Soft-wrap happens in two cases:
        //   1. The terminal auto-wrapped (wrapped flag set by Screen).
        //   2. The application did its own word-wrap by inserting '\n' at exactly
        //      the terminal width — these lines fill all columns with real content.
        // In both cases we suppress '\n' so that URL detection can span the boundary.
        const bool terminalWrapped = (lineProperties.value(i, LineProperty()).flags.f.wrapped) != 0;
        const bool appWordWrapped = !terminalWrapped && (decodedLength == columns);
        if (!terminalWrapped && !appWordWrapped) {
            lineStream << QLatin1Char('\n');
        }
    }
    decoder.end();
}

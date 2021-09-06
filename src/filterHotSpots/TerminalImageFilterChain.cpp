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
    decoder.setLeadingWhitespace(true);
    decoder.setTrailingWhitespace(true);

    // setup new shared buffers for the filters to process on
    _buffer.reset(new QString());
    _linePositions.reset(new QList<int>());

    setBuffer(_buffer.get(), _linePositions.get());

    QTextStream lineStream(_buffer.get());
    decoder.begin(&lineStream);

    for (int i = 0; i < lines; i++) {
        _linePositions->append(_buffer->length());
        decoder.decodeLine(image + i * columns, columns, LINE_DEFAULT);

        // pretend that each line ends with a newline character.
        // this prevents a link that occurs at the end of one line
        // being treated as part of a link that occurs at the start of the next line
        //
        // the downside is that links which are spread over more than one line are not
        // highlighted.
        //
        // TODO - Use the "line wrapped" attribute associated with lines in a
        // terminal image to avoid adding this imaginary character for wrapped
        // lines
        if ((lineProperties.value(i, LINE_DEFAULT) & LINE_WRAPPED) == 0) {
            lineStream << QLatin1Char('\n');
        }
    }
    decoder.end();
}

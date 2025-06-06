/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PLAINTEXTDECODER_H
#define PLAINTEXTDECODER_H

// Konsole decoders
#include "TerminalCharacterDecoder.h"

class QTextStream;
template<typename>
class QList;

namespace Konsole
{
/**
 * A terminal character decoder which produces plain text, ignoring colors and other appearance-related
 * properties of the original characters.
 */
class PlainTextDecoder : public TerminalCharacterDecoder
{
public:
    PlainTextDecoder();

    /**
     * Returns of character positions in the output stream
     * at which new lines where added.  Returns an empty if setTrackLinePositions() is false or if
     * the output device is not a string.
     */
    QList<int> linePositions() const;
    /** Enables recording of character positions at which new lines are added.  See linePositions() */
    void setRecordLinePositions(bool record);

    void begin(QTextStream *output) override;
    void end() override;

    void decodeLine(const Character *const characters, int count, LineProperty properties) override;

private:
    QTextStream *_output;

    bool _recordLinePositions;
    QList<int> _linePositions;
};
}

#endif

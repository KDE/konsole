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
class KONSOLEDECODERS_EXPORT PlainTextDecoder : public TerminalCharacterDecoder
{
public:
    PlainTextDecoder();

    /**
     * Set whether leading whitespace at the end of lines should be included
     * in the output.
     * Defaults to true.
     */
    void setLeadingWhitespace(bool enable);
    /**
     * Set whether trailing whitespace at the end of lines should be included
     * in the output.
     * Defaults to true.
     */
    void setTrailingWhitespace(bool enable);
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
    bool _includeLeadingWhitespace;
    bool _includeTrailingWhitespace;

    bool _recordLinePositions;
    QList<int> _linePositions;
};
}

#endif

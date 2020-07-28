/*
    This file is part of Konsole, an X terminal.

    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

#ifndef PLAINTEXTDECODER_H
#define PLAINTEXTDECODER_H

#include "konsoleprivate_export.h"

#include "TerminalCharacterDecoder.h"

class QTextStream;
template<typename> class QList;

namespace Konsole
{
    /**
     * A terminal character decoder which produces plain text, ignoring colors and other appearance-related
     * properties of the original characters.
     */
    class KONSOLEPRIVATE_EXPORT PlainTextDecoder : public TerminalCharacterDecoder
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

        void decodeLine(const Character * const characters, int count,
                        LineProperty properties) override;

    private:
        QTextStream *_output;
        bool _includeLeadingWhitespace;
        bool _includeTrailingWhitespace;

        bool _recordLinePositions;
        QList<int> _linePositions;
    };
}

#endif

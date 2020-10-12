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

#ifndef TERMINAL_CHARACTER_DECODER_H
#define TERMINAL_CHARACTER_DECODER_H

// Qt
#include <QList>
#include <QExplicitlySharedDataPointer>

// Konsole
#include "../characters/Character.h"
#include "konsoleprivate_export.h"

class QTextStream;

namespace Konsole {
class Profile;

/**
 * Base class for terminal character decoders
 *
 * The decoder converts lines of terminal characters which consist of a unicode character, foreground
 * and background colors and other appearance-related properties into text strings.
 *
 * Derived classes may produce either plain text with no other color or appearance information, or
 * they may produce text which incorporates these additional properties.
 */
class KONSOLEPRIVATE_EXPORT TerminalCharacterDecoder
{
public:
    virtual ~TerminalCharacterDecoder()
    {
    }

    /** Begin decoding characters.  The resulting text is appended to @p output. */
    virtual void begin(QTextStream *output) = 0;
    /** End decoding. */
    virtual void end() = 0;

    /**
     * Converts a line of terminal characters with associated properties into a text string
     * and writes the string into an output QTextStream.
     *
     * @param characters An array of characters of length @p count.
     * @param count The number of characters
     * @param properties Additional properties which affect all characters in the line
     */
    virtual void decodeLine(const Character * const characters, int count,
                            LineProperty properties) = 0;
};

}

#endif

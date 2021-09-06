/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TERMINAL_CHARACTER_DECODER_H
#define TERMINAL_CHARACTER_DECODER_H

// Qt
#include <QExplicitlySharedDataPointer>
#include <QList>

// Konsole characters
#include <Character.h>

// Konsole decoders
#include "konsoledecoders_export.h"

class QTextStream;

namespace Konsole
{
/**
 * Base class for terminal character decoders
 *
 * The decoder converts lines of terminal characters which consist of a unicode character, foreground
 * and background colors and other appearance-related properties into text strings.
 *
 * Derived classes may produce either plain text with no other color or appearance information, or
 * they may produce text which incorporates these additional properties.
 */
class KONSOLEDECODERS_EXPORT TerminalCharacterDecoder
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
    virtual void decodeLine(const Character *const characters, int count, LineProperty properties) = 0;
};

}

#endif

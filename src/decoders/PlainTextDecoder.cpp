/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "PlainTextDecoder.h"

// Konsole characters
#include <ExtendedCharTable.h>

// Qt
#include <QList>
#include <QTextStream>

using namespace Konsole;

PlainTextDecoder::PlainTextDecoder()
    : _output(nullptr)
    , _includeLeadingWhitespace(true)
    , _includeTrailingWhitespace(true)
    , _recordLinePositions(false)
    , _linePositions(QList<int>())
{
}

void PlainTextDecoder::setLeadingWhitespace(bool enable)
{
    _includeLeadingWhitespace = enable;
}

void PlainTextDecoder::setTrailingWhitespace(bool enable)
{
    _includeTrailingWhitespace = enable;
}

void PlainTextDecoder::begin(QTextStream *output)
{
    _output = output;
    if (!_linePositions.isEmpty()) {
        _linePositions.clear();
    }
}

void PlainTextDecoder::end()
{
    _output = nullptr;
}

void PlainTextDecoder::setRecordLinePositions(bool record)
{
    _recordLinePositions = record;
}

QList<int> PlainTextDecoder::linePositions() const
{
    return _linePositions;
}

void PlainTextDecoder::decodeLine(const Character *const characters, int count, LineProperty /*properties*/)
{
    Q_ASSERT(_output);

    if (_recordLinePositions && (_output->string() != nullptr)) {
        int pos = _output->string()->count();
        _linePositions << pos;
    }

    // TODO should we ignore or respect the LINE_WRAPPED line property?

    // If we should remove leading whitespace find the first non-space character
    int start = 0;
    if (!_includeLeadingWhitespace) {
        while (start < count && characters[start].isSpace()) {
            start++;
        }
    }

    int outputCount = count - start;

    if (outputCount <= 0) {
        return;
    }

    // if inclusion of trailing whitespace is disabled then find the end of the
    // line
    if (!_includeTrailingWhitespace) {
        while (outputCount > 0 && characters[start + outputCount - 1].isSpace()) {
            outputCount--;
        }
    }

    // find out the last technically real character in the line
    int realCharacterGuard = -1;
    for (int i = count - 1; i >= start; i--) {
        // FIXME: the special case of '\n' here is really ugly
        // Maybe the '\n' should be added after calling this method in
        // Screen::copyLineToStream()
        if (characters[i].isRealCharacter && characters[i].character != '\n') {
            realCharacterGuard = i;
            break;
        }
    }

    // note:  we build up a QVector<uint> and send it to the text stream transformed into a QString
    // rather than writing into the text stream a character at a time because it is more efficient.
    //(since QTextStream always deals with QStrings internally anyway)
    QVector<uint> characterBuffer;
    characterBuffer.reserve(count);

    for (int i = start; i < outputCount;) {
        if ((characters[i].rendition & RE_EXTENDED_CHAR) != 0) {
            ushort extendedCharLength = 0;
            const uint *chars = ExtendedCharTable::instance.lookupExtendedChar(characters[i].character, extendedCharLength);
            if (chars != nullptr) {
                for (uint nchar = 0; nchar < extendedCharLength; nchar++) {
                    characterBuffer.append(chars[nchar]);
                }
                i += qMax(1, Character::stringWidth(chars, extendedCharLength));
            } else {
                ++i;
            }
        } else {
            // All characters which appear before the last real character are
            // seen as real characters, even when they are technically marked as
            // non-real.
            //
            // This feels tricky, but otherwise leading "whitespaces" may be
            // lost in some situation. One typical example is copying the result
            // of `dialog --infobox "qwe" 10 10` .
            if (characters[i].isRealCharacter || i <= realCharacterGuard) {
                characterBuffer.append(characters[i].character);
                i += qMax(1, Character::stringWidth(&characters[i].character, 1));
            } else {
                ++i; // should we 'break' directly here?
            }
        }
    }
    *_output << QString::fromUcs4(characterBuffer.data(), characterBuffer.size());
}

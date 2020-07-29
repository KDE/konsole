/*
    This source file is part of Konsole, a terminal emulator.

    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>

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

#ifndef KEYBOARDTRANSLATORWRITER_H
#define KEYBOARDTRANSLATORWRITER_H

#include "KeyboardTranslator.h"

namespace Konsole
{
    /** Writes a keyboard translation to disk. */
    class KeyboardTranslatorWriter
    {
    public:
        /**
         * Constructs a new writer which saves data into @p destination.
         * The caller is responsible for closing the device when writing is complete.
         */
        explicit KeyboardTranslatorWriter(QIODevice *destination);
        ~KeyboardTranslatorWriter();

        KeyboardTranslatorWriter(const KeyboardTranslatorWriter &) = delete;
        KeyboardTranslatorWriter &operator=(const KeyboardTranslatorWriter &) = delete;

        /**
         * Writes the header for the keyboard translator.
         * @param description Description of the keyboard translator.
         */
        void writeHeader(const QString &description);
        /** Writes a translator entry. */
        void writeEntry(const KeyboardTranslator::Entry &entry);

    private:
        QIODevice *_destination;
        QTextStream *_writer;
    };
}

#endif

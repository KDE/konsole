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

#ifndef KEYBOARDTRANSLATORREADER_H
#define KEYBOARDTRANSLATORREADER_H

// Konsole
#include "KeyboardTranslator.h"

class QIODevice;

namespace Konsole
{
    /**
     * Parses the contents of a Keyboard Translator (.keytab) file and
     * returns the entries found in it.
     *
     * Usage example:
     *
     * @code
     *  QFile source( "/path/to/keytab" );
     *  source.open( QIODevice::ReadOnly );
     *
     *  KeyboardTranslator* translator = new KeyboardTranslator( "name-of-translator" );
     *
     *  KeyboardTranslatorReader reader(source);
     *  while ( reader.hasNextEntry() )
     *      translator->addEntry(reader.nextEntry());
     *
     *  source.close();
     *
     *  if ( !reader.parseError() )
     *  {
     *      // parsing succeeded, do something with the translator
     *  }
     *  else
     *  {
     *      // parsing failed
     *  }
     * @endcode
     */
    class KONSOLEPRIVATE_EXPORT KeyboardTranslatorReader
    {
    public:
        /** Constructs a new reader which parses the given @p source */
        explicit KeyboardTranslatorReader(QIODevice *source);
        ~KeyboardTranslatorReader();

        KeyboardTranslatorReader(const KeyboardTranslatorReader &) = delete;
        KeyboardTranslatorReader &operator=(const KeyboardTranslatorReader &) = delete;

        /**
         * Returns the description text.
         * TODO: More documentation
         */
        QString description() const;

        /** Returns true if there is another entry in the source stream */
        bool hasNextEntry() const;
        /** Returns the next entry found in the source stream */
        KeyboardTranslator::Entry nextEntry();

        /**
         * Returns true if an error occurred whilst parsing the input or
         * false if no error occurred.
         */
        bool parseError();

        /**
         * Parses a condition and result string for a translator entry
         * and produces a keyboard translator entry.
         *
         * The condition and result strings are in the same format as in
         */
        static KeyboardTranslator::Entry createEntry(const QString &condition, const QString &result);
    private:
        struct Token {
            enum Type {
                TitleKeyword,
                TitleText,
                KeyKeyword,
                KeySequence,
                Command,
                OutputText
            };
            Type type;
            QString text;
        };
        QList<Token> tokenize(const QString &);
        void readNext();
        bool decodeSequence(const QString &, int &keyCode, Qt::KeyboardModifiers &modifiers,
                            Qt::KeyboardModifiers &modifierMask, KeyboardTranslator::States &flags,
                            KeyboardTranslator::States &flagMask);

        static bool parseAsModifier(const QString &item, Qt::KeyboardModifier &modifier);
        static bool parseAsStateFlag(const QString &item, KeyboardTranslator::State &flag);
        static bool parseAsKeyCode(const QString &item, int &keyCode);
        static bool parseAsCommand(const QString &text, KeyboardTranslator::Command &command);

        QIODevice *_source;
        QString _description;
        KeyboardTranslator::Entry _nextEntry;
        bool _hasNext;
    };

}

#endif

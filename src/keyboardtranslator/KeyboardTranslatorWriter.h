/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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

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

// Own
#include "KeyboardTranslatorWriter.h"

// Qt
#include <QIODevice>
#include <QTextStream>

using namespace Konsole;

KeyboardTranslatorWriter::KeyboardTranslatorWriter(QIODevice *destination) :
    _destination(destination),
    _writer(nullptr)
{
    Q_ASSERT(destination && destination->isWritable());

    _writer = new QTextStream(_destination);
}

KeyboardTranslatorWriter::~KeyboardTranslatorWriter()
{
    delete _writer;
}

void KeyboardTranslatorWriter::writeHeader(const QString &description)
{
    *_writer << "keyboard \"" << description << '\"' << '\n';
}

void KeyboardTranslatorWriter::writeEntry(const KeyboardTranslator::Entry &entry)
{
    QString result;
    if (entry.command() != KeyboardTranslator::NoCommand) {
        result = entry.resultToString();
    } else {
        result = QLatin1Char('\"') + entry.resultToString() + QLatin1Char('\"');
    }

    *_writer << "key " << entry.conditionToString() << " : " << result << '\n';
}

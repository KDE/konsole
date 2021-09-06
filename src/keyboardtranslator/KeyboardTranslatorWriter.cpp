/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "KeyboardTranslatorWriter.h"

// Qt
#include <QIODevice>
#include <QTextStream>

using namespace Konsole;

KeyboardTranslatorWriter::KeyboardTranslatorWriter(QIODevice *destination)
    : _destination(destination)
    , _writer(nullptr)
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

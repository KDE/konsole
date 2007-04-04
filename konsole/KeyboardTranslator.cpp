/*
    This source file is part of Konsole, a terminal emulator.

    Copyright (C) 2007 by Robert Knight <robertknight@gmail.com>

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

// System
#include <stdio.h>

// Qt
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QtDebug>

// KDE
#include <KStandardDirs>

// Konsole
#include "KeyboardTranslator.h"

KeyboardTranslatorManager::KeyboardTranslatorManager()
{
    findTranslators();
}

void KeyboardTranslatorManager::findTranslators()
{
    Q_ASSERT( _translators.count() == 0 );

    QStringList list = KGlobal::dirs()->findAllResources("data","konsole/*.keytab");

    qDebug() << __FUNCTION__ << ": found " << list.count() << " keyboard translators.";

    // add the name of each translator to the list and associated
    // the name with a null pointer to indicate that the translator
    // has not yet been loaded from disk
    QStringListIterator listIter(list);
    while (listIter.hasNext())
    {
        QString translatorPath = listIter.next();

        QString name = QFileInfo(translatorPath).baseName();
        
        _paths.insert(name,translatorPath);
        _translators.insert(name,0);
    }
}

const KeyboardTranslator* KeyboardTranslatorManager::findTranslator(const QString& name)
{
    if ( _translators.contains(name) )
    {
        KeyboardTranslator* translator = _translators[name];

        // if the translator with this name has not yet been loaded, then load
        // it first.
        if ( translator == 0 )
        {
            translator = loadTranslator(name);

            if ( translator )
                _translators[name] = translator;
            else
                return 0; // an error occurred whilst loading the translator
        }

       return translator; 
    }
    else
    {
        return 0;
    }
}

KeyboardTranslator* KeyboardTranslatorManager::loadTranslator(const QString& name)
{
    Q_ASSERT( _paths.contains(name) );
    Q_ASSERT( _translators.contains(name) && _translators[name] == 0 );

    KeyboardTranslator* translator = new KeyboardTranslator(name);

    QFile source(_paths[name]); // TODO get correct path here
    source.open(QIODevice::ReadOnly);

    KeyboardTranslatorReader reader(&source);
    while ( reader.hasNextEntry() )
        translator->addEntry(reader.nextEntry());

    source.close();

    if ( !reader.parseError() )
    {
        return translator;
    }
    else
    {
        delete translator;
        return 0;
    }
}


// each line of the keyboard translation file is one of:
//
// - keyboard "name"
// - key KeySequence : "characters"
// - key KeySequence : CommandName
//
// KeySequence begins with the name of the key ( taken from the Qt::Key enum )
// and is followed by the keyboard modifiers and state flags ( with + or - in front
// of each modifier or flag to indicate whether it is required ).  All keyboard modifiers
// and flags are optional, if a particular modifier or state is not specified it is 
// assumed not to be a part of the sequence.  The key sequence may contain whitespace
//
// eg:  "key Up+Shift : scrollLineUp"
//      "key Next-Shift : "\E[6~"
//
// (lines containing only whitespace are ignored, parseLine assumes that comments have
// already been removed)
//



QList<QString> KeyboardTranslatorManager::availableTranslators() const
{
    return _translators.keys();
}

KeyboardTranslator::Entry::Entry( int keyCode,
                                  Qt::KeyboardModifier modifiers,
                                  State flags,
                                  const char* text,
                                  Command command )
: _keyCode(keyCode)
, _modifiers(modifiers)
, _state(flags)
, _command(command)
, _text(0)
{
    _text = new char[strlen(text)];
    strcpy(_text,text); 
}
KeyboardTranslator::Entry::Entry()
: _keyCode(0)
, _modifiers(Qt::NoModifier)
, _state(NoState)
, _command(NoCommand)
, _text(0)
{
}
KeyboardTranslator::Entry::~Entry()
{
    delete[] _text;
}

bool KeyboardTranslator::Entry::matches(int keyCode , Qt::KeyboardModifier modifiers,
                                        State state) const
{
    return false;
}
KeyboardTranslator::KeyboardTranslator(const QString& name)
: _name(name)
{
}

void KeyboardTranslator::addEntry(const Entry& entry)
{
    QVarLengthArray<Entry>* array = 0;
    const int keyCode = entry.keyCode();

    if ( _entries.contains(keyCode) )
    {
        array = &_entries[keyCode];
    }
    else
    {
        // create a new single-item array for entries with the given keycode
        _entries.insert(keyCode,QVarLengthArray<Entry>(1));
        array = &_entries[keyCode];
    }

    // insert the new entry
    array->append(entry);
}

KeyboardTranslator::Entry* KeyboardTranslator::findEntry(int keyCode, Qt::KeyboardModifier modifiers, State state) const
{
    if ( _entries.contains(keyCode) )
    {
        const QVarLengthArray<Entry>& array = _entries[keyCode];

        for (int i = 0 ; i < array.count() ; i++)
        {
            if ( array[i].matches(keyCode,modifiers,state) )
                return const_cast<Entry*>(&array[i]);
        }

        return 0; // entry not found
    }
    else
    {
        return 0;
    }
}

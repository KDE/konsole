/*
    This file is part of Konsole, an X terminal.

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
#include "ExtendedCharTable.h"

// KDE
#include <KDebug>

// Konsole
#include "TerminalDisplay.h"
#include "SessionManager.h"
#include "Session.h"
#include "Screen.h"

using namespace Konsole;

ExtendedCharTable::ExtendedCharTable()
{
}

ExtendedCharTable::~ExtendedCharTable()
{
    // free all allocated character buffers
    QHashIterator<ushort, ushort*> iter(extendedCharTable);
    while (iter.hasNext()) {
        iter.next();
        delete[] iter.value();
    }
}

// global instance
ExtendedCharTable ExtendedCharTable::instance;

ushort ExtendedCharTable::createExtendedChar(const ushort* unicodePoints , ushort length)
{
    // look for this sequence of points in the table
    ushort hash = extendedCharHash(unicodePoints, length);
    const ushort initialHash = hash;
    bool triedCleaningSolution = false;

    // check existing entry for match
    while (extendedCharTable.contains(hash) && hash != 0) { // 0 has a special meaning for chars so we don't use it
        if (extendedCharMatch(hash, unicodePoints, length)) {
            // this sequence already has an entry in the table,
            // return its hash
            return hash;
        } else {
            // if hash is already used by another, different sequence of unicode character
            // points then try next hash
            hash++;

            if (hash == initialHash) {
                if (!triedCleaningSolution) {
                    triedCleaningSolution = true;
                    // All the hashes are full, go to all Screens and try to free any
                    // This is slow but should happen very rarely
                    QSet<ushort> usedExtendedChars;
                    const SessionManager* sm = SessionManager::instance();
                    foreach(const Session * s, sm->sessions()) {
                        foreach(const TerminalDisplay * td, s->views()) {
                            usedExtendedChars += td->screenWindow()->screen()->usedExtendedChars();
                        }
                    }

                    QHash<ushort, ushort*>::iterator it = extendedCharTable.begin();
                    QHash<ushort, ushort*>::iterator itEnd = extendedCharTable.end();
                    while (it != itEnd) {
                        if (usedExtendedChars.contains(it.key())) {
                            ++it;
                        } else {
                            it = extendedCharTable.erase(it);
                        }
                    }
                } else {
                    kWarning() << "Using all the extended char hashes, going to miss this extended character";
                    return 0;
                }
            }
        }
    }

    // add the new sequence to the table and
    // return that index
    ushort* buffer = new ushort[length + 1];
    buffer[0] = length;
    for (int i = 0 ; i < length ; i++)
        buffer[i + 1] = unicodePoints[i];

    extendedCharTable.insert(hash, buffer);

    return hash;
}

ushort* ExtendedCharTable::lookupExtendedChar(ushort hash , ushort& length) const
{
    // lookup index in table and if found, set the length
    // argument and return a pointer to the character sequence

    ushort* buffer = extendedCharTable[hash];
    if (buffer) {
        length = buffer[0];
        return buffer + 1;
    } else {
        length = 0;
        return 0;
    }
}

ushort ExtendedCharTable::extendedCharHash(const ushort* unicodePoints , ushort length) const
{
    ushort hash = 0;
    for (ushort i = 0 ; i < length ; i++) {
        hash = 31 * hash + unicodePoints[i];
    }
    return hash;
}

bool ExtendedCharTable::extendedCharMatch(ushort hash , const ushort* unicodePoints , ushort length) const
{
    ushort* entry = extendedCharTable[hash];

    // compare given length with stored sequence length ( given as the first ushort in the
    // stored buffer )
    if (entry == 0 || entry[0] != length)
        return false;
    // if the lengths match, each character must be checked.  the stored buffer starts at
    // entry[1]
    for (int i = 0 ; i < length ; i++) {
        if (entry[i + 1] != unicodePoints[i])
            return false;
    }
    return true;
}


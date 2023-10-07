/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "ExtendedCharTable.h"

#include "charactersdebug.h"

using namespace Konsole;

ExtendedCharTable::ExtendedCharTable() = default;

ExtendedCharTable::~ExtendedCharTable()
{
    // free all allocated character buffers
    QHashIterator<uint, char32_t *> iter(_extendedCharTable);
    while (iter.hasNext()) {
        iter.next();
        delete[] iter.value();
    }
}

// global instance
ExtendedCharTable ExtendedCharTable::instance;

char32_t ExtendedCharTable::createExtendedChar(const char32_t *unicodePoints, ushort length, const pExtendedChars extendedChars)
{
    // look for this sequence of points in the table
    uint hash = extendedCharHash(unicodePoints, length);
    const uint initialHash = hash;
    bool triedCleaningSolution = false;

    // check existing entry for match
    while (_extendedCharTable.contains(hash) && hash != 0) { // 0 has a special meaning for chars so we don't use it
        if (extendedCharMatch(hash, unicodePoints, length)) {
            // this sequence already has an entry in the table,
            // return its hash
            return hash;
        }
        // if hash is already used by another, different sequence of unicode character
        // points then try next hash
        hash++;

        if (hash == initialHash) {
            if (!triedCleaningSolution) {
                triedCleaningSolution = true;
                // All the hashes are full, go to all Screens and try to free any
                // This is slow but should happen very rarely
                QSet<uint> usedExtendedChars = extendedChars();

                QHash<uint, char32_t *>::iterator it = _extendedCharTable.begin();
                QHash<uint, char32_t *>::iterator itEnd = _extendedCharTable.end();
                while (it != itEnd) {
                    if (usedExtendedChars.contains(it.key())) {
                        ++it;
                    } else {
                        it = _extendedCharTable.erase(it);
                    }
                }
            } else {
                qCDebug(CharactersDebug) << "Using all the extended char hashes, going to miss this extended character";
                return 0;
            }
        }
    }

    // add the new sequence to the table and
    // return that index
    auto buffer = new char32_t[length + 1];
    buffer[0] = length;
    std::copy_n(unicodePoints, length, &buffer[1]);

    _extendedCharTable.insert(hash, buffer);

    return hash;
}

char32_t *ExtendedCharTable::lookupExtendedChar(uint hash, ushort &length) const
{
    // look up index in table and if found, set the length
    // argument and return a pointer to the character sequence

    char32_t *buffer = _extendedCharTable[hash];
    if (buffer != nullptr) {
        length = ushort(buffer[0]);
        return buffer + 1;
    }
    length = 0;
    return nullptr;
}

uint ExtendedCharTable::extendedCharHash(const char32_t *unicodePoints, ushort length) const
{
    uint hash = 0;
    for (ushort i = 0; i < length; i++) {
        hash = 31 * hash + unicodePoints[i];
    }
    return hash;
}

bool ExtendedCharTable::extendedCharMatch(uint hash, const char32_t *unicodePoints, ushort length) const
{
    char32_t *entry = _extendedCharTable[hash];

    // compare given length with stored sequence length ( given as the first ushort in the
    // stored buffer )
    if (entry == nullptr || entry[0] != length) {
        return false;
    }
    // if the lengths match, each character must be checked.  the stored buffer starts at
    // entry[1]
    for (int i = 0; i < length; i++) {
        if (entry[i + 1] != unicodePoints[i]) {
            return false;
        }
    }
    return true;
}

/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef EXTENDEDCHARTABLE_H
#define EXTENDEDCHARTABLE_H

// Qt
#include <QHash>
#include <QSet>

#include <functional>

namespace Konsole
{
/**
 * A table which stores sequences of unicode characters, referenced
 * by hash keys.  The hash key itself is the same size as a unicode
 * character ( uint ) so that it can occupy the same space in
 * a structure.
 */
class ExtendedCharTable
{
public:
    typedef std::function<QSet<uint>()> pExtendedChars;

    /** Constructs a new character table. */
    ExtendedCharTable();
    ~ExtendedCharTable();

    /**
     * Adds a sequences of unicode characters to the table and returns
     * a hash code which can be used later to look up the sequence
     * using lookupExtendedChar()
     *
     * If the same sequence already exists in the table, the hash
     * of the existing sequence will be returned.
     *
     * @param unicodePoints An array of unicode character points
     * @param length Length of @p unicodePoints
     */
    uint createExtendedChar(const uint *unicodePoints, ushort length, const pExtendedChars extendedChars);
    /**
     * Looks up and returns a pointer to a sequence of unicode characters
     * which was added to the table using createExtendedChar().
     *
     * @param hash The hash key returned by createExtendedChar()
     * @param length This variable is set to the length of the
     * character sequence.
     *
     * @return A unicode character sequence of size @p length.
     */
    uint *lookupExtendedChar(uint hash, ushort &length) const;

    /** The global ExtendedCharTable instance. */
    static ExtendedCharTable instance;

private:
    // calculates the hash key of a sequence of unicode points of size 'length'
    uint extendedCharHash(const uint *unicodePoints, ushort length) const;
    // tests whether the entry in the table specified by 'hash' matches the
    // character sequence 'unicodePoints' of size 'length'
    bool extendedCharMatch(uint hash, const uint *unicodePoints, ushort length) const;
    // internal, maps hash keys to character sequence buffers.  The first uint
    // in each value is the length of the buffer, followed by the uints in the buffer
    // themselves.
    QHash<uint, uint *> _extendedCharTable;
};

}

#endif // end of EXTENDEDCHARTABLE_H

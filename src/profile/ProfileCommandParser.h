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

#ifndef PROFILECOMMANDPARSER_H
#define PROFILECOMMANDPARSER_H

// Konsole
#include "konsoleprofile_export.h"
#include "Profile.h"

class QVariant;
template<typename, typename> class QHash;

namespace Konsole
{
    /**
     * Parses an input string consisting of property names
     * and assigned values and returns a table of properties
     * and values.
     *
     * The input string will typically look like this:
     *
     * @code
     *   PropertyName=Value;PropertyName=Value ...
     * @endcode
     *
     * For example:
     *
     * @code
     *   Icon=konsole;Directory=/home/bob
     * @endcode
     */
    class KONSOLEPROFILE_EXPORT ProfileCommandParser
    {
    public:
        /**
         * Parses an input string consisting of property names
         * and assigned values and returns a table of
         * properties and values.
         */
        QHash<Profile::Property, QVariant> parse(const QString &input);
    };
}

#endif

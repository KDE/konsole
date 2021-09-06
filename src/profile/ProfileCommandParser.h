/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PROFILECOMMANDPARSER_H
#define PROFILECOMMANDPARSER_H

// Konsole
#include "Profile.h"
#include "konsoleprofile_export.h"

class QVariant;
template<typename, typename>
class QHash;

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

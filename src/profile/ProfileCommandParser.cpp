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
#include "ProfileCommandParser.h"

// Konsole

// Qt
#include <QHash>
#include <QVariant>
#include <QRegularExpression>

using namespace Konsole;

QHash<Profile::Property, QVariant> ProfileCommandParser::parse(const QString& input)
{
    QHash<Profile::Property, QVariant> changes;

    // regular expression to parse profile change requests.
    //
    // format: property=value;property=value ...
    //
    // where 'property' is a word consisting only of characters from A-Z
    // where 'value' is any sequence of characters other than a semi-colon
    //
    static const QRegularExpression regExp(QStringLiteral("([a-zA-Z]+)=([^;]+)"));

    QRegularExpressionMatchIterator iterator(regExp.globalMatch(input));
    while (iterator.hasNext()) {
        QRegularExpressionMatch match(iterator.next());
        Profile::Property property = Profile::lookupByName(match.captured(1));
        const QString value = match.captured(2);
        changes.insert(property, value);
    }

    return changes;
}

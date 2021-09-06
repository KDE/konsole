/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "ProfileCommandParser.h"

// Konsole

// Qt
#include <QHash>
#include <QRegularExpression>
#include <QVariant>

using namespace Konsole;

QHash<Profile::Property, QVariant> ProfileCommandParser::parse(const QString &input)
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

/*
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
#include "ShellCommand.h"

// KDE
#include <KShell>

using Konsole::ShellCommand;

ShellCommand::ShellCommand(const QString& aCommand)
{
    _arguments = KShell::splitArgs(aCommand);
}
ShellCommand::ShellCommand(const QString& aCommand, const QStringList& aArguments)
{
    _arguments = aArguments;

    if (!_arguments.isEmpty())
        _arguments[0] = aCommand;
}
QString ShellCommand::fullCommand() const
{
    QStringList quotedArgs(_arguments);
    for (int i = 0; i < quotedArgs.count(); i++) {
        QString arg = quotedArgs.at(i);
        bool hasSpace = false;
        for (int j = 0; j < arg.count(); j++)
            if (arg[j].isSpace())
                hasSpace = true;
        if (hasSpace)
            quotedArgs[i] = '\"' + arg + '\"';
    }
    return quotedArgs.join(QChar(' '));
}
QString ShellCommand::command() const
{
    if (!_arguments.isEmpty())
        return _arguments[0];
    else
        return QString();
}
QStringList ShellCommand::arguments() const
{
    return _arguments;
}
QStringList ShellCommand::expand(const QStringList& items)
{
    QStringList result;

    foreach(const QString & item , items) {
        result << expand(item);
    }

    return result;
}
QString ShellCommand::expand(const QString& text)
{
    QString result = text;
    expandEnv(result);
    return result;
}

bool ShellCommand::isValidEnvCharacter(const QChar& ch)
{
    const ushort code = ch.unicode();
    return isValidLeadingEnvCharacter(ch) || ('0' <= code && code <= '9');
}

bool ShellCommand::isValidLeadingEnvCharacter(const QChar& ch)
{
    const ushort code = ch.unicode();
    return (code == '_') || ('A' <= code && code <= 'Z');
}

/*
 * expandEnv
 *
 * Expand environment variables in text. Escaped '$' characters are ignored.
 * Return true if any variables were expanded
 */
bool ShellCommand::expandEnv(QString& text)
{
    const QLatin1Char dollarChar('$');
    const QLatin1Char backslashChar('\\');

    int dollarPos = 0;
    bool expanded = false;

    // find and expand all environment variables beginning with '$'
    while ((dollarPos = text.indexOf(dollarChar, dollarPos)) != -1) {
        // if '$' is the last character, there is no way of expanding
        if (dollarPos == text.length() - 1) {
            break;
        }

        // skip escaped '$'
        if (dollarPos > 0 && text.at(dollarPos - 1) == backslashChar) {
            dollarPos++;
            continue;
        }

        // if '$' is followed by an invalid leading character, skip this '$'
        if (!isValidLeadingEnvCharacter(text.at(dollarPos + 1))) {
            dollarPos++;
            continue;
        }

        int endPos = dollarPos + 1;
        Q_ASSERT(endPos < text.length());
        while (endPos < text.length() && isValidEnvCharacter(text.at(endPos))) {
            endPos++;
        }

        const int len = endPos - dollarPos;
        const QString key = text.mid(dollarPos + 1, len - 1);
        const QString value = QString::fromLocal8Bit(qgetenv(key.toLocal8Bit().constData()));

        if (!value.isEmpty()) {
            text.replace(dollarPos, len, value);
            expanded = true;
            dollarPos = dollarPos + value.length();
        } else {
            dollarPos = endPos;
        }
    }

    return expanded;
}

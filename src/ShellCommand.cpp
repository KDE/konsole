/*
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

// Own
#include "ShellCommand.h"

// Qt
#include <QtDebug>

using namespace Konsole;

ShellCommand::ShellCommand(const QString& fullCommand)
{
    bool inQuotes = false;

    QString builder;

    for ( int i = 0 ; i < fullCommand.count() ; i++ )
    {
        QChar ch = fullCommand[i];

        const bool isLastChar = ( i == fullCommand.count() - 1 );
        const bool isQuote = ( ch == '\'' || ch == '\"' );

        if ( !isLastChar && isQuote )
            inQuotes = !inQuotes;
        else
        { 
            if ( (!ch.isSpace() || inQuotes) && !isQuote )
                builder.append(ch);

            if ( (ch.isSpace() && !inQuotes) || ( i == fullCommand.count()-1 ) )
            {
                _arguments << builder;      
                builder.clear(); 
            }
        }
    }

    //qDebug() << "Arguments:" << _arguments;
}
ShellCommand::ShellCommand(const QString& command , const QStringList& arguments)
{
    _arguments = arguments;
    
    if ( !_arguments.isEmpty() )
        _arguments[0] == command;
}
QString ShellCommand::fullCommand() const
{
    return _arguments.join(QChar(' '));
}
QString ShellCommand::command() const
{
    if ( !_arguments.isEmpty() )
        return _arguments[0];
    else
        return QString();
}
QStringList ShellCommand::arguments() const
{
    return _arguments;
}
bool ShellCommand::isRootCommand() const
{
    Q_ASSERT(0); // not implemented yet
    return false;
}
bool ShellCommand::isAvailable() const
{
    Q_ASSERT(0); // not implemented yet
    return false; 
}

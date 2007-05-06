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

#ifndef SHELLCOMMAND_H
#define SHELLCOMMAND_H

// Qt
#include <QStringList>

namespace Konsole
{

/** A class to parse and extract information about shell commands. */
class ShellCommand
{
public:
    /**
     * Constructs a ShellCommand from a command line.
     *
     * @param fullCommand The command line to parse.  
     */
    ShellCommand(const QString& fullCommand);
    /**
     * Constructs a ShellCommand with the specified @p command and @p arguments.
     */
    ShellCommand(const QString& command , const QStringList& arguments);

    /** Returns the command. */
    QString command() const;
    /** Returns the arguments. */
    QStringList arguments() const;

    /** Returns the full command line. */
    QString fullCommand() const;

    /** Returns true if this is a root command. */
    bool isRootCommand() const;
    /** Returns true if the program specified by @p command() exists. */
    bool isAvailable() const;

private:
    QStringList _arguments;    
};

}

#endif // SHELLCOMMAND_H


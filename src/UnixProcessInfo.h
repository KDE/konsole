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

#ifndef UNIXPROCESSINFO_H
#define UNIXPROCESSINFO_H

#include "ProcessInfo.h"

namespace Konsole
{

    /**
     * Implementation of ProcessInfo for Unix platforms which uses
     * the /proc filesystem
     */
    class UnixProcessInfo : public ProcessInfo
    {
    public:
        /**
         * Constructs a new instance of UnixProcessInfo.
         * See ProcessInfo::newInstance()
         */
        explicit UnixProcessInfo(int pid);

    protected:
        /**
         * Implementation of ProcessInfo::readProcessInfo(); calls the
         * four private methods below in turn.
         */
        void readProcessInfo(int pid) override;

        void readUserName(void) override;

    private:
        /**
         * Read the standard process information -- PID, parent PID, foreground PID.
         * @param pid process ID to use
         * @return true on success
         */
        virtual bool readProcInfo(int pid) = 0;

        /**
         * Determine what arguments were passed to the process. Sets _arguments.
         * @param pid process ID to use
         * @return true on success
         */
        virtual bool readArguments(int pid) = 0;
    };

}

#endif

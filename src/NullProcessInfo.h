/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.countm>

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

#ifndef NULLPROCESSINFO_H
#define NULLPROCESSINFO_H

#include "ProcessInfo.h"

namespace Konsole
{

    /**
     * Implementation of ProcessInfo which does nothing.
     * Used on platforms where a suitable ProcessInfo subclass is not
     * available.
     *
     * isValid() will always return false for instances of NullProcessInfo
     */
    class NullProcessInfo : public ProcessInfo
    {
    public:
        /**
         * Constructs a new NullProcessInfo instance.
         * See ProcessInfo::newInstance()
         */
        explicit NullProcessInfo(int pid);
    protected:
        void readProcessInfo(int pid) override;
        bool readCurrentDir(int pid) override;
        void readUserName(void) override;
    };

}

#endif

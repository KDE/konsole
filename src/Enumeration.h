/*
  Copyright 2012 Jekyll Wu <adaptee@gmail.com>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of
  the License or (at your option) version 3 or any later version
  accepted by the membership of KDE e.V. (or its successor appro-
  ved by the membership of KDE e.V.), which shall act as a proxy
  defined in Section 14 of version 3 of the license.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see http://www.gnu.org/licenses/.
*/

#ifndef ENUMERATION_H
#define ENUMERATION_H

#include "konsole_export.h"

namespace Konsole
{

class Enum
{
public:

    /**
     * This enum describes the modes available to remember lines of output
     * produced by the terminal.
     */
    enum HistoryModeEnum {
        /** No output is remembered.  As soon as lines of text are scrolled
         * off-screen they are lost.
         */
        NoHistory   = 0,
        /** A fixed number of lines of output are remembered.  Once the
         * limit is reached, the oldest lines are lost.
         */
        FixedSizeHistory = 1,
        /** All output is remembered for the duration of the session.
         * Typically this means that lines are recorded to
         * a file as they are scrolled off-screen.
         */
        UnlimitedHistory = 2
    };

};

}
#endif // ENUMERATION_H


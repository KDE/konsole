/*
    This source file is part of Konsole, a terminal emulator.

    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>

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

#ifndef SHOULDAPPLYPROPERTY_H
#define SHOULDAPPLYPROPERTY_H

#include "profile/Profile.h"

namespace Konsole
{
    /** Utility class to simplify code in SessionManager::applyProfile(). */
    class ShouldApplyProperty
    {
    public:
        ShouldApplyProperty(const Profile::Ptr &profile, bool modifiedOnly);

        bool shouldApply(Profile::Property property) const;

    private:
        const Profile::Ptr _profile;
        bool _modifiedPropertiesOnly;
    };
}

#endif

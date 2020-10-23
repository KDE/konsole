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
#include "ProfileGroup.h"

// Konsole

using namespace Konsole;

void ProfileGroup::addProfile(const Profile::Ptr &profile)
{
    _profiles.append(profile);
}

void ProfileGroup::removeProfile(const Profile::Ptr &profile)
{
    _profiles.removeAll(profile);
}

QList<Profile::Ptr> ProfileGroup::profiles() const
{
    return _profiles;
}

void ProfileGroup::updateValues()
{
    const PropertyInfo* properties = Profile::DefaultPropertyNames;
    while (properties->name != nullptr) {
        // the profile group does not store a value for some properties
        // (eg. name, path) if even they are equal between profiles -
        //
        // the exception is when the group has only one profile in which
        // case it behaves like a standard Profile
        if (_profiles.count() > 1 &&
                !canInheritProperty(properties->property)) {
            properties++;
            continue;
        }

        QVariant value;
        for (int i = 0; i < _profiles.count(); i++) {
            QVariant profileValue = _profiles[i]->property<QVariant>(properties->property);
            if (value.isNull()) {
                value = profileValue;
            } else if (value != profileValue) {
                value = QVariant();
                break;
            }
        }
        Profile::setProperty(properties->property, value);
        properties++;
    }
}

void ProfileGroup::setProperty(Property p, const QVariant& value)
{
    if (_profiles.count() > 1 && !canInheritProperty(p)) {
        return;
    }

    Profile::setProperty(p, value);
    for (const Profile::Ptr &profile : qAsConst(_profiles)) {
        profile->setProperty(p, value);
    }
}

/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
    const PropertyInfo *properties = Profile::DefaultPropertyNames;
    while (properties->name != nullptr) {
        // the profile group does not store a value for some properties
        // (eg. name, path) if even they are equal between profiles -
        //
        // the exception is when the group has only one profile in which
        // case it behaves like a standard Profile
        if (_profiles.count() > 1 && !canInheritProperty(properties->property)) {
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

void ProfileGroup::setProperty(Property p, const QVariant &value)
{
    if (_profiles.count() > 1 && !canInheritProperty(p)) {
        return;
    }

    Profile::setProperty(p, value);
    for (const Profile::Ptr &profile : qAsConst(_profiles)) {
        profile->setProperty(p, value);
    }
}

/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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

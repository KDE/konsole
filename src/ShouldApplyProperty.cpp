/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ShouldApplyProperty.h"

using namespace Konsole;

ShouldApplyProperty::ShouldApplyProperty(const Profile::Ptr &profile, bool modifiedOnly)
    : _profile(profile)
    , _modifiedPropertiesOnly(modifiedOnly)
{
}

bool ShouldApplyProperty::shouldApply(Profile::Property property) const
{
    return !_modifiedPropertiesOnly || _profile->isPropertySet(property);
}

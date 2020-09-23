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

#ifndef PROFILEGROUP_H
#define PROFILEGROUP_H

// Konsole
#include "konsoleprofile_export.h"
#include "Profile.h"

// Qt
#include <QHash>
#include <QStringList>
#include <QVariant>
#include <QFont>
#include <QColor>

namespace Konsole
{
    /**
     * A composite profile which allows a group of profiles to be treated as one.
     * When setting a property, the new value is applied to all profiles in the
     * group.  When reading a property, if all profiles in the group have the same
     * value then that value is returned, otherwise the result is null.
     *
     * Profiles can be added to the group using addProfile().  When all profiles
     * have been added updateValues() must be called
     * to sync the group's property values with those of the group's profiles.
     *
     * The Profile::Name and Profile::Path properties are unique to individual
     * profiles, setting these properties on a ProfileGroup has no effect.
     */
    class KONSOLEPROFILE_EXPORT ProfileGroup : public Profile
    {
    public:
        using Ptr = QExplicitlySharedDataPointer<ProfileGroup>;

        /** Construct a new profile group, which is hidden by default. */
        explicit ProfileGroup(const Profile::Ptr &profileParent = Profile::Ptr());

        /** Add a profile to the group.  Calling setProperty() will update this
         * profile.  When creating a group, add the profiles to the group then
         * call updateValues() to make the group's property values reflect the
         * profiles currently in the group.
         */
        void addProfile(const Profile::Ptr &profile);

        /** Remove a profile from the group.  Calling setProperty() will no longer
         * affect this profile. */
        void removeProfile(const Profile::Ptr &profile);

        /** Returns the profiles in this group .*/
        QList<Profile::Ptr> profiles() const;

        /**
         * Updates the property values in this ProfileGroup to match those from
         * the group's profiles()
         *
         * For each available property, if each profile in the group has the same
         * value then the ProfileGroup will use that value for the property.
         * Otherwise the value for the property will be set to a null QVariant
         *
         * Some properties such as the name and the path of the profile
         * will always be set to null if the group has more than one profile.
         */
        void updateValues();

        /** Sets the value of @p property in each of the group's profiles to
         * @p value.
         */
        void setProperty(Property p, const QVariant &value) override;

    private:
        Q_DISABLE_COPY(ProfileGroup)

        QList<Profile::Ptr> _profiles;
    };

    inline ProfileGroup::ProfileGroup(const Profile::Ptr &profileParent)
        : Profile(profileParent)
        , _profiles(QList<Profile::Ptr>())
    {
        setHidden(true);
    }
}

#endif

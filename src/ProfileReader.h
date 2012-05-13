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

#ifndef PROFILEREADER_H
#define PROFILEREADER_H

#include "Profile.h"

class KConfig;

namespace Konsole
{
/** Interface for all classes which can load profile settings from a file. */
class ProfileReader
{
public:
    virtual ~ProfileReader() {}
    /** Returns a list of paths to profiles which this reader can read. */
    virtual QStringList findProfiles() {
        return QStringList();
    }
    /**
     * Attempts to read a profile from @p path and
     * save the property values described into @p profile.
     *
     * Returns true if the profile was successfully read or false otherwise.
     *
     * @param path Path to the profile to read
     * @param profile Pointer to the Profile the settings will be read into
     * @param parentProfile Receives the name of the parent profile
     */
    virtual bool readProfile(const QString& path , Profile::Ptr profile , QString& parentProfile) = 0;
};

/** Reads a KDE 4 .profile file. */
class KDE4ProfileReader : public ProfileReader
{
public:
    virtual QStringList findProfiles();
    virtual bool readProfile(const QString& path , Profile::Ptr profile, QString& parentProfile);
private:
    void readProperties(const KConfig& config, Profile::Ptr profile,
                        const Profile::PropertyInfo* properties);
};
}

#endif // PROFILEREADER_H

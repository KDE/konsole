/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PROFILEREADER_H
#define PROFILEREADER_H

#include "Profile.h"

class KConfig;

namespace Konsole
{
/** Interface for all classes which can load profile settings from a file. */
class KONSOLEPROFILE_EXPORT ProfileReader
{
public:
    ProfileReader();
    ~ProfileReader();

    /** Returns a list of paths to profiles which this reader can read. */
    QStringList findProfiles();

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
    bool readProfile(const QString &path, Profile::Ptr profile, QString &parentProfile);

private:
    void readProperties(const KConfig &config, Profile::Ptr profile, const Profile::PropertyInfo *properties);
};
}

#endif // PROFILEREADER_H

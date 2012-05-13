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

#ifndef PROFILEWRITER_H
#define PROFILEWRITER_H

#include "Profile.h"

class KConfig;

namespace Konsole
{
/** Interface for all classes which can write profile settings to a file. */
class ProfileWriter
{
public:
    virtual ~ProfileWriter() {}
    /**
     * Returns a suitable path-name for writing
     * @p profile to. The path-name should be accepted by
     * the corresponding ProfileReader class.
     */
    virtual QString getPath(const Profile::Ptr profile) = 0;
    /**
     * Writes the properties and values from @p profile to the file specified
     * by @p path.  This profile should be readable by the corresponding
     * ProfileReader class.
     */
    virtual bool writeProfile(const QString& path , const Profile::Ptr profile) = 0;
};
/** Writes a KDE 4 .profile file. */
class KDE4ProfileWriter : public ProfileWriter
{
public:
    virtual QString getPath(const Profile::Ptr profile);
    virtual bool writeProfile(const QString& path , const Profile::Ptr profile);

private:
    void writeProperties(KConfig& config, const Profile::Ptr profile,
                         const Profile::PropertyInfo* properties);
};
}

#endif // PROFILEWRITER_H

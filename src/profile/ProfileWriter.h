/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PROFILEWRITER_H
#define PROFILEWRITER_H

#include "Profile.h"

class KConfig;

namespace Konsole
{
/** Interface for all classes which can write profile settings to a file. */
class KONSOLEPROFILE_EXPORT ProfileWriter
{
public:
    ProfileWriter();
    ~ProfileWriter();

    /**
     * Returns a suitable path-name for writing
     * @p profile to. The path-name should be accepted by
     * the corresponding ProfileReader class.
     */
    QString getPath(const Profile::Ptr &profile);
    /**
     * Writes the properties and values from @p profile to the file specified
     * by @p path.  This profile should be readable by the corresponding
     * ProfileReader class.
     */
    bool writeProfile(const QString &path, const Profile::Ptr &profile);

private:
    void writeProperties(KConfig &config, const Profile::Ptr &profile);
};
}

#endif // PROFILEWRITER_H

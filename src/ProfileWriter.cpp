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

// Own
#include "ProfileWriter.h"

// Qt

// KDE
#include <KConfig>
#include <KConfigGroup>

// Konsole
#include "ShellCommand.h"

using namespace Konsole;

// FIXME: A dup line from Profile.cpp - redo these
static const char GENERAL_GROUP[]     = "General";

ProfileWriter::ProfileWriter() = default;
ProfileWriter::~ProfileWriter() = default;

// All profiles changes are stored under users' local account
QString ProfileWriter::getPath(const Profile::Ptr &profile)
{
    // If any changes are made to this location, check that programs using
    // the Konsole part can write/save profiles
    static const QString localDataLocation = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/konsole");

    return localDataLocation % QLatin1String("/") % profile->untranslatedName() % QLatin1String(".profile");
}
void ProfileWriter::writeProperties(KConfig& config,
                                        const Profile::Ptr &profile,
                                        const Profile::PropertyInfo* properties)
{
    const char* groupName = nullptr;
    KConfigGroup group;

    while (properties->name != nullptr) {
        if (properties->group != nullptr) {
            if (groupName == nullptr || qstrcmp(groupName, properties->group) != 0) {
                group = config.group(properties->group);
                groupName = properties->group;
            }

            if (profile->isPropertySet(properties->property)) {
                group.writeEntry(QLatin1String(properties->name),
                                 profile->property<QVariant>(properties->property));
            }
        }

        properties++;
    }
}
bool ProfileWriter::writeProfile(const QString& path , const Profile::Ptr &profile)
{
    KConfig config(path, KConfig::NoGlobals);

    if (!config.isConfigWritable(false)) {
        return false;
    }

    KConfigGroup general = config.group(GENERAL_GROUP);

    // Parent profile if set, when loading the profile in future, the parent
    // must be loaded as well if it exists.
    if (profile->parent()) {
        general.writeEntry("Parent", profile->parent()->path());
    }

    if (profile->isPropertySet(Profile::Command)
            || profile->isPropertySet(Profile::Arguments)) {
        general.writeEntry("Command",
                           ShellCommand(profile->command(), profile->arguments()).fullCommand());
     }

    // Write remaining properties
    writeProperties(config, profile, Profile::DefaultPropertyNames);

    return true;
}

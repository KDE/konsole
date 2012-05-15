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
#include "ProfileReader.h"

// Qt
#include <QtCore/QFile>

// KDE
#include <KConfig>
#include <KConfigGroup>
#include <KGlobal>
#include <KStandardDirs>

// Konsole
#include "ShellCommand.h"

using namespace Konsole;

// FIXME: A dup line from Profile.cpp - redo these
static const char GENERAL_GROUP[]     = "General";

QStringList KDE4ProfileReader::findProfiles()
{
    return KGlobal::dirs()->findAllResources("data", "konsole/*.profile",
            KStandardDirs::NoDuplicates);
}
void KDE4ProfileReader::readProperties(const KConfig& config, Profile::Ptr profile,
                                       const Profile::PropertyInfo* properties)
{
    const char* groupName = 0;
    KConfigGroup group;

    while (properties->name != 0) {
        if (properties->group != 0) {
            if (groupName == 0 || qstrcmp(groupName, properties->group) != 0) {
                group = config.group(properties->group);
                groupName = properties->group;
            }

            QString name(properties->name);

            if (group.hasKey(name))
                profile->setProperty(properties->property,
                                     group.readEntry(name, QVariant(properties->type)));
        }

        properties++;
    }
}

bool KDE4ProfileReader::readProfile(const QString& path , Profile::Ptr profile , QString& parentProfile)
{
    if (!QFile::exists(path))
        return false;

    KConfig config(path, KConfig::NoGlobals);

    KConfigGroup general = config.group(GENERAL_GROUP);
    if (general.hasKey("Parent"))
        parentProfile = general.readEntry("Parent");

    if (general.hasKey("Command")) {
        ShellCommand shellCommand(general.readEntry("Command"));

        profile->setProperty(Profile::Command, shellCommand.command());
        profile->setProperty(Profile::Arguments, shellCommand.arguments());
    }

    profile->setProperty(Profile::UntranslatedName, general.readEntryUntranslated("Name"));

    // Read remaining properties
    readProperties(config, profile, Profile::DefaultPropertyNames);

    return true;
}


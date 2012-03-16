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
#include <QtCore/QFile>

// KDE
#include <KConfigGroup>
#include <KGlobal>
#include <KGlobalSettings>
#include <KStandardDirs>

// Konsole
#include "ShellCommand.h"

using namespace Konsole;

// FIXME: A dup line from Profile.cpp - redo these
static const char GENERAL_GROUP[]     = "General";

QString KDE4ProfileWriter::getPath(const Profile::Ptr info)
{
    QString newPath;

    if (info->isPropertySet(Profile::Path) &&
            info->path().startsWith(KGlobal::dirs()->saveLocation("data", "konsole/"))) {
        newPath = info->path();
    } else {
        // use the profile name + ".profile" and save it in $KDEHOME
        newPath = KGlobal::dirs()->saveLocation("data", "konsole/") + info->name() + ".profile";
    }

    return newPath;
}
void KDE4ProfileWriter::writeProperties(KConfig& config,
                                        const Profile::Ptr profile,
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

            if (profile->isPropertySet(properties->property))
                group.writeEntry(QString(properties->name),
                                 profile->property<QVariant>(properties->property));
        }

        properties++;
    }
}
bool KDE4ProfileWriter::writeProfile(const QString& path , const Profile::Ptr profile)
{
    KConfig config(path, KConfig::NoGlobals);

    KConfigGroup general = config.group(GENERAL_GROUP);

    // Parent profile if set, when loading the profile in future, the parent
    // must be loaded as well if it exists.
    if (profile->parent())
        general.writeEntry("Parent", profile->parent()->path());

    if (profile->isPropertySet(Profile::Command)
            || profile->isPropertySet(Profile::Arguments))
        general.writeEntry("Command",
                           ShellCommand(profile->command(), profile->arguments()).fullCommand());

    // Write remaining properties
    writeProperties(config, profile, Profile::DefaultPropertyNames);

    return true;
}


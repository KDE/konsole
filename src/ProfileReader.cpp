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
#include <QFile>
#include <QDir>

// KDE
#include <KConfig>
#include <KConfigGroup>

// Konsole
#include "ShellCommand.h"

using namespace Konsole;

// FIXME: A dup line from Profile.cpp - redo these
static const char GENERAL_GROUP[]     = "General";
static const char FEATURES_GROUP[]        = "Terminal Features";
static const char URLHINTS_KEY[]          = "EnableUrlHints";
static const char URLHINTSMODIFIERS_KEY[] = "UrlHintsModifiers";

QStringList KDE4ProfileReader::findProfiles()
{
    QStringList profiles;
    const QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("konsole"), QStandardPaths::LocateDirectory);
    profiles.reserve(dirs.size());

    Q_FOREACH (const QString& dir, dirs) {
        const QStringList fileNames = QDir(dir).entryList(QStringList() << QStringLiteral("*.profile"));
        Q_FOREACH (const QString& file, fileNames) {
            profiles.append(dir + QLatin1Char('/') + file);
        }
    }
    return profiles;
}
void KDE4ProfileReader::readProperties(const KConfig& config, Profile::Ptr profile,
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

            QString name(QLatin1String(properties->name));

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

    // Check if the user earlier had set the URL hints option, and in that case set the default
    // URL hints modifier to the earlier default.
    if (config.hasGroup(FEATURES_GROUP)) {
        KConfigGroup features = config.group(FEATURES_GROUP);
        if (features.hasKey(URLHINTS_KEY)) {
            bool enable = features.readEntry(URLHINTS_KEY, false);
            if (enable && !features.hasKey(URLHINTSMODIFIERS_KEY)) {
                features.writeEntry(URLHINTSMODIFIERS_KEY, int(Qt::ControlModifier));
            }
            features.deleteEntry(URLHINTS_KEY);
        }
    }

    profile->setProperty(Profile::UntranslatedName, general.readEntryUntranslated("Name"));

    // Read remaining properties
    readProperties(config, profile, Profile::DefaultPropertyNames);

    return true;
}


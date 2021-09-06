/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "ProfileReader.h"

// Qt
#include <QDir>
#include <QFile>

// KDE
#include <KConfig>
#include <KConfigGroup>

// Konsole
#include "ShellCommand.h"

using namespace Konsole;

// FIXME: A dup line from Profile.cpp - redo these
static const char GENERAL_GROUP[] = "General";
static const char FEATURES_GROUP[] = "Terminal Features";
static const char URLHINTS_KEY[] = "EnableUrlHints";
static const char URLHINTSMODIFIERS_KEY[] = "UrlHintsModifiers";

ProfileReader::ProfileReader() = default;
ProfileReader::~ProfileReader() = default;

QStringList ProfileReader::findProfiles()
{
    QStringList profiles;
    const QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("konsole"), QStandardPaths::LocateDirectory);
    profiles.reserve(dirs.size());

    for (const QString &dir : dirs) {
        const QStringList fileNames = QDir(dir).entryList(QStringList() << QStringLiteral("*.profile"));
        for (const QString &file : fileNames) {
            profiles.append(dir + QLatin1Char('/') + file);
        }
    }
    return profiles;
}
void ProfileReader::readProperties(const KConfig &config, Profile::Ptr profile, const Profile::PropertyInfo *properties)
{
    const char *groupName = nullptr;
    KConfigGroup group;

    while (properties->name != nullptr) {
        if (properties->group != nullptr) {
            if (groupName == nullptr || qstrcmp(groupName, properties->group) != 0) {
                group = config.group(properties->group);
                groupName = properties->group;
            }

            QString name(QLatin1String(properties->name));

            if (group.hasKey(name)) {
                profile->setProperty(properties->property, group.readEntry(name, QVariant(properties->type)));
            }
        }

        properties++;
    }
}

bool ProfileReader::readProfile(const QString &path, Profile::Ptr profile, QString &parentProfile)
{
    if (!QFile::exists(path)) {
        return false;
    }

    KConfig config(path, KConfig::NoGlobals);

    KConfigGroup general = config.group(GENERAL_GROUP);
    if (general.hasKey("Parent")) {
        parentProfile = general.readEntry("Parent");
    }

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

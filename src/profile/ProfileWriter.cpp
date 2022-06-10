/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
static const char GENERAL_GROUP[] = "General";

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

void ProfileWriter::writeProperties(KConfig &config, const Profile::Ptr &profile)
{
    const char *groupName = nullptr;
    KConfigGroup group;

    for (const Profile::PropertyInfo &info : Profile::DefaultProperties) {
        if (info.group != nullptr) {
            if (groupName == nullptr || qstrcmp(groupName, info.group) != 0) {
                group = config.group(info.group);
                groupName = info.group;
            }

            if (profile->isPropertySet(info.property)) {
                group.writeEntry(QLatin1String(info.name), profile->property<QVariant>(info.property));
            }
        }
    }
}

bool ProfileWriter::writeProfile(const QString &path, const Profile::Ptr &profile)
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

    if (profile->isPropertySet(Profile::Command) || profile->isPropertySet(Profile::Arguments)) {
        general.writeEntry("Command", ShellCommand(profile->command(), profile->arguments()).fullCommand());
    }

    // Write remaining properties
    writeProperties(config, profile);

    return true;
}

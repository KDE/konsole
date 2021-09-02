/*
    SPDX-FileCopyrightText: 2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "ProfileTest.h"

// Qt
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTest>
#include <QTextStream>

// KDE

// Konsole
#include "../profile/Profile.h"
#include "../profile/ProfileGroup.h"
#include "../profile/ProfileManager.cpp"
#include "../profile/ProfileReader.h"
#include "../profile/ProfileWriter.h"

#include <QStandardPaths>

#include <array>

using namespace Konsole;

void ProfileTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void ProfileTest::testProfile()
{
    // create a new profile
    Profile *parent = new Profile;
    parent->setProperty(Profile::Name, QStringLiteral("Parent"));
    parent->setProperty(Profile::Path, QStringLiteral("FakePath"));

    parent->setProperty(Profile::AntiAliasFonts, false);
    parent->setProperty(Profile::StartInCurrentSessionDir, false);

    parent->setProperty(Profile::UseCustomCursorColor, true);
    QVERIFY(parent->useCustomCursorColor());
    QCOMPARE(parent->customCursorColor(), QColor());
    QCOMPARE(parent->customCursorTextColor(), QColor());
    parent->setProperty(Profile::UseCustomCursorColor, false);
    QVERIFY(!parent->useCustomCursorColor());
    QCOMPARE(parent->customCursorColor(), QColor());
    QCOMPARE(parent->customCursorTextColor(), QColor());

    // create a child profile
    Profile *child = new Profile(Profile::Ptr(parent));
    child->setProperty(Profile::StartInCurrentSessionDir, true);

    // check which properties are set
    QVERIFY(parent->isPropertySet(Profile::Name));
    QVERIFY(parent->isPropertySet(Profile::Path));
    QVERIFY(parent->isPropertySet(Profile::AntiAliasFonts));
    QVERIFY(!parent->isPropertySet(Profile::Icon));
    QVERIFY(!parent->isPropertySet(Profile::Command));
    QVERIFY(!parent->isPropertySet(Profile::Arguments));

    QVERIFY(child->isPropertySet(Profile::StartInCurrentSessionDir));
    QVERIFY(!child->isPropertySet(Profile::Name));
    QVERIFY(!child->isPropertySet(Profile::AntiAliasFonts));
    QVERIFY(!child->isPropertySet(Profile::ColorScheme));

    // read non-inheritable properties
    QCOMPARE(parent->property<QString>(Profile::Name), QStringLiteral("Parent"));
    QCOMPARE(child->property<QVariant>(Profile::Name), QVariant());
    QCOMPARE(parent->property<QString>(Profile::Path), QStringLiteral("FakePath"));
    QCOMPARE(child->property<QVariant>(Profile::Path), QVariant());

    // read inheritable properties
    QVERIFY(!parent->property<bool>(Profile::AntiAliasFonts));
    QVERIFY(!child->property<bool>(Profile::AntiAliasFonts));

    QVERIFY(!parent->startInCurrentSessionDir());
    QVERIFY(child->startInCurrentSessionDir());

    delete child;
}

void ProfileTest::testClone()
{
    // create source profile and parent
    Profile::Ptr parent(new Profile);
    parent->setProperty(Profile::Command, QStringLiteral("ps"));
    parent->setProperty(Profile::ColorScheme, QStringLiteral("BlackOnWhite"));

    Profile::Ptr source(new Profile(parent));
    source->setProperty(Profile::AntiAliasFonts, false);
    source->setProperty(Profile::HistorySize, 4567);

    source->setProperty(Profile::Name, QStringLiteral("SourceProfile"));
    source->setProperty(Profile::Path, QStringLiteral("SourcePath"));

    // create target to clone source and parent
    Profile::Ptr targetParent(new Profile);
    // same value as source parent
    targetParent->setProperty(Profile::Command, QStringLiteral("ps"));
    // different value from source parent
    targetParent->setProperty(Profile::ColorScheme, QStringLiteral("BlackOnGrey"));
    Profile::Ptr target(new Profile(parent));

    // clone source profile, setting only properties that differ
    // between the source and target
    target->clone(source, true);

    // check that properties from source have been cloned into target
    QCOMPARE(source->property<bool>(Profile::AntiAliasFonts), target->property<bool>(Profile::AntiAliasFonts));
    QCOMPARE(source->property<int>(Profile::HistorySize), target->property<int>(Profile::HistorySize));

    // check that Name and Path properties are handled specially and not cloned
    QVERIFY(source->property<QString>(Profile::Name) != target->property<QString>(Profile::Name));
    QVERIFY(source->property<QString>(Profile::Path) != target->property<QString>(Profile::Path));

    // check that Command property is not set in target because the values
    // are the same
    QVERIFY(!target->isPropertySet(Profile::Command));
    // check that ColorScheme property is cloned because the inherited values
    // from the source parent and target parent differ
    QCOMPARE(source->property<QString>(Profile::ColorScheme), target->property<QString>(Profile::ColorScheme));
}

void ProfileTest::testProfileGroup()
{
    // create three new profiles
    std::array<Profile::Ptr, 3> profile;
    for (auto &i : profile) {
        i = new Profile;
        QVERIFY(!i->asGroup());
    }

    // set properties with different values
    profile[0]->setProperty(Profile::UseCustomCursorColor, true);
    profile[1]->setProperty(Profile::UseCustomCursorColor, false);

    // set properties with same values
    for (auto &i : profile) {
        i->setProperty(Profile::HistorySize, 1234);
    }

    // create a group profile
    ProfileGroup::Ptr group = ProfileGroup::Ptr(new ProfileGroup);
    const ProfileGroup::Ptr group_const = ProfileGroup::Ptr(new ProfileGroup);
    QVERIFY(group->asGroup());
    QVERIFY(group_const->asGroup());
    for (auto &i : profile) {
        group->addProfile(i);
        QVERIFY(group->profiles().contains(i));
        QVERIFY(!group_const->profiles().contains(i));
    }
    group->updateValues();

    // read and check properties from the group
    QCOMPARE(group->property<int>(Profile::HistorySize), 1234);
    QCOMPARE(group_const->property<int>(Profile::HistorySize), 0);
    QCOMPARE(group->property<QVariant>(Profile::UseCustomCursorColor), QVariant());
    QCOMPARE(group_const->property<QVariant>(Profile::UseCustomCursorColor), QVariant());

    // set and test shareable properties in the group
    group->setProperty(Profile::Command, QStringLiteral("ssh"));
    group->setProperty(Profile::AntiAliasFonts, false);

    QCOMPARE(profile[0]->property<QString>(Profile::Command), QStringLiteral("ssh"));
    QVERIFY(!profile[1]->property<bool>(Profile::AntiAliasFonts));

    // set and test non-shareable properties in the group
    // (should have no effect)
    group->setProperty(Profile::Name, QStringLiteral("NewName"));
    group->setProperty(Profile::Path, QStringLiteral("NewPath"));
    QVERIFY(profile[1]->property<QString>(Profile::Name) != QLatin1String("NewName"));
    QVERIFY(profile[2]->property<QString>(Profile::Path) != QLatin1String("NewPath"));

    // remove a profile from the group
    group->removeProfile(profile[0]);
    QVERIFY(!group->profiles().contains(profile[0]));
    group->updateValues();

    // check that profile is no longer affected by group
    group->setProperty(Profile::Command, QStringLiteral("fish"));
    QVERIFY(profile[0]->property<QString>(Profile::Command) != QLatin1String("fish"));
}

// Verify the correct file name is created from the UntranslatedName
void ProfileTest::testProfileFileNames()
{
    Profile::Ptr profile = Profile::Ptr(new Profile);
    QFileInfo fileInfo;
    ProfileWriter writer;

    profile->setProperty(Profile::UntranslatedName, QStringLiteral("Indiana"));
    fileInfo.setFile(writer.getPath(profile));
    QCOMPARE(fileInfo.fileName(), QStringLiteral("Indiana.profile"));

    profile->setProperty(Profile::UntranslatedName, QStringLiteral("Old Paris"));
    fileInfo.setFile(writer.getPath(profile));
    QCOMPARE(fileInfo.fileName(), QStringLiteral("Old Paris.profile"));

    /* FIXME: deal w/ file systems that are case-insensitive
       This leads to confusion as both Test and test can appear in the Manager
       Profile dialog while really there is only 1 test.profile file.
       Suggestions:  all lowercase, testing the file system, ...
    */
    /*
    profile->setProperty(Profile::UntranslatedName, "New Profile");
    fileInfo.setFile(writer.getPath(profile));
    QCOMPARE(fileInfo.fileName(), QString("new profile.profile"));
    */

    /* FIXME: don't allow certain characters in file names
       Consider: ,^@=+{}[]~!?:&*\"|#%<>$\"'();`'/\
       Suggestions: changing them all to _, just remove them, ...
       Bug 315086 comes from a user using / in the profile name - multiple
         issues there.
    */
    /*
    profile->setProperty(Profile::UntranslatedName, "new/profile");
    fileInfo.setFile(writer.getPath(profile));
    QCOMPARE(fileInfo.fileName(), QString("new_profile.profile"));
    */
}

void ProfileTest::testProfileNameSorting()
{
    auto *manager = ProfileManager::instance();

    const int origCount = manager->allProfiles().size();

    Profile::Ptr profile1 = Profile::Ptr(new Profile);
    profile1->setProperty(Profile::UntranslatedName, QStringLiteral("Indiana"));
    manager->addProfile(profile1);
    auto list = manager->allProfiles();
    int counter = 1;
    QCOMPARE(list.size(), origCount + counter++);
    // Built-in profile always at the top
    QCOMPARE(list.at(0)->name(), QStringLiteral("Built-in"));

    QVERIFY(std::is_sorted(list.cbegin(), list.cend(), profileNameLessThan));

    Profile::Ptr profile2 = Profile::Ptr(new Profile);
    profile2->setProperty(Profile::UntranslatedName, QStringLiteral("Old Paris"));
    manager->addProfile(profile2);
    list = manager->allProfiles();
    QCOMPARE(list.size(), origCount + counter++);
    QVERIFY(std::is_sorted(list.cbegin(), list.cend(), profileNameLessThan));

    Profile::Ptr profile3 = Profile::Ptr(new Profile);
    profile3->setProperty(Profile::UntranslatedName, QStringLiteral("New Zealand"));
    manager->addProfile(profile3);
    list = manager->allProfiles();
    QCOMPARE(list.size(), origCount + counter++);
    QVERIFY(std::is_sorted(list.cbegin(), list.cend(), profileNameLessThan));

    QCOMPARE(list.at(0)->name(), QStringLiteral("Built-in"));
}

void ProfileTest::testBuiltinProfile()
{
    // create a new profile
    Profile::Ptr builtin = Profile::Ptr(new Profile);
    QVERIFY(!builtin->isBuiltin());

    builtin->useBuiltin();
    QVERIFY(builtin->isBuiltin());
    QCOMPARE(builtin->untranslatedName(), QStringLiteral("Built-in"));
    QCOMPARE(builtin->path(), QStringLiteral("FALLBACK/"));
}

void ProfileTest::testLoadProfileNamedAsBuiltin()
{
    // Create a new profile data which is literally named "Built-in".
    // New code should support loading such profiles created in older versions,
    // but new profiles must not be saved with such name.
    Profile::Ptr builtin = Profile::Ptr(new Profile);
    builtin->useBuiltin();

    QString profileStr = QStringLiteral(
        "[General]\n"
        "Icon=terminator\n"
        "Name=Built-in\n"
        "Parent=FALLBACK/\n");

    QTemporaryFile file(QStringLiteral("konsole.XXXXXX.profile"));
    QVERIFY(file.open());
    QTextStream(&file) << profileStr;

    Profile::Ptr profile = Profile::Ptr(new Profile(builtin));

    ProfileReader reader;
    QString parentProfilePath;
    QVERIFY(reader.readProfile(file.fileName(), profile, parentProfilePath));

    QCOMPARE(profile->property<QString>(Profile::Icon), QStringLiteral("terminator"));
    // It's called "Built-in", but its parent is the real built-in profile
    QCOMPARE(profile->name(), builtin->name());
    QCOMPARE(parentProfilePath, builtin->path());
}

QTEST_GUILESS_MAIN(ProfileTest)

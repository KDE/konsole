/*
    Copyright 2008 by Robert Knight <robertknight@gmail.com>

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
#include "ProfileTest.h"

// KDE
#include <qtest.h>
#include <QFileInfo>

// Konsole
#include "../profile/Profile.h"
#include "../profile/ProfileGroup.h"
#include "../profile/ProfileWriter.h"

#include <array>

using namespace Konsole;

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
    QCOMPARE(source->property<bool>(Profile::AntiAliasFonts),
             target->property<bool>(Profile::AntiAliasFonts));
    QCOMPARE(source->property<int>(Profile::HistorySize),
             target->property<int>(Profile::HistorySize));

    // check that Name and Path properties are handled specially and not cloned
    QVERIFY(source->property<QString>(Profile::Name)
            != target->property<QString>(Profile::Name));
    QVERIFY(source->property<QString>(Profile::Path)
            != target->property<QString>(Profile::Path));

    // check that Command property is not set in target because the values
    // are the same
    QVERIFY(!target->isPropertySet(Profile::Command));
    // check that ColorScheme property is cloned because the inherited values
    // from the source parent and target parent differ
    QCOMPARE(source->property<QString>(Profile::ColorScheme),
             target->property<QString>(Profile::ColorScheme));
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

// Verify the correct file name is created from the untranslatedname
void ProfileTest::testProfileFileNames()
{
    Profile::Ptr profile = Profile::Ptr(new Profile);
    QFileInfo fileInfo;
    auto writer = new ProfileWriter();

    profile->setProperty(Profile::UntranslatedName, QStringLiteral("Indiana"));
    fileInfo.setFile(writer->getPath(profile));
    QCOMPARE(fileInfo.fileName(), QStringLiteral("Indiana.profile"));

    profile->setProperty(Profile::UntranslatedName, QStringLiteral("Old Paris"));
    fileInfo.setFile(writer->getPath(profile));
    QCOMPARE(fileInfo.fileName(), QStringLiteral("Old Paris.profile"));

    /* FIXME: deal w/ file systems that are case-insensitive
       This leads to confusion as both Test and test can appear in the Manager
       Profile dialog while really there is only 1 test.profile file.
       Suggestions:  all lowercase, testing the file system, ...
    */
    /*
    profile->setProperty(Profile::UntranslatedName, "New Profile");
    fileInfo.setFile(writer->getPath(profile));
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
    fileInfo.setFile(writer->getPath(profile));
    QCOMPARE(fileInfo.fileName(), QString("new_profile.profile"));
    */

    delete writer;
}

void ProfileTest::testFallbackProfile()
{
    // create a new profile
    Profile *fallback = new Profile();
    fallback->useFallback();

    QCOMPARE(fallback->property<QString>(Profile::UntranslatedName), QStringLiteral("Default"));
    QCOMPARE(fallback->property<QString>(Profile::Path), QStringLiteral("FALLBACK/"));
    delete fallback;
}

QTEST_GUILESS_MAIN(ProfileTest)

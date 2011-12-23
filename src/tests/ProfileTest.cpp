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
#include <qtest_kde.h>

// Konsole
#include "../Profile.h"

using namespace Konsole;

void ProfileTest::testProfile()
{
    // create a new profile
    Profile* parent = new Profile;
    parent->setProperty(Profile::Name, "Parent");
    parent->setProperty(Profile::Path, "FakePath");

    parent->setProperty(Profile::AntiAliasFonts, false);
    parent->setProperty(Profile::StartInCurrentSessionDir, false);

    // create a child profile
    Profile* child = new Profile(Profile::Ptr(parent));
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
    QCOMPARE(parent->property<QString>(Profile::Name), QString("Parent"));
    QCOMPARE(child->property<QVariant>(Profile::Name), QVariant());
    QCOMPARE(parent->property<QString>(Profile::Path), QString("FakePath"));
    QCOMPARE(child->property<QVariant>(Profile::Path), QVariant());

    // read inheritable properties
    QVERIFY(parent->property<bool>(Profile::AntiAliasFonts) == false);
    QVERIFY(child->property<bool>(Profile::AntiAliasFonts) == false);

    QVERIFY(parent->property<bool>(Profile::StartInCurrentSessionDir) == false);
    QVERIFY(child->property<bool>(Profile::StartInCurrentSessionDir) == true);

    delete child;
}
void ProfileTest::testClone()
{
    // create source profile and parent
    Profile::Ptr parent(new Profile);
    parent->setProperty(Profile::Command, "ps");
    parent->setProperty(Profile::ColorScheme, "BlackOnWhite");

    Profile::Ptr source(new Profile(parent));
    source->setProperty(Profile::AntiAliasFonts, false);
    source->setProperty(Profile::HistorySize, 4567);

    source->setProperty(Profile::Name, "SourceProfile");
    source->setProperty(Profile::Path, "SourcePath");

    // create target to clone source and parent
    Profile::Ptr targetParent(new Profile);
    // same value as source parent
    targetParent->setProperty(Profile::Command, "ps");
    // different value from source parent
    targetParent->setProperty(Profile::ColorScheme, "BlackOnGrey");
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
    QVERIFY(source->property<QString>(Profile::Name) !=
            target->property<QString>(Profile::Name));
    QVERIFY(source->property<QString>(Profile::Path) !=
            target->property<QString>(Profile::Path));

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
    Profile::Ptr profile[3];
    for (int i = 0; i < 3; i++) {
        profile[i] = new Profile;
        QVERIFY(!profile[i]->asGroup());
    }

    // set properties with different values
    profile[0]->setProperty(Profile::UseCustomCursorColor, true);
    profile[1]->setProperty(Profile::UseCustomCursorColor, false);

    // set properties with same values
    for (int i = 0; i < 3; i++)
        profile[i]->setProperty(Profile::HistorySize, 1234);

    // create a group profile
    ProfileGroup::Ptr group = ProfileGroup::Ptr(new ProfileGroup);
    QVERIFY(group->asGroup());
    for (int i = 0; i < 3; i++) {
        group->addProfile(profile[i]);
        QVERIFY(group->profiles().contains(profile[i]));
    }
    group->updateValues();

    // read and check properties from the group
    QCOMPARE(group->property<int>(Profile::HistorySize), 1234);
    QCOMPARE(group->property<QVariant>(Profile::UseCustomCursorColor), QVariant());

    // set and test shareable properties in the group
    group->setProperty(Profile::Command, "ssh");
    group->setProperty(Profile::AntiAliasFonts, false);

    QCOMPARE(profile[0]->property<QString>(Profile::Command), QString("ssh"));
    QVERIFY(profile[1]->property<bool>(Profile::AntiAliasFonts) == false);

    // set and test non-sharable properties in the group
    // (should have no effect)
    group->setProperty(Profile::Name, "NewName");
    group->setProperty(Profile::Path, "NewPath");
    QVERIFY(profile[1]->property<QString>(Profile::Name) != "NewName");
    QVERIFY(profile[2]->property<QString>(Profile::Path) != "NewPath");

    // remove a profile from the group
    group->removeProfile(profile[0]);
    QVERIFY(!group->profiles().contains(profile[0]));
    group->updateValues();

    // check that profile is no longer affected by group
    group->setProperty(Profile::Command, "fish");
    QVERIFY(profile[0]->property<QString>(Profile::Command) != "fish");
}

QTEST_KDEMAIN_CORE(ProfileTest)

#include "ProfileTest.moc"



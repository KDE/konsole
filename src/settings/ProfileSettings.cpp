/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>

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
#include "ProfileSettings.h"

// Qt
#include <QFileInfo>
#include <QStandardPaths>
#include <QStandardItem>
#include <QKeyEvent>

// Konsole
#include "profile/ProfileManager.h"
#include "profile/ProfileModel.h"
#include "session/Session.h"
#include "widgets/EditProfileDialog.h"
#include "terminalDisplay/TerminalDisplay.h"
#include "session/SessionManager.h"
#include "session/SessionController.h"
#include "delegates/ProfileShortcutDelegate.h"

using namespace Konsole;

ProfileSettings::ProfileSettings(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);

    profilesList->setModel(ProfileModel::instance());
    profilesList->setItemDelegateForColumn(ProfileModel::SHORTCUT, new ShortcutItemDelegate(this));

    // double clicking the profile name opens the profile edit dialog
    connect(profilesList, &QAbstractItemView::doubleClicked, this, &Konsole::ProfileSettings::doubleClicked);

    // populate the table with profiles
    populateTable();

    // setup buttons
    connect(newProfileButton, &QPushButton::clicked, this, &Konsole::ProfileSettings::createProfile);
    connect(editProfileButton, &QPushButton::clicked, this, &Konsole::ProfileSettings::editSelected);
    connect(deleteProfileButton, &QPushButton::clicked, this, &Konsole::ProfileSettings::deleteSelected);
    connect(setAsDefaultButton, &QPushButton::clicked, this, &Konsole::ProfileSettings::setSelectedAsDefault);
}

ProfileSettings::~ProfileSettings() = default;

void ProfileSettings::slotAccepted()
{
    ProfileManager::instance()->saveSettings();
    deleteLater();
}

void ProfileSettings::doubleClicked(const QModelIndex &idx)
{

    if (idx.column() == ProfileModel::NAME) {
        editSelected();
    }
}

void ProfileSettings::populateTable()
{
    QStyleOptionViewItem opt;
    opt.features = QStyleOptionViewItem::HasCheckIndicator | QStyleOptionViewItem::HasDecoration;
    auto *listHeader = profilesList->header();

    profilesList->resizeColumnToContents(ProfileModel::NAME);

    listHeader->setSectionResizeMode(ProfileModel::NAME, QHeaderView::ResizeMode::Stretch);
    listHeader->setSectionResizeMode(ProfileModel::SHORTCUT, QHeaderView::ResizeMode::ResizeToContents);
    listHeader->setStretchLastSection(false);
    listHeader->setSectionsMovable(false);

    profilesList->hideColumn(ProfileModel::PROFILE);

    // listen for changes in the table selection and update the state of the form's buttons
    // accordingly.
    //
    // it appears that the selection model is changed when the model itself is replaced,
    // so the signals need to be reconnected each time the model is updated.
    connect(profilesList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &Konsole::ProfileSettings::tableSelectionChanged);
}

void ProfileSettings::tableSelectionChanged(const QItemSelection&)
{
    const ProfileManager* manager = ProfileManager::instance();
    bool isNotDefault = true;
    bool isDeletable = true;

    const auto profiles = selectedProfiles();
    for (const auto &profile: profiles) {
        isNotDefault = isNotDefault && (profile != manager->defaultProfile());
        isDeletable = isDeletable && isProfileDeletable(profile);
    }

    newProfileButton->setEnabled(profiles.count() < 2);
    // FIXME: At some point editing 2+ profiles no longer works
    editProfileButton->setEnabled(profiles.count() == 1);
    // do not allow the default session type to be removed
    deleteProfileButton->setEnabled(isDeletable && isNotDefault && (profiles.count() > 0));
    setAsDefaultButton->setEnabled(isNotDefault && (profiles.count() == 1));
}

void ProfileSettings::deleteSelected()
{
    const QList<Profile::Ptr> profiles = selectedProfiles();
    for (const Profile::Ptr &profile : profiles) {
        if (profile != ProfileManager::instance()->defaultProfile()) {
            ProfileManager::instance()->deleteProfile(profile);
        }
    }
}

void ProfileSettings::setSelectedAsDefault()
{
    ProfileManager::instance()->setDefaultProfile(currentProfile());
    // do not allow the new default session type to be removed
    deleteProfileButton->setEnabled(false);
    setAsDefaultButton->setEnabled(false);
}

void ProfileSettings::createProfile()
{
    // setup a temporary profile which is a clone of the selected profile
    // or the default if no profile is selected
    Profile::Ptr sourceProfile = currentProfile() ? currentProfile() : ProfileManager::instance()->defaultProfile();

    Q_ASSERT(sourceProfile);

    auto newProfile = Profile::Ptr(new Profile(ProfileManager::instance()->fallbackProfile()));
    newProfile->clone(sourceProfile, true);
    // TODO: add number suffix when the name is taken
    newProfile->setProperty(Profile::Name, i18nc("@item This will be used as part of the file name", "New Profile"));
    newProfile->setProperty(Profile::UntranslatedName, QStringLiteral("New Profile"));
    newProfile->setProperty(Profile::MenuIndex, QStringLiteral("0"));

    // Consider https://blogs.kde.org/2009/03/26/how-crash-almost-every-qtkde-application-and-how-fix-it-0 before changing the below
    QPointer<EditProfileDialog> dialog = new EditProfileDialog(this);
    dialog.data()->setProfile(newProfile);
    dialog.data()->selectProfileName();

    if (dialog.data()->exec() == QDialog::Accepted) {
        ProfileManager::instance()->addProfile(newProfile);
        ProfileManager::instance()->changeProfile(newProfile, newProfile->setProperties());
    }
    delete dialog.data();
}
void ProfileSettings::editSelected()
{
    const QList<Profile::Ptr> profiles = selectedProfiles();
    EditProfileDialog *profileDialog = nullptr;
    // sessions() returns a const QList
    for (const Session *session : SessionManager::instance()->sessions()) {
        for (TerminalDisplay *terminalDisplay : session->views()) {
            // Searching for opened profiles
            profileDialog = terminalDisplay->sessionController()->profileDialogPointer();
            if (profileDialog == nullptr) {
                continue;
            }
            for (const Profile::Ptr &profile : profiles) {
                if (profile->name() == profileDialog->lookupProfile()->name()
                    && profileDialog->isVisible()) {
                    // close opened edit dialog
                    profileDialog->close();
                }
            }
        }
    }

    EditProfileDialog dialog(this);
    // the dialog will delete the profile group when it is destroyed
    ProfileGroup* group = new ProfileGroup;
    for (const Profile::Ptr &profile : profiles) {
        group->addProfile(profile);
    }
    group->updateValues();

    dialog.setProfile(Profile::Ptr(group));
    dialog.exec();
}
QList<Profile::Ptr> ProfileSettings::selectedProfiles() const
{
    QList<Profile::Ptr> list;
    QItemSelectionModel* selection = profilesList->selectionModel();
    if (selection == nullptr) {
        return list;
    }

    const QList<QModelIndex> selectedIndexes = selection->selectedIndexes();
    for (const QModelIndex &index : selectedIndexes) {
        if (index.column() == ProfileModel::PROFILE) {
            list << index.data(ProfileModel::ProfilePtrRole).value<Profile::Ptr>();
        }
    }

    return list;
}
Profile::Ptr ProfileSettings::currentProfile() const
{
    QItemSelectionModel* selection = profilesList->selectionModel();

    if ((selection == nullptr) || selection->selectedRows().count() != 1) {
        return Profile::Ptr();
    }

    return selection
        ->selectedIndexes()
        .at(ProfileModel::PROFILE)
        .data(ProfileModel::ProfilePtrRole)
        .value<Profile::Ptr>();
}

bool ProfileSettings::isProfileDeletable(Profile::Ptr profile) const
{
    if (!profile) {
        return false;
    }

    const QFileInfo fileInfo(profile->path());
    if (!fileInfo.exists()) {
        return false;
    }

    const QFileInfo dirInfo(fileInfo.path());
    return dirInfo.isWritable();
}

void ProfileSettings::setShortcutEditorVisible(bool visible)
{
    profilesList->setColumnHidden(ProfileModel::SHORTCUT, !visible);
}

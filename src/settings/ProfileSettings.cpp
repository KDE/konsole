/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
    profilesList->setSelectionMode(QAbstractItemView::SingleSelection);

    // double clicking the profile name opens the profile edit dialog
    connect(profilesList, &QAbstractItemView::doubleClicked, this, &Konsole::ProfileSettings::doubleClicked);

    // populate the table with profiles
    populateTable();

    // setup buttons
    newProfileButton->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    editProfileButton->setIcon(QIcon::fromTheme(QStringLiteral("document-edit")));
    deleteProfileButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    setAsDefaultButton->setIcon(QIcon::fromTheme(QStringLiteral("dialog-ok-apply")));

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
    const auto profile = currentProfile();
    const bool isNotDefault = profile != ProfileManager::instance()->defaultProfile();

    newProfileButton->setEnabled(true);

    // See comment about isProfileWritable(profile) in editSelected()
    editProfileButton->setEnabled(isProfileWritable(profile));

    // Do not allow the current default profile of the session to be removed
    deleteProfileButton->setEnabled(isNotDefault && isProfileDeletable(profile));

    setAsDefaultButton->setEnabled(isNotDefault);
}

void ProfileSettings::deleteSelected()
{
    const auto profile = currentProfile();

    // The "Delete" button is disabled for the current default profile
    Q_ASSERT(profile != ProfileManager::instance()->defaultProfile());

    ProfileManager::instance()->deleteProfile(profile);
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
    const auto profile = currentProfile();

    // Read-only profiles, i.e. oens with .profile's that aren't writable
    // for the user aren't editable, only clone-able by using the "New"
    // button, this includes the Default/fallback profile, which is hardcoded.
    if (!isProfileWritable(profile)) {
        return;
    }

    EditProfileDialog *profileDialog = nullptr;
    const auto sessionsList = SessionManager::instance()->sessions();
    for (const Session *session : sessionsList) {
        for (TerminalDisplay *terminalDisplay : session->views()) {
            // Searching for already open EditProfileDialog instances
            // for this profile
            profileDialog = terminalDisplay->sessionController()->profileDialogPointer();
            if (profileDialog == nullptr) {
                continue;
            }

            if (profile->name() == profileDialog->lookupProfile()->name()
                && profileDialog->isVisible()) {
                // close opened edit dialog
                profileDialog->close();
            }
        }
    }

    EditProfileDialog dialog(this);

    dialog.setProfile(profile);
    dialog.exec();
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
    if (!profile || profile->isFallback()) {
        return false;
    }

    const QFileInfo fileInfo(profile->path());
    return fileInfo.exists()
        && QFileInfo(fileInfo.path()).isWritable(); // To delete a file, parent dir must be writable
}

bool ProfileSettings::isProfileWritable(Profile::Ptr profile) const
{
    return profile
        && !profile->isFallback() // Default/fallback profile is hardcoded
        && QFileInfo(profile->path()).isWritable();
}

void ProfileSettings::setShortcutEditorVisible(bool visible)
{
    profilesList->setColumnHidden(ProfileModel::SHORTCUT, !visible);
}

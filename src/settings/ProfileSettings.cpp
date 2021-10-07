/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "ProfileSettings.h"

// Qt
#include <QFileInfo>
#include <QKeyEvent>
#include <QStandardItem>
#include <QStandardPaths>

// Konsole
#include "delegates/ProfileShortcutDelegate.h"
#include "profile/ProfileManager.h"
#include "profile/ProfileModel.h"
#include "session/Session.h"
#include "session/SessionController.h"
#include "session/SessionManager.h"
#include "terminalDisplay/TerminalDisplay.h"
#include "widgets/EditProfileDialog.h"

using namespace Konsole;

ProfileSettings::ProfileSettings(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);

    profileListView->setModel(ProfileModel::instance());
    profileListView->setItemDelegateForColumn(ProfileModel::SHORTCUT, new ShortcutItemDelegate(this));
    profileListView->setSelectionMode(QAbstractItemView::SingleSelection);

    // double clicking the profile name opens the profile edit dialog
    connect(profileListView, &QAbstractItemView::doubleClicked, this, &Konsole::ProfileSettings::doubleClicked);

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
    auto *listHeader = profileListView->header();

    profileListView->resizeColumnToContents(ProfileModel::NAME);

    listHeader->setSectionResizeMode(ProfileModel::NAME, QHeaderView::ResizeMode::Stretch);
    listHeader->setSectionResizeMode(ProfileModel::SHORTCUT, QHeaderView::ResizeMode::ResizeToContents);
    listHeader->setStretchLastSection(false);
    listHeader->setSectionsMovable(false);

    profileListView->hideColumn(ProfileModel::PROFILE);

    // listen for changes in the table selection and update the state of the form's buttons
    // accordingly.
    //
    // it appears that the selection model is changed when the model itself is replaced,
    // so the signals need to be reconnected each time the model is updated.
    connect(profileListView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &Konsole::ProfileSettings::tableSelectionChanged);
}

void ProfileSettings::tableSelectionChanged(const QItemSelection &selected)
{
    newProfileButton->setEnabled(true);

    if (selected.isEmpty()) {
        editProfileButton->setEnabled(false);
        deleteProfileButton->setEnabled(false);
        setAsDefaultButton->setEnabled(false);
        return;
    }

    const auto profile = currentProfile();
    const bool isNotDefault = profile != ProfileManager::instance()->defaultProfile();

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
    auto newProfile = Profile::Ptr(new Profile(ProfileManager::instance()->fallbackProfile()));

    // If a profile is selected, clone its properties, otherwise the
    // the fallback profile properties will be used
    if (currentProfile()) {
        newProfile->clone(currentProfile(), true);
    }

    const QString uniqueName = ProfileManager::instance()->generateUniqueName();
    newProfile->setProperty(Profile::Name, uniqueName);
    newProfile->setProperty(Profile::UntranslatedName, uniqueName);

    auto *dialog = new EditProfileDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(true);
    dialog->setProfile(newProfile, EditProfileDialog::NewProfile);
    dialog->selectProfileName();

    dialog->show();
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

    auto *dialog = new EditProfileDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(true);
    dialog->setProfile(profile);
    dialog->show();
}

Profile::Ptr ProfileSettings::currentProfile() const
{
    QItemSelectionModel *selection = profileListView->selectionModel();

    if ((selection == nullptr) || !selection->hasSelection()) {
        return Profile::Ptr();
    }

    return selection->selectedIndexes().at(ProfileModel::PROFILE).data(ProfileModel::ProfilePtrRole).value<Profile::Ptr>();
}

bool ProfileSettings::isProfileDeletable(Profile::Ptr profile) const
{
    if (!profile || profile->isFallback()) {
        return false;
    }

    const QFileInfo fileInfo(profile->path());
    return fileInfo.exists() && QFileInfo(fileInfo.path()).isWritable(); // To delete a file, parent dir must be writable
}

bool ProfileSettings::isProfileWritable(Profile::Ptr profile) const
{
    return profile && !profile->isFallback() // Default/fallback profile is hardcoded
        && QFileInfo(profile->path()).isWritable();
}

void ProfileSettings::setShortcutEditorVisible(bool visible)
{
    profileListView->setColumnHidden(ProfileModel::SHORTCUT, !visible);
}

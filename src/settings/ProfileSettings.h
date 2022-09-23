/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PROFILESETTINGS_H
#define PROFILESETTINGS_H

// Qt
#include <QExplicitlySharedDataPointer>

// KDE

// Konsole
// TODO: Move this file to the profile folder?
#include "ui_ProfileSettings.h"

class QItemSelection;
class QStandardItem;
class QStandardItemModel;

namespace Konsole
{
class Profile;

/**
 * A dialog which lists the available types of profiles and allows
 * the user to add new profiles, and remove or edit existing
 * profile types.
 */
class ProfileSettings : public QWidget, private Ui::ProfileSettings
{
    Q_OBJECT

    friend class ShortcutItemDelegate;

public:
    /** Constructs a new profile type with the specified parent. */
    explicit ProfileSettings(QWidget *parent = nullptr);
    ~ProfileSettings() override;

private Q_SLOTS:
    friend class MainWindow;
    void slotAccepted();

    void deleteSelected();
    void setSelectedAsDefault();
    void createProfile();
    void editSelected();

    // enables or disables Edit/Delete/Set as Default buttons when the
    // selection changes
    void tableSelectionChanged(const QItemSelection &);

    // double clicking the profile name opens the edit profile dialog
    void doubleClicked(const QModelIndex &index);

private:
    QExplicitlySharedDataPointer<Profile> currentProfile() const;

    // updates the profile table to be in sync with the
    // session manager
    void populateTable();
};

}
#endif // MANAGEPROFILESDIALOG_H

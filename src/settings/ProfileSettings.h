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

#ifndef PROFILESETTINGS_H
#define PROFILESETTINGS_H

// Qt
#include <QStyledItemDelegate>
#include <QSet>
#include <QKeySequenceEdit>
#include <QExplicitlySharedDataPointer>

// KDE

// Konsole
// TODO: Move this file to the profile folder?
#include "ui_ProfileSettings.h"

class QItemSelection;
class QStandardItem;
class QStandardItemModel;

namespace Konsole {
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

    /**
     * Specifies whether the shortcut editor should be show.
     * The shortcut editor allows shortcuts to be associated with
     * profiles.  When a shortcut is changed, the dialog will call
     * SessionManager::instance()->setShortcut() to update the shortcut
     * associated with the profile.
     *
     * By default the editor is visible.
     */
    void setShortcutEditorVisible(bool visible);

protected:

private Q_SLOTS:
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
    QList<QExplicitlySharedDataPointer<Profile>> selectedProfiles() const;
    bool isProfileDeletable(QExplicitlySharedDataPointer<Profile> profile) const;

    // updates the profile table to be in sync with the
    // session manager
    void populateTable();
};

}
#endif // MANAGEPROFILESDIALOG_H

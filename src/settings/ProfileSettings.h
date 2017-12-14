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

// KDE

// Konsole
#include "Profile.h"
#include "ui_ProfileSettings.h"

class QItemSelection;
class QShowEvent;
class QStandardItem;
class QStandardItemModel;

namespace Konsole {
/**
 * A dialog which lists the available types of profiles and allows
 * the user to add new profiles, and remove or edit existing
 * profile types.
 */
class ProfileSettings : public QWidget, private Ui::ProfileSettings
{
    Q_OBJECT

    friend class FavoriteItemDelegate;
    friend class ShortcutItemDelegate;

public:
    /** Constructs a new profile type with the specified parent. */
    explicit ProfileSettings(QWidget *parent = nullptr);
    ~ProfileSettings() Q_DECL_OVERRIDE;

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
    void moveUpSelected();
    void moveDownSelected();

    void itemDataChanged(QStandardItem *item);

    // enables or disables Edit/Delete/Set as Default buttons when the
    // selection changes
    void tableSelectionChanged(const QItemSelection &);

    void updateFavoriteStatus(Profile::Ptr profile, bool favorite);

    void addItems(const Profile::Ptr);
    void updateItems(const Profile::Ptr);
    void removeItems(const Profile::Ptr);

    // double clicking the profile name opens the edit profile dialog
    void doubleClicked(const QModelIndex &index);

private:
    Profile::Ptr currentProfile() const;
    QList<Profile::Ptr> selectedProfiles() const;
    bool isProfileDeletable(Profile::Ptr profile) const;

    // updates the font of the items to match
    // their default / non-default profile status
    void updateDefaultItem();
    void updateItemsForProfile(const Profile::Ptr profile, QList<QStandardItem *> &items) const;
    // updates the profile table to be in sync with the
    // session manager
    void populateTable();
    int rowForProfile(const Profile::Ptr profile) const;

    QStandardItemModel *_sessionModel;

    static const int ProfileNameColumn = 0;
    static const int FavoriteStatusColumn = 1;
    static const int ShortcutColumn = 2;
    static const int ProfileKeyRole = Qt::UserRole + 1;
    static const int ShortcutRole = Qt::UserRole + 1;
};

class StyledBackgroundPainter
{
public:
    static void drawBackground(QPainter *painter, const QStyleOptionViewItem &option,
                               const QModelIndex &index);
};

class FavoriteItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit FavoriteItemDelegate(QObject *parent = nullptr);

    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                     const QModelIndex &index) Q_DECL_OVERRIDE;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const Q_DECL_OVERRIDE;
};

class ShortcutItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ShortcutItemDelegate(QObject *parent = nullptr);

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const Q_DECL_OVERRIDE;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const Q_DECL_OVERRIDE;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const Q_DECL_OVERRIDE;

private Q_SLOTS:
    void editorModified(const QKeySequence &keys);

private:
    mutable QSet<QWidget *> _modifiedEditors;
    mutable QSet<QModelIndex> _itemsBeingEdited;
};
}
#endif // MANAGEPROFILESDIALOG_H

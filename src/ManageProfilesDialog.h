/*
    Copyright (C) 2007 by Robert Knight <robertknight@gmail.com>

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

#ifndef MANAGEPROFILESDIALOG_H
#define MANAGEPROFILESDIALOG_H

// Qt
#include <QtGui/QItemDelegate>

// KDE
#include <KDialog>

class QItemSelection;
class QShowEvent;
class QStandardItem;
class QStandardItemModel;

namespace Ui
{
    class ManageProfilesDialog;
}

namespace Konsole
{

/**
 * A dialog which lists the available types of profiles and allows
 * the user to add new profiles, and remove or edit existing
 * profile types.
 */
class ManageProfilesDialog : public KDialog
{
Q_OBJECT

friend class ProfileItemDelegate;

public:
    /** Constructs a new profile type with the specified parent. */
    ManageProfilesDialog(QWidget* parent = 0);
    virtual ~ManageProfilesDialog();

	/** 
	 * Specifies whether the shorcut editor should be show.
	 * The shortcut editor allows shortcuts to be associated with 
	 * profiles.  When a shortcut is changed, the dialog will call
	 * SessionManager::instance()->setShortcut() to update the shortcut
	 * associated with the profile.
	 *
	 * By default the editor is visible.
	 */
	void setShortcutEditorVisible(bool visible);

protected:
    virtual void showEvent(QShowEvent* event);

private slots:
    void deleteSelected();
    void setSelectedAsDefault();
    void newType();
    void editSelected();

    void itemDataChanged(QStandardItem* item);

    // enables or disables Edit/Delete/Set as Default buttons when the 
    // selection changes
    void tableSelectionChanged(const QItemSelection&);

    // updates the profile table to be in sync with the 
    // session manager
    void updateTableModel();

    void updateFavoriteStatus(const QString& key , bool favorite);

private:
    QString selectedKey() const; // return the key associated with the currently selected
                                 // item in the profile table

    void updateDefaultItem(); // updates the font of the items to match
                              // their default / non-default profile status
    Ui::ManageProfilesDialog* _ui;
    QStandardItemModel* _sessionModel;

    static const int FavoriteStatusColumn = 1;
    static const int ShortcutColumn = 2;
    static const int ProfileKeyRole = Qt::UserRole + 1;
    static const int ShortcutRole = Qt::UserRole + 1;
};

class ProfileItemDelegate : public QItemDelegate
{
public:
    ProfileItemDelegate(QObject* parent = 0);

    virtual bool editorEvent(QEvent* event,QAbstractItemModel* model,
                             const QStyleOptionViewItem& option,const QModelIndex& index);

protected:
    virtual void drawDecoration(QPainter*,const QStyleOptionViewItem&,const QRect&,
                                const QPixmap&) const;

};

}
#endif // MANAGEPROFILESDIALOG_H


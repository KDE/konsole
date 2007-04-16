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

#ifndef SESSIONTYPEDIALOG_H
#define SESSIONTYPEDIALOG_H

// Qt
#include <QItemDelegate>

// KDE
#include <KDialog>

class QItemSelection;
class QStandardItem;
class QStandardItemModel;

namespace Ui
{
    class SessionTypeDialog;
};

namespace Konsole
{

/**
 * A dialog which lists the available types of sessions and allows
 * the user to add new sessions, and remove or edit existing
 * session types.
 */
class SessionTypeDialog : public KDialog
{
Q_OBJECT

public:
    /** Constructs a new session type with the specified parent. */
    SessionTypeDialog(QWidget* parent = 0);
    virtual ~SessionTypeDialog();

private slots:
    void deleteSelected();
    void setSelectedAsDefault();
    void newType();
    void editSelected();

    // enables or disables Edit/Delete/Set as Default buttons when the 
    // selection changes
    void tableSelectionChanged(const QItemSelection&);

    // updates the session table to be in sync with the 
    // session manager
    void updateTableModel();

private:
    QString selectedKey() const; // return the key associated with the currently selected
                                 // item in the session table
    Ui::SessionTypeDialog* _ui;
    QStandardItemModel* _sessionModel;
};

class SessionViewDelegate : public QItemDelegate
{
public:
    SessionViewDelegate(QObject* parent = 0);

    virtual bool editorEvent(QEvent* event,QAbstractItemModel* model,
                             const QStyleOptionViewItem& option,const QModelIndex& index);
};

};
#endif // SESSIONTYPEDIALOG_H


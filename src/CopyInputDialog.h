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

#ifndef COPYINPUTDIALOG_H
#define COPYINPUTDIALOG_H

// Qt
#include <QPointer>
#include <QSet>

// KDE
#include <QDialog>

// Konsole
#include "SessionManager.h"
#include "Session.h"
#include "SessionListModel.h"

namespace Ui {
class CopyInputDialog;
}

namespace Konsole {
class CheckableSessionModel;

/**
 * Dialog which allows the user to mark a list of sessions to copy
 * the input from the current session to.  The current session is
 * set using setMasterSession().  After the dialog has been executed,
 * the set of chosen sessions can be retrieved using chosenSessions()
 */
class CopyInputDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CopyInputDialog(QWidget *parent = nullptr);
    ~CopyInputDialog() Q_DECL_OVERRIDE;
    /**
     * Sets the 'source' session whose input will be copied to
     * other sessions.  This session is displayed grayed out in the list
     * and cannot be unchecked.
     */
    void setMasterSession(Session *session);
    /** See setMasterSession() */
    Session *masterSession() const;

    /** Sets the sessions in the list which are checked. */
    void setChosenSessions(const QSet<Session *> &sessions);
    /** Set setChosenSessions() */
    QSet<Session *> chosenSessions() const;

private Q_SLOTS:
    void selectAll()
    {
        setSelectionChecked(true);
    }

    void deselectAll()
    {
        setSelectionChecked(false);
    }

private:
    Q_DISABLE_COPY(CopyInputDialog)

    // Checks or unchecks selected sessions.  If there are no
    // selected items then all sessions are checked or unchecked
    void setSelectionChecked(bool checked);
    void setRowChecked(int row, bool checked);

    Ui::CopyInputDialog *_ui;
    CheckableSessionModel *_model;
    QPointer<Session> _masterSession;
};

/**
 * A list of sessions with a checkbox next to each one which allows the
 * user to select a subset of the available sessions to perform
 * some action on them.
 */
class CheckableSessionModel : public SessionListModel
{
    Q_OBJECT

public:
    explicit CheckableSessionModel(QObject *parent);

    void setCheckColumn(int column);
    int checkColumn() const;

    /**
     * Sets whether a session can be checked or un-checked.
     * Non-checkable items have the Qt::ItemIsEnabled flag unset.
     */
    void setCheckable(Session *session, bool checkable);

    /** Sets the list of sessions which are currently checked. */
    void setCheckedSessions(const QSet<Session *> &sessions);
    /** Returns the set of checked sessions. */
    QSet<Session *> checkedSessions() const;

    // reimplemented from QAbstractItemModel
    Qt::ItemFlags flags(const QModelIndex &index) const Q_DECL_OVERRIDE;
    QVariant data(const QModelIndex &index, int role) const Q_DECL_OVERRIDE;
    bool setData(const QModelIndex &index, const QVariant &value, int role) Q_DECL_OVERRIDE;

protected:
    void sessionRemoved(Session *) Q_DECL_OVERRIDE;

private:
    QSet<Session *> _checkedSessions;
    QSet<Session *> _fixedSessions;
    int _checkColumn;
};

inline int CheckableSessionModel::checkColumn() const
{
    return _checkColumn;
}
}

#endif // COPYINPUTDIALOG_H

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
#include "session/SessionManager.h"
#include "session/Session.h"
#include "session/SessionListModel.h"

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
    ~CopyInputDialog() override;
    /**
     * Sets the 'source' session whose input will be copied to
     * other sessions.  This session is displayed grayed out in the list
     * and cannot be unchecked.
     */
    void setMasterSession(Session *session);

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

}

#endif // COPYINPUTDIALOG_H

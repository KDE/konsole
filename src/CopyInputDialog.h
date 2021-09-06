/*
    SPDX-FileCopyrightText: 2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COPYINPUTDIALOG_H
#define COPYINPUTDIALOG_H

// Qt
#include <QPointer>
#include <QSet>

// KDE
#include <QDialog>

// Konsole
#include "session/Session.h"
#include "session/SessionListModel.h"
#include "session/SessionManager.h"

namespace Ui
{
class CopyInputDialog;
}

namespace Konsole
{
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

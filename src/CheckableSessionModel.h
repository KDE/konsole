/*
    SPDX-FileCopyrightText: 2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CHECKABLESESSIONMODEL_H
#define CHECKABLESESSIONMODEL_H

// Qt
#include <QPointer>
#include <QSet>

// Konsole
#include "session/Session.h"
#include "session/SessionListModel.h"

namespace Konsole
{
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
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

protected:
    void sessionRemoved(Session *) override;

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

#endif

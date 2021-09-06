/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2009 Thomas Dreibholz <dreibh@iem.uni-due.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SEARCHHISTORYTASK_H
#define SEARCHHISTORYTASK_H

#include <QMap>
#include <QPointer>
#include <QRegularExpression>

#include "Enumeration.h"
#include "ScreenWindow.h"
#include "session/Session.h"
#include "session/SessionTask.h"

namespace Konsole
{
// class SearchHistoryThread;
/**
 * A task which searches through the output of sessions for matches for a given regular expression.
 * SearchHistoryTask operates on ScreenWindow instances rather than sessions added by addSession().
 * A screen window can be added to the list to search using addScreenWindow()
 *
 * When execute() is called, the search begins in the direction specified by searchDirection(),
 * starting at the position of the current selection.
 *
 * FIXME - This is not a proper implementation of SessionTask, in that it ignores sessions specified
 * with addSession()
 *
 * TODO - Implementation requirements:
 *          May provide progress feedback to the user when searching very large output logs.
 */
class SearchHistoryTask : public SessionTask
{
    Q_OBJECT

public:
    /**
     * Constructs a new search task.
     */
    explicit SearchHistoryTask(QObject *parent = nullptr);

    /** Adds a screen window to the list to search when execute() is called. */
    void addScreenWindow(Session *session, ScreenWindow *searchWindow);

    /** Sets the regular expression which is searched for when execute() is called */
    void setRegExp(const QRegularExpression &expression);
    /** Returns the regular expression which is searched for when execute() is called */
    QRegularExpression regExp() const;

    /** Specifies the direction to search in when execute() is called. */
    void setSearchDirection(Enum::SearchDirection direction);
    /** Returns the current search direction.  See setSearchDirection(). */
    Enum::SearchDirection searchDirection() const;

    /** The line from which the search will be done **/
    void setStartLine(int line);

    /**
     * Performs a search through the session's history, starting at the position
     * of the current selection, in the direction specified by setSearchDirection().
     *
     * If it finds a match, the ScreenWindow specified in the constructor is
     * scrolled to the position where the match occurred and the selection
     * is set to the matching text.  execute() then returns immediately.
     *
     * To continue the search looking for further matches, call execute() again.
     */
    void execute() override;

private:
    using ScreenWindowPtr = QPointer<ScreenWindow>;

    void executeOnScreenWindow(const QPointer<Session> &session, const ScreenWindowPtr &window);
    void highlightResult(const ScreenWindowPtr &window, int findPos);

    QMap<QPointer<Session>, ScreenWindowPtr> _windows;
    QRegularExpression _regExp;
    Enum::SearchDirection _direction;
    int _startLine;
};

}
#endif

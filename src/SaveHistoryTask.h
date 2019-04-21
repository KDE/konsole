/*
    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 2009 by Thomas Dreibholz <dreibh@iem.uni-due.de>

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

#ifndef SAVEHISTORYTASK_H
#define SAVEHISTORYTASK_H

#include "SessionTask.h"
#include "TerminalCharacterDecoder.h"

#include <kio/job.h>

namespace Konsole
{

/**
 * A task which prompts for a URL for each session and saves that session's output
 * to the given URL
 */
class SaveHistoryTask : public SessionTask
{
    Q_OBJECT

public:
    /** Constructs a new task to save session output to URLs */
    explicit SaveHistoryTask(QObject *parent = nullptr);
    ~SaveHistoryTask() Q_DECL_OVERRIDE;

    /**
     * Opens a save file dialog for each session in the group and begins saving
     * each session's history to the given URL.
     *
     * The data transfer is performed asynchronously and will continue after execute() returns.
     */
    void execute() Q_DECL_OVERRIDE;

private Q_SLOTS:
    void jobDataRequested(KIO::Job *job, QByteArray &data);
    void jobResult(KJob *job);

private:
    class SaveJob // structure to keep information needed to process
        // incoming data requests from jobs
    {
    public:
	QPointer<Session> session; // the session associated with a history save job
	int lastLineFetched; // the last line processed in the previous data request
	// set this to -1 at the start of the save job

	TerminalCharacterDecoder *decoder;  // decoder used to convert terminal characters
	// into output
    };

    QHash<KJob *, SaveJob> _jobSession;

    static QString _saveDialogRecentURL;
};

}

#endif

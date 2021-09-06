/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2009 Thomas Dreibholz <dreibh@iem.uni-due.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SAVEHISTORYTASK_H
#define SAVEHISTORYTASK_H

#include "session/SessionTask.h"

#include <kio/job.h>

namespace Konsole
{
class TerminalCharacterDecoder;

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
    ~SaveHistoryTask() override;

    /**
     * Opens a save file dialog for each session in the group and begins saving
     * each session's history to the given URL.
     *
     * The data transfer is performed asynchronously and will continue after execute() returns.
     */
    void execute() override;

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

        TerminalCharacterDecoder *decoder; // decoder used to convert terminal characters
        // into output
    };

    QHash<KJob *, SaveJob> _jobSession;

    static QString _saveDialogRecentURL;
};

}

#endif

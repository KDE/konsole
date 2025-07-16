/*
    SPDX-FileCopyrightText: 2024 Theodore Wang <theodorewang12@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SAVEHISTORYAUTOTASK_H
#define SAVEHISTORYAUTOTASK_H

#include <QFile>
#include <QFileSystemWatcher>
#include <QTimer>

#include "../decoders/PlainTextDecoder.h"
#include "konsoleprivate_export.h"
#include "session/SessionTask.h"

namespace Konsole
{
/**
 * A task which prompts for a URL for each session and saves that session's output
 * to the given URL
 */
class KONSOLEPRIVATE_EXPORT SaveHistoryAutoTask : public SessionTask
{
    Q_OBJECT

public:
    /** Constructs a new task to save session output to URLs */
    explicit SaveHistoryAutoTask(QObject *parent = nullptr);
    ~SaveHistoryAutoTask() override;

    /**
     * Opens a save file dialog for each session in the group and begins saving
     * each session's history to the given URL.
     *
     * The data transfer is performed asynchronously and will continue after execute() returns.
     */
    bool execute() override;

public Q_SLOTS:
    // Stops the autosave process.
    void stop();

private Q_SLOTS:
    /**
     * Increases _droppedBytes using info in _bytesLines when lines
     * have been dropped from the screen and history.
     */
    void linesDropped(int linesDropped);

    /**
     * Resets _bytesLines since the lines on the screen changes when
     * the screen is resized.
     */
    void imageResized(int rows, int columns);

    // Called when Emulation::outputChanged() is emitted.
    void linesChanged();

    /**
     * If the QFileSystemWatcher::fileChanged() signal is emitted,
     * this is called which terminates the autosave process with an error.
     */
    void fileModified();

private:
    // Reads the session output.
    void readLines();

    bool updateArchive();

    // Updates _bytesLines.
    void updateByteLineAnchors();

    const QPointer<Session> &session() const;

    /**
     * The number of milliseconds to wait after an autosave before
     * checking if another is needed.
     */
    int timerInterval() const;

    // File object used to store the autosaved contents.
    QFile _destinationFile;

    /**
     * Since the autosave process relies on the files not being modified
     * externally, this keeps watch on the autosave destination file.
     */
    QFileSystemWatcher _watcher;

    // Number of bytes used to accommodate dropped content in the autosave file.
    qint64 _droppedBytes;

    /**
     * A list of byte offsets in _destinationFile.
     * Each offset corresponds to the first of a series of bytes
     * containing content of a line on the emulation's current screen and history.
     */
    QList<qint64> _bytesLines;

    PlainTextDecoder _decoder;

    // Used to time how often the output should be re-read.
    QTimer _timer;

    /**
     * Determines whether the output is re-read at the end
     * of the current timer internal.
     */
    bool _pendingChanges;

    static QString _saveDialogRecentURL;
};

}

#endif
